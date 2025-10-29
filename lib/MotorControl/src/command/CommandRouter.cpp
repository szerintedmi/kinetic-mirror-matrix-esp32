#include "MotorControl/command/CommandRouter.h"

namespace motor {
namespace command {

CommandRouter::CommandRouter(std::vector<std::unique_ptr<CommandHandler>> handlers)
    : handlers_(std::move(handlers)) {}

bool CommandRouter::knowsVerb(const std::string &verb) const {
  for (const auto &handler : handlers_) {
    if (handler->canHandle(verb)) {
      return true;
    }
  }
  return false;
}

CommandResult CommandRouter::dispatch(const ParsedCommand &command,
                                      CommandExecutionContext &context,
                                      uint32_t now_ms) {
  for (auto &handler : handlers_) {
    if (handler->canHandle(command.verb)) {
      return handler->execute(command, context, now_ms);
    }
  }
  return CommandResult::Error("CTRL:ERR CID=" + std::to_string(context.nextCid()) + " E01 BAD_CMD");
}

} // namespace command
} // namespace motor

