#include "MotorControl/command/CommandBatchExecutor.h"

#include "MotorControl/command/CommandUtils.h"
#include "transport/CommandSchema.h"
#include "transport/ResponseDispatcher.h"
#include "transport/ResponseModel.h"

namespace motor {
namespace command {

namespace {

void EmitLineEvent(const transport::command::ResponseLine& line,
                   const std::string& action = std::string()) {
  transport::response::ResponseDispatcher::Instance().Emit(
      transport::response::BuildEvent(line, action));
}

CommandResult MakeErrorResult(const transport::command::ResponseLine& line,
                              const std::string& action = std::string()) {
  EmitLineEvent(line, action);
  CommandResult res;
  res.is_error = true;
  res.append(line);
  return res;
}

bool ExtractUintField(const transport::command::ResponseLine& line,
                      const std::string& key,
                      uint32_t& out_value) {
  for (const auto& field : line.fields) {
    if (field.key == key) {
      try {
        out_value = static_cast<uint32_t>(std::stoul(field.value));
        return true;
      } catch (...) {
        return false;
      }
    }
  }
  return false;
}

}  // namespace

CommandResult CommandBatchExecutor::execute(const std::vector<ParsedCommand>& commands,
                                            CommandExecutionContext& context,
                                            CommandRouter& router,
                                            uint32_t now_ms) {
  // Overlap detection
  uint32_t seen = 0;
  for (const auto& cmd : commands) {
    uint32_t mask = maskFor(cmd, context);
    if (mask != 0 && (mask & seen)) {
      auto line = transport::command::MakeErrorLine(
          context.nextMsgId(), "E03", "BAD_PARAM MULTI_CMD_CONFLICT", {});
      return MakeErrorResult(line);
    }
    seen |= mask;
  }

  // Unknown action detection
  for (const auto& cmd : commands) {
    if (!router.knowsAction(cmd.action)) {
      auto line = transport::command::MakeErrorLine(context.nextMsgId(), "E01", "BAD_CMD", {});
      return MakeErrorResult(line, cmd.action);
    }
  }

  bool initially_idle = true;
  for (uint8_t i = 0; i < context.controller().motorCount(); ++i) {
    if (context.controller().state(i).moving) {
      initially_idle = false;
      break;
    }
  }

  bool prev_in_batch = context.inBatch();
  bool prev_initially_idle = context.batchInitiallyIdle();
  context.setBatchState(true, initially_idle);

  transport::command::Response aggregate;
  uint32_t agg_est_ms = 0;
  bool saw_est = false;

  for (const auto& cmd : commands) {
    CommandResult result = router.dispatch(cmd, context, now_ms);
    if (result.is_error) {
      context.setBatchState(prev_in_batch, prev_initially_idle);
      return result;
    }
    if (!result.hasStructuredResponse()) {
      continue;
    }
    const auto& response = result.structuredResponse();
    for (const auto& line : response.lines) {
      uint32_t value = 0;
      if (line.type == transport::command::ResponseLineType::kAck &&
          ExtractUintField(line, "est_ms", value)) {
        if (value > agg_est_ms) {
          agg_est_ms = value;
        }
        saw_est = true;
        continue;
      }
      aggregate.lines.push_back(line);
    }
  }

  context.setBatchState(prev_in_batch, prev_initially_idle);

  if (saw_est) {
    auto ack_line = transport::command::MakeAckLine(context.nextMsgId(),
                                                    {{"est_ms", std::to_string(agg_est_ms)}});
    EmitLineEvent(ack_line);
    aggregate.lines.push_back(ack_line);
  }
  CommandResult res;
  res.structured = std::move(aggregate);
  return res;
}

bool CommandBatchExecutor::isMotionAction(const std::string& action) const {
  return action == "MOVE" || action == "M" || action == "HOME" || action == "H" ||
         action == "WAKE" || action == "SLEEP";
}

uint32_t CommandBatchExecutor::maskFor(const ParsedCommand& command,
                                       const CommandExecutionContext& context) const {
  if (!isMotionAction(command.action)) {
    return 0;
  }
  uint32_t mask = 0;
  if (command.action == "MOVE" || command.action == "M" || command.action == "HOME" ||
      command.action == "H") {
    auto parts = Split(Trim(command.args), ',');
    if (!parts.empty()) {
      ParseIdMask(Trim(parts[0]), mask, context.controller().motorCount());
    }
  } else {
    ParseIdMask(Trim(command.args), mask, context.controller().motorCount());
  }
  return mask;
}

}  // namespace command
}  // namespace motor
