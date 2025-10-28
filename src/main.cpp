// Arduino entrypoints; excluded during PlatformIO unit tests to avoid duplicate setup/loop
#if defined(ARDUINO) && !defined(UNIT_TEST)
#include <Arduino.h>
#include "console/SerialConsole.h"
#include "net_onboarding/NetOnboarding.h"
#include "net_onboarding/NetSingleton.h"

#if defined(ARDUINO) && (defined(ESP32) || defined(ARDUINO_ARCH_ESP32))
#include <WiFi.h>
#endif

using net_onboarding::State;
using net_onboarding::Net;

static State g_last_state = State::AP_ACTIVE; // will be updated after begin()

void setup() {
  serial_console_setup();

  // Configure Wi‑Fi onboarding
  Net().setConnectTimeoutMs(8000);
#ifdef SEED_NVS_CLEAR
  // Optional one-time build flag to force AP path
  Net().clearCredentials();
#endif
  Net().begin(8000);
  g_last_state = Net().status().state;

  // Emit initial state as control event
#if defined(ARDUINO) && (defined(ESP32) || defined(ARDUINO_ARCH_ESP32))
  if (g_last_state == State::AP_ACTIVE) {
    auto ip = WiFi.softAPIP();
    Serial.printf("CTRL: NET:AP_ACTIVE ssid=%s ip=%u.%u.%u.%u\n",
                  WiFi.softAPSSID().c_str(), ip[0], ip[1], ip[2], ip[3]);
  } else if (g_last_state == State::CONNECTING) {
    Serial.println("CTRL: NET:CONNECTING");
  }
#endif
}

void loop() {
  serial_console_tick();
  Net().loop();

  auto s = Net().status();
  if (s.state != g_last_state) {
    State prev = g_last_state;
    g_last_state = s.state;
#if defined(ARDUINO) && (defined(ESP32) || defined(ARDUINO_ARCH_ESP32))
    if (s.state == State::AP_ACTIVE) {
      // If we fell back from CONNECTING → AP, surface connect failure
      if (prev == State::CONNECTING) {
        Serial.println("CTRL:ERR NET_CONNECT_FAILED");
      }
      auto ip = WiFi.softAPIP();
      Serial.printf("CTRL: NET:AP_ACTIVE ssid=%s ip=%u.%u.%u.%u\n",
                    WiFi.softAPSSID().c_str(), ip[0], ip[1], ip[2], ip[3]);
    } else if (s.state == State::CONNECTING) {
      Serial.println("CTRL: NET:CONNECTING");
    } else if (s.state == State::CONNECTED) {
      Serial.printf("CTRL: NET:CONNECTED ssid=%s ip=%s rssi=%d\n", s.ssid.data(), s.ip.data(), s.rssi_dbm);
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
