#include "net_onboarding/SerialImmediate.h"

#if defined(ARDUINO)
#include <Arduino.h>
#endif

namespace net_onboarding {

bool PrintCtrlLineImmediate(const std::string &line) {
#if defined(ARDUINO)
  Serial.println(line.c_str());
  return true;
#else
  (void)line;
  return false;
#endif
}

} // namespace net_onboarding
