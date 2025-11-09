#pragma once

#include "MotorControl/command/CommandExecutionContext.h"
#include "MotorControl/command/CommandParser.h"
#include "MotorControl/command/CommandRouter.h"

namespace motor {
namespace command {

class CommandBatchExecutor {
public:
  CommandResult execute(const std::vector<ParsedCommand>& commands,
                        CommandExecutionContext& context,
                        CommandRouter& router,
                        uint32_t now_ms);

private:
  bool isMotionAction(const std::string& action) const;
  uint32_t maskFor(const ParsedCommand& command, const CommandExecutionContext& context) const;
};

}  // namespace command
}  // namespace motor
