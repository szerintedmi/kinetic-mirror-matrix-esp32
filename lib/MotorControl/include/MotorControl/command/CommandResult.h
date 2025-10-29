#pragma once

#include <string>
#include <vector>

namespace motor {
namespace command {

struct CommandResult {
  bool is_error = false;
  std::vector<std::string> lines;

  // Convenience helpers
  static CommandResult Error(const std::string &line) {
    CommandResult r;
    r.is_error = true;
    r.lines.push_back(line);
    return r;
  }

  static CommandResult SingleLine(const std::string &line) {
    CommandResult r;
    r.lines.push_back(line);
    return r;
  }
};

std::string JoinLines(const std::vector<std::string> &lines);

} // namespace command
} // namespace motor

