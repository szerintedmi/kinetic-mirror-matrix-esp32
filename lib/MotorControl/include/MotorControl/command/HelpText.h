#pragma once

#include <string>

namespace motor {
namespace command {

// Returns the canonical HELP text used across serial and MQTT transports.
// The string contains newline separators between lines and matches the
// content printed on the serial console.
const std::string& HelpText();

}  // namespace command
}  // namespace motor
