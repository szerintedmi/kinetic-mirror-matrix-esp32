#pragma once
#include <stdint.h>
#include <memory>
#include <string>
#include "MotorControl/MotorController.h"
#include "MotorControl/command/CommandBatchExecutor.h"
#include "MotorControl/command/CommandExecutionContext.h"
#include "MotorControl/command/CommandParser.h"
#include "MotorControl/command/CommandRouter.h"
#include "MotorControl/command/CommandResult.h"

class MotorCommandProcessor {
public:
  MotorCommandProcessor();
  ~MotorCommandProcessor();
  MotorCommandProcessor(const MotorCommandProcessor&) = delete;
  MotorCommandProcessor& operator=(const MotorCommandProcessor&) = delete;
  MotorCommandProcessor(MotorCommandProcessor&&) noexcept = default;
  MotorCommandProcessor& operator=(MotorCommandProcessor&&) noexcept = default;
  std::string processLine(const std::string& line, uint32_t now_ms);
  void tick(uint32_t now_ms) { controller_->tick(now_ms); }
  motor::command::CommandResult execute(const std::string &line, uint32_t now_ms);
  MotorController &controller() { return *controller_; }
  const MotorController &controller() const { return *controller_; }

private:
  std::unique_ptr<MotorController> controller_;
  bool thermal_limits_enabled_ = true;
  int default_speed_sps_;
  int default_accel_sps2_;
  int default_decel_sps2_;
  bool in_batch_ = false;
  bool batch_initially_idle_ = false;

  motor::command::CommandParser parser_;
  std::unique_ptr<motor::command::CommandRouter> router_;
  motor::command::CommandBatchExecutor batch_executor_;

  motor::command::CommandExecutionContext makeContext();
  motor::command::CommandResult dispatchSingle(const motor::command::ParsedCommand &command,
                                               motor::command::CommandExecutionContext &context,
                                               uint32_t now_ms);
};
