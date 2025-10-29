#include "MotorControl/command/CommandParser.h"

namespace motor {
namespace command {

std::vector<ParsedCommand> CommandParser::parse(const std::string &line) const {
  std::vector<ParsedCommand> out;
  auto trimmed = Trim(line);
  if (trimmed.empty()) {
    return out;
  }

  auto segments = Split(trimmed, ';');
  for (auto &segment : segments) {
    auto cmd = Trim(segment);
    if (cmd.empty()) {
      continue;
    }
    ParsedCommand parsed;
    parsed.raw = cmd;

    size_t space = cmd.find(' ');
    size_t colon = cmd.find(':');
    if (space != std::string::npos && (colon == std::string::npos || space < colon)) {
      parsed.verb = ToUpperCopy(cmd.substr(0, space));
      parsed.args = cmd.substr(space + 1);
    } else if (colon != std::string::npos) {
      parsed.verb = ToUpperCopy(cmd.substr(0, colon));
      parsed.args = cmd.substr(colon + 1);
    } else {
      parsed.verb = ToUpperCopy(cmd);
      parsed.args.clear();
    }
    out.push_back(parsed);
  }
  return out;
}

} // namespace command
} // namespace motor

