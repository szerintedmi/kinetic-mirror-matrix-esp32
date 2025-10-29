#include "MotorControl/command/ResponseFormatter.h"

namespace motor {
namespace command {

std::string FormatForSerial(const CommandResult &result) {
  if (result.is_error) {
    return result.lines.empty() ? std::string() : result.lines.front();
  }
  return JoinLines(result.lines);
}

} // namespace command
} // namespace motor

