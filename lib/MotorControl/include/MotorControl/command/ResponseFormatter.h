#pragma once

#include "MotorControl/command/CommandResult.h"

namespace motor {
namespace command {

// Format a command result for the legacy serial transport.
std::string FormatForSerial(const CommandResult& result);

}  // namespace command
}  // namespace motor
