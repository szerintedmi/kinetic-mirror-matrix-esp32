// Arduino entrypoints; excluded during PlatformIO unit tests to avoid duplicate setup/loop
#if defined(ARDUINO) && !defined(UNIT_TEST)
#include <Arduino.h>
#include "console/SerialConsole.h"
#include "net_onboarding/NetOnboarding.h"

#if defined(ARDUINO) && (defined(ESP32) || defined(ARDUINO_ARCH_ESP32))
#include <WiFi.h>
#endif

using net_onboarding::NetOnboarding;
using net_onboarding::State;

static NetOnboarding g_net;
static State g_last_state = State::AP_ACTIVE; // will be updated after begin()

void setup() {
  serial_console_setup();

  // Configure Wi‑Fi onboarding
  g_net.setConnectTimeoutMs(8000);
#ifdef SEED_NVS_CLEAR
  // Optional one-time build flag to force AP path
  g_net.clearCredentials();
#endif
  g_net.begin(8000);
  g_last_state = g_net.status().state;

#if defined(ARDUINO) && (defined(ESP32) || defined(ARDUINO_ARCH_ESP32))
  if (g_last_state == State::AP_ACTIVE) {
    auto ip = WiFi.softAPIP();
    Serial.printf("NET: AP active SSID=%s ip=%u.%u.%u.%u\n",
                  WiFi.softAPSSID().c_str(), ip[0], ip[1], ip[2], ip[3]);
  } else if (g_last_state == State::CONNECTING) {
    Serial.println("NET: CONNECTING...");
  }
#endif
}

void loop() {
  serial_console_tick();
  g_net.loop();

  auto s = g_net.status();
  if (s.state != g_last_state) {
    g_last_state = s.state;
#if defined(ARDUINO) && (defined(ESP32) || defined(ARDUINO_ARCH_ESP32))
    if (s.state == State::AP_ACTIVE) {
      auto ip = WiFi.softAPIP();
      Serial.printf("NET: AP active SSID=%s ip=%u.%u.%u.%u\n",
                    WiFi.softAPSSID().c_str(), ip[0], ip[1], ip[2], ip[3]);
    } else if (s.state == State::CONNECTING) {
      Serial.println("NET: CONNECTING...");
    } else if (s.state == State::CONNECTED) {
      Serial.printf("NET: CONNECTED ip=%s rssi=%d\n", s.ip.data(), s.rssi_dbm);
    }
#endif
  }
}

#elif !defined(ARDUINO) && !defined(UNIT_TEST)
int main(int, char**)
{
  return 0;
}
#endif
