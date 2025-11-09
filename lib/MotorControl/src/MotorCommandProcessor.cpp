#include "MotorControl/MotorCommandProcessor.h"

#include "MotorControl/MotorControlConstants.h"
#include "MotorControl/command/CommandHandlers.h"
#include "MotorControl/command/CommandResult.h"
#include "MotorControl/command/ResponseFormatter.h"
#include "StubMotorController.h"
#include "transport/CompletionTracker.h"
#if !defined(USE_STUB_BACKEND) && !defined(UNIT_TEST)
#include "MotorControl/HardwareMotorController.h"
#endif

#include <memory>
#include <vector>

using motor::command::CommandBatchExecutor;
using motor::command::CommandExecutionContext;
using motor::command::CommandHandler;
using motor::command::CommandParser;
using motor::command::CommandResult;
using motor::command::CommandRouter;
using motor::command::ParsedCommand;

MotorCommandProcessor::MotorCommandProcessor()
#if !defined(USE_STUB_BACKEND) && !defined(UNIT_TEST)
    : controller_(new HardwareMotorController())
#else
    : controller_(new StubMotorController(8))
#endif
{
  controller_->setThermalLimitsEnabled(thermal_limits_enabled_);
  default_speed_sps_ = MotorControlConstants::DEFAULT_SPEED_SPS;
  default_accel_sps2_ = MotorControlConstants::DEFAULT_ACCEL_SPS2;
  default_decel_sps2_ = 0;
  controller_->setDeceleration(default_decel_sps2_);

  std::vector<std::unique_ptr<CommandHandler>> handlers;
  handlers.emplace_back(std::unique_ptr<CommandHandler>(new motor::command::MotorCommandHandler()));
  handlers.emplace_back(std::unique_ptr<CommandHandler>(new motor::command::QueryCommandHandler()));
  handlers.emplace_back(std::unique_ptr<CommandHandler>(new motor::command::NetCommandHandler()));
  handlers.emplace_back(
      std::unique_ptr<CommandHandler>(new motor::command::MqttConfigCommandHandler()));
  router_.reset(new CommandRouter(std::move(handlers)));
}

MotorCommandProcessor::~MotorCommandProcessor() {
  transport::response::CompletionTracker::Instance().RemoveController(controller_.get());
}

CommandResult MotorCommandProcessor::execute(const std::string& line, uint32_t now_ms) {
  auto commands = parser_.parse(line);
  if (commands.empty()) {
    return CommandResult();
  }

  CommandExecutionContext context = makeContext();
  if (commands.size() == 1) {
    context.setBatchState(false, false);
    return dispatchSingle(commands[0], context, now_ms);
  }
  return batch_executor_.execute(commands, context, *router_, now_ms);
}

std::string MotorCommandProcessor::processLine(const std::string& line, uint32_t now_ms) {
  CommandResult result = execute(line, now_ms);
  return motor::command::FormatForSerial(result);
}

CommandExecutionContext MotorCommandProcessor::makeContext() {
  return CommandExecutionContext(*controller_,
                                 thermal_limits_enabled_,
                                 default_speed_sps_,
                                 default_accel_sps2_,
                                 default_decel_sps2_,
                                 in_batch_,
                                 batch_initially_idle_);
}

CommandResult MotorCommandProcessor::dispatchSingle(const ParsedCommand& command,
                                                    CommandExecutionContext& context,
                                                    uint32_t now_ms) {
  return router_->dispatch(command, context, now_ms);
}
