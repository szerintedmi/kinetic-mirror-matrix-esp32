#pragma once

#include "MotorControl/command/CommandResult.h"
#include "MotorControl/command/CommandUtils.h"

#include <cstdint>
#include <string>
#include <vector>

namespace motor {
namespace command {

struct ParsedCommand {
  std::string raw;
  std::string verb;
  std::string args;
};

class CommandParser {
public:
  std::vector<ParsedCommand> parse(const std::string &line) const;
};

} // namespace command
} // namespace motor

