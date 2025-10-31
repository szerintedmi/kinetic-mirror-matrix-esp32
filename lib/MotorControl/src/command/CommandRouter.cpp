#include "MotorControl/command/CommandRouter.h"

namespace motor {
namespace command {

CommandRouter::CommandRouter(std::vector<std::unique_ptr<CommandHandler>> handlers)
    : handlers_(std::move(handlers)) {}

bool CommandRouter::knowsAction(const std::string &action) const {
  for (const auto &handler : handlers_) {
    if (handler->canHandle(action)) {
      return true;
    }
  }
  return false;
}

CommandResult CommandRouter::dispatch(const ParsedCommand &command,
                                      CommandExecutionContext &context,
                                      uint32_t now_ms) {
  for (auto &handler : handlers_) {
    if (handler->canHandle(command.action)) {
      return handler->execute(command, context, now_ms);
    }
  }
  std::string msg_id = context.nextMsgId();
  return CommandResult::Error("CTRL:ERR msg_id=" + msg_id + " E01 BAD_CMD");
}

} // namespace command
} // namespace motor
