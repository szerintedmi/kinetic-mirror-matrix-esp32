#include "MotorControl/command/ResponseFormatter.h"

#include "transport/CommandSchema.h"

namespace motor {
namespace command {

std::string FormatForSerial(const CommandResult& result) {
  if (!result.hasStructuredResponse()) {
    return {};
  }
  return transport::command::FormatSerialResponse(result.structuredResponse());
}

}  // namespace command
}  // namespace motor
