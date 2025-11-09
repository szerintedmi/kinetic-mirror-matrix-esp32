#pragma once

#include "MotorControl/command/CommandRouter.h"

namespace motor {
namespace command {

class MotorCommandHandler : public CommandHandler {
public:
  bool canHandle(const std::string& action) const override;
  CommandResult
  execute(const ParsedCommand& command, CommandExecutionContext& context, uint32_t now_ms) override;

private:
  CommandResult handleWake(const std::string& args, CommandExecutionContext& context);
  CommandResult handleSleep(const std::string& args, CommandExecutionContext& context);
  CommandResult
  handleMove(const std::string& args, CommandExecutionContext& context, uint32_t now_ms);
  CommandResult
  handleHome(const std::string& args, CommandExecutionContext& context, uint32_t now_ms);
};

class QueryCommandHandler : public CommandHandler {
public:
  bool canHandle(const std::string& action) const override;
  CommandResult
  execute(const ParsedCommand& command, CommandExecutionContext& context, uint32_t now_ms) override;

private:
  CommandResult handleHelp() const;
  CommandResult handleStatus(CommandExecutionContext& context);
  CommandResult handleGet(const std::string& args, CommandExecutionContext& context);
  CommandResult handleSet(const std::string& args, CommandExecutionContext& context);
};

class NetCommandHandler : public CommandHandler {
public:
  bool canHandle(const std::string& action) const override;
  CommandResult
  execute(const ParsedCommand& command, CommandExecutionContext& context, uint32_t now_ms) override;
};

class MqttConfigCommandHandler : public CommandHandler {
public:
  bool canHandle(const std::string& action) const override;
  CommandResult
  execute(const ParsedCommand& command, CommandExecutionContext& context, uint32_t now_ms) override;
};

}  // namespace command
}  // namespace motor
