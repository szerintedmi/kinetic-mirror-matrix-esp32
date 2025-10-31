#include "MotorControl/command/CommandRouter.h"

#include "transport/CommandSchema.h"
#include "transport/ResponseDispatcher.h"
#include "transport/ResponseModel.h"

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
  auto line = transport::command::MakeErrorLine(msg_id,
                                                "E01",
                                                "BAD_CMD",
                                                {});
  transport::response::ResponseDispatcher::Instance().Emit(
      transport::response::BuildEvent(line, command.action));
  CommandResult res;
  res.is_error = true;
  res.append(line);
  return res;
}

} // namespace command
} // namespace motor
