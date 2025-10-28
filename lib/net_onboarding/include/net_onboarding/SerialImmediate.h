#pragma once

#include <string>

namespace net_onboarding {

// Emit a CTRL line immediately to the active transport (Serial on-device).
// Returns true if the line was written immediately; false when running in
// environments without a live serial port (native/tests). Callers can use the
// return value to decide whether to also buffer the line in their response.
bool PrintCtrlLineImmediate(const std::string &line);

} // namespace net_onboarding
