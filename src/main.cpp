#ifndef UNIT_TEST
#include <Arduino.h>
#include "console/SerialConsole.h"

void setup()
{
  serial_console_setup();
}

void loop()
{
  serial_console_tick();
}
#endif // UNIT_TEST
