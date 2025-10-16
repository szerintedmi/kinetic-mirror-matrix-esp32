// Arduino entrypoints; excluded during PlatformIO unit tests to avoid duplicate setup/loop
#if defined(ARDUINO) && !defined(UNIT_TEST)
#include <Arduino.h>
#include "console/SerialConsole.h"

void setup() { serial_console_setup(); }
void loop() { serial_console_tick(); }

#elif !defined(ARDUINO) && !defined(UNIT_TEST)
int main(int, char**)
{
  return 0;
}
#endif
