#ifndef UNIT_TEST
#include <Arduino.h>
#include "protocol/Protocol.h"

static Protocol protocol;
static char inputBuf[256];
static size_t inputLen = 0;

void serial_console_setup()
{
  Serial.begin(115200);
  while (!Serial)
  {
    ;
  }

  Serial.println("CTRL:READY Serial v1 — send HELP");
}

void serial_console_tick()
{
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
      std::string resp = protocol.processLine(std::string(inputBuf), millis());
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

  // Allow backend to progress time-based completions
  protocol.tick(millis());
}

#endif // UNIT_TEST
