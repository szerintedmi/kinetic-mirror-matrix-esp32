#include "MotorControl/command/CommandResult.h"

namespace motor {
namespace command {

std::string JoinLines(const std::vector<std::string> &lines) {
  if (lines.empty()) {
    return {};
  }
  std::string out;
  for (size_t i = 0; i < lines.size(); ++i) {
    out += lines[i];
    if (i + 1 < lines.size()) {
      out.push_back('\n');
    }
  }
  return out;
}

} // namespace command
} // namespace motor
