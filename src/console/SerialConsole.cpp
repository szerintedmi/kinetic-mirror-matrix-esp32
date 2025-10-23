// Arduino serial console only
#if defined(ARDUINO)
#include <Arduino.h>
#include "MotorControl/MotorCommandProcessor.h"

// Defer heavy backend initialization until setup() runs, to avoid
// hardware init during static construction before Arduino core is ready.
static MotorCommandProcessor *commandProcessor = nullptr;
static char inputBuf[256];
static size_t inputLen = 0;
static uint32_t ignore_until_ms = 0; // grace period to ignore deploy-time noise

void serial_console_setup()
{
  Serial.begin(115200);
  while (!Serial)
  {
    ;
  }

  if (!commandProcessor)
  {
    commandProcessor = new MotorCommandProcessor();
  }

  // After flashing/deploy, host tools may send text into the port.
  // Ignore all serial input for a short window to avoid parsing it.
  ignore_until_ms = millis() + 500;
}

void serial_console_tick()
{
  // During initial grace window, drain and ignore any incoming bytes
  if (ignore_until_ms && millis() < ignore_until_ms)
  {
    while (Serial.available() > 0)
    {
      (void)Serial.read();
    }
    return;
  }
  if (ignore_until_ms && millis() >= ignore_until_ms)
  {
    ignore_until_ms = 0; // end grace period
    Serial.println("CTRL:READY Serial v1 — send HELP");
  }

  // Read incoming bytes until newline, then process
  while (Serial.available() > 0)
  {
    char c = (char)Serial.read();
    if (c == '\r')
    {
      // Ignore carriage return (common in CRLF); don't echo
      continue;
    }
    if (c == '\n')
    {
      // Echo newline for user feedback
      Serial.println();
      inputBuf[inputLen] = '\0';
      if (!commandProcessor)
        return; // not ready
      std::string resp = commandProcessor->processLine(std::string(inputBuf), millis());
      if (!resp.empty())
      {
        // Print as-is; response may be multi-line
        Serial.println(resp.c_str());
      }
      inputLen = 0;
    }
    else
    {
      // Handle backspace/delete
      if (c == 0x08 || c == 0x7F)
      { // BS or DEL
        if (inputLen > 0)
        {
          inputLen--;
          // Visual erase: move back, write space, move back
          Serial.write('\b');
          Serial.write(' ');
          Serial.write('\b');
        }
        continue;
      }
      if (inputLen + 1 < sizeof(inputBuf))
      {
        inputBuf[inputLen++] = c;
        // Echo typed character
        Serial.write(c);
      }
      else
      {
        // Overflow guard: reset buffer
        inputLen = 0;
        Serial.println("CTRL:ERR E03 BAD_PARAM buffer_overflow");
      }
    }
  }

  // Allow controller to progress time-based completions
  if (commandProcessor)
  {
    commandProcessor->tick(millis());
  }

  // Debug heartbeat removed after stabilization
}

#endif // ARDUINO
