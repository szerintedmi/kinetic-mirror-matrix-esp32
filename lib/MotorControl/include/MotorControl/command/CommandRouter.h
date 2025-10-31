#pragma once

#include "MotorControl/command/CommandExecutionContext.h"
#include "MotorControl/command/CommandParser.h"

#include <memory>
#include <vector>

namespace motor {
namespace command {

class CommandHandler {
public:
  virtual ~CommandHandler() = default;
  virtual bool canHandle(const std::string &action) const = 0;
  virtual CommandResult execute(const ParsedCommand &command,
                                CommandExecutionContext &context,
                                uint32_t now_ms) = 0;
};

class CommandRouter {
public:
  explicit CommandRouter(std::vector<std::unique_ptr<CommandHandler>> handlers);

  bool knowsAction(const std::string &action) const;

  CommandResult dispatch(const ParsedCommand &command,
                         CommandExecutionContext &context,
                         uint32_t now_ms);

private:
  std::vector<std::unique_ptr<CommandHandler>> handlers_;
};

} // namespace command
} // namespace motor
