#include "MotorControl/command/CommandBatchExecutor.h"

#include "MotorControl/command/CommandUtils.h"

#include <sstream>

namespace motor {
namespace command {

namespace {

uint32_t parseEstMs(const std::string &line) {
  size_t p = line.find("est_ms=");
  if (p == std::string::npos) {
    return 0;
  }
  p += 7;
  while (p < line.size() && line[p] == ' ') ++p;
  uint32_t acc = 0;
  bool any = false;
  while (p < line.size() && line[p] >= '0' && line[p] <= '9') {
    acc = acc * 10u + static_cast<uint32_t>(line[p] - '0');
    ++p;
    any = true;
  }
  return any ? acc : 0;
}

} // namespace

CommandResult CommandBatchExecutor::execute(const std::vector<ParsedCommand> &commands,
                                            CommandExecutionContext &context,
                                            CommandRouter &router,
                                            uint32_t now_ms) {
  // Overlap detection
  uint32_t seen = 0;
  for (const auto &cmd : commands) {
    uint32_t mask = maskFor(cmd, context);
    if (mask != 0 && (mask & seen)) {
      std::ostringstream eo;
      eo << "CTRL:ERR msg_id=" << context.nextMsgId() << " E03 BAD_PARAM MULTI_CMD_CONFLICT";
      return CommandResult::Error(eo.str());
    }
    seen |= mask;
  }

  // Unknown action detection
  for (const auto &cmd : commands) {
    if (!router.knowsAction(cmd.action)) {
      std::ostringstream eo;
      eo << "CTRL:ERR msg_id=" << context.nextMsgId() << " E01 BAD_CMD";
      return CommandResult::Error(eo.str());
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

  std::vector<std::string> combined;
  uint32_t agg_est_ms = 0;
  bool saw_est = false;
  bool saw_err = false;
  std::string first_err;

  for (const auto &cmd : commands) {
    CommandResult result = router.dispatch(cmd, context, now_ms);
    if (result.is_error) {
      context.setBatchState(prev_in_batch, prev_initially_idle);
      return result;
    }
    for (const auto &line : result.lines) {
      if (line.empty()) continue;
      if (line.rfind("CTRL:ERR", 0) == 0) {
        if (!saw_err) {
          saw_err = true;
          first_err = line;
        }
        break;
      }
      if (line.rfind("CTRL:ACK", 0) == 0 && line.find("est_ms=") != std::string::npos) {
        uint32_t value = parseEstMs(line);
        if (value > agg_est_ms) {
          agg_est_ms = value;
        }
        saw_est = true;
      } else {
        combined.push_back(line);
      }
    }
    if (saw_err) break;
  }

  context.setBatchState(prev_in_batch, prev_initially_idle);

  if (saw_err) {
    return CommandResult::Error(first_err);
  }
  if (saw_est) {
    std::ostringstream ok;
    ok << "CTRL:ACK msg_id=" << context.nextMsgId() << " est_ms=" << agg_est_ms;
    combined.push_back(ok.str());
  }
  CommandResult res;
  res.lines = std::move(combined);
  return res;
}

bool CommandBatchExecutor::isMotionAction(const std::string &action) const {
  return action == "MOVE" || action == "M" ||
         action == "HOME" || action == "H" ||
         action == "WAKE" || action == "SLEEP";
}

uint32_t CommandBatchExecutor::maskFor(const ParsedCommand &command,
                                       const CommandExecutionContext &context) const {
  if (!isMotionAction(command.action)) {
    return 0;
  }
  uint32_t mask = 0;
  if (command.action == "MOVE" || command.action == "M" ||
      command.action == "HOME" || command.action == "H") {
    auto parts = Split(Trim(command.args), ',');
    if (!parts.empty()) {
      ParseIdMask(Trim(parts[0]), mask, context.controller().motorCount());
    }
  } else {
    ParseIdMask(Trim(command.args), mask, context.controller().motorCount());
  }
  return mask;
}

} // namespace command
} // namespace motor
