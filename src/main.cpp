// Arduino entrypoints; excluded during PlatformIO unit tests to avoid duplicate setup/loop
#if defined(ARDUINO) && !defined(UNIT_TEST)
#include <Arduino.h>
#include "console/SerialConsole.h"
#include "net_onboarding/NetOnboarding.h"
#include "net_onboarding/NetSingleton.h"
#include "net_onboarding/Cid.h"
#include "boards/Esp32Dev.hpp"

#if defined(ARDUINO) && (defined(ESP32) || defined(ARDUINO_ARCH_ESP32))
#include <WiFi.h>
#endif

using net_onboarding::State;
using net_onboarding::Net;

static State g_last_state = State::AP_ACTIVE; // will be updated after begin()
constexpr uint32_t kResetHoldMs = 5000;

static void reset_button_setup()
{
#if defined(ARDUINO)
  if (RESET_BUTTON_PIN >= 0) {
    pinMode(RESET_BUTTON_PIN, RESET_BUTTON_ACTIVE_LOW ? INPUT_PULLUP : INPUT_PULLDOWN);
  }
#endif
}

static void reset_button_tick(uint32_t now_ms)
{
#if defined(ARDUINO)
  if (RESET_BUTTON_PIN < 0) return;
  bool pressed = digitalRead(RESET_BUTTON_PIN) == (RESET_BUTTON_ACTIVE_LOW ? LOW : HIGH);
  static bool was_pressed = false;
  static uint32_t press_started = 0;
  static bool fired = false;

  if (pressed) {
    if (!was_pressed) {
      press_started = now_ms;
      fired = false;
    } else if (!fired && (now_ms - press_started) >= kResetHoldMs) {
      fired = true;
      Serial.println("CTRL: NET:RESET_BUTTON LONG_PRESS");
      Net().resetCredentials();
    }
  } else {
    fired = false;
  }

  was_pressed = pressed;
#endif
}

// Quote and escape a C-string for serial output: returns Arduino String with
// surrounding double quotes and escapes embedded '"' and '\\'.
static String quote_str_(const char* s) {
  if (!s) return String("\"");
  String out; out.reserve((s ? strlen(s) : 0) + 2);
  out += '"';
  for (const char* p = s; *p; ++p) {
    if (*p == '"' || *p == '\\') out += '\\';
    out += *p;
  }
  out += '"';
  return out;
}

void setup() {
  serial_console_setup();

  // Configure Wi‑Fi onboarding
  Net().configureStatusLed(STATUS_LED_PIN, STATUS_LED_ACTIVE_LOW);
  Net().setConnectTimeoutMs(8000);
#ifdef SEED_NVS_CLEAR
  // Optional one-time build flag to force AP path
  Net().clearCredentials();
#endif
  Net().begin(8000);
  g_last_state = Net().status().state;
  reset_button_setup();

  // Emit initial state as control event
#if defined(ARDUINO) && (defined(ESP32) || defined(ARDUINO_ARCH_ESP32))
  if (g_last_state == State::AP_ACTIVE) {
    auto ip = WiFi.softAPIP();
    if (net_onboarding::HasActiveCid()) {
      String qssid = quote_str_(WiFi.softAPSSID().c_str());
      Serial.printf("CTRL: NET:AP_ACTIVE CID=%lu ssid=%s ip=%u.%u.%u.%u\n",
                    (unsigned long)net_onboarding::ActiveCid(), qssid.c_str(), ip[0], ip[1], ip[2], ip[3]);
      // RESET path may emit AP_ACTIVE immediately; if so, clear CID now
      net_onboarding::ClearActiveCid();
    } else {
      String qssid = quote_str_(WiFi.softAPSSID().c_str());
      Serial.printf("CTRL: NET:AP_ACTIVE ssid=%s ip=%u.%u.%u.%u\n",
                    qssid.c_str(), ip[0], ip[1], ip[2], ip[3]);
    }
  } else if (g_last_state == State::CONNECTING) {
    if (net_onboarding::HasActiveCid()) {
      Serial.printf("CTRL: NET:CONNECTING CID=%lu\n", (unsigned long)net_onboarding::ActiveCid());
    } else {
      Serial.println("CTRL: NET:CONNECTING");
    }
  }
#endif
}

void loop() {
  serial_console_tick();
  Net().loop();
  reset_button_tick(millis());

  auto s = Net().status();
  if (s.state != g_last_state) {
    State prev = g_last_state;
    g_last_state = s.state;
#if defined(ARDUINO) && (defined(ESP32) || defined(ARDUINO_ARCH_ESP32))
    if (s.state == State::AP_ACTIVE) {
      // If we fell back from CONNECTING → AP, surface connect failure
      if (prev == State::CONNECTING) {
        if (net_onboarding::HasActiveCid()) {
          Serial.printf("CTRL:ERR CID=%lu NET_CONNECT_FAILED\n", (unsigned long)net_onboarding::ActiveCid());
        } else {
          Serial.println("CTRL:ERR NET_CONNECT_FAILED");
        }
      }
      auto ip = WiFi.softAPIP();
      if (net_onboarding::HasActiveCid()) {
        String qssid = quote_str_(WiFi.softAPSSID().c_str());
        Serial.printf("CTRL: NET:AP_ACTIVE CID=%lu ssid=%s ip=%u.%u.%u.%u\n",
                      (unsigned long)net_onboarding::ActiveCid(), qssid.c_str(), ip[0], ip[1], ip[2], ip[3]);
        // AP_ACTIVE after a connect attempt (success or fallback) ends the flow
        net_onboarding::ClearActiveCid();
      } else {
        String qssid = quote_str_(WiFi.softAPSSID().c_str());
        Serial.printf("CTRL: NET:AP_ACTIVE ssid=%s ip=%u.%u.%u.%u\n",
                      qssid.c_str(), ip[0], ip[1], ip[2], ip[3]);
      }
    } else if (s.state == State::CONNECTING) {
      if (net_onboarding::HasActiveCid()) {
        Serial.printf("CTRL: NET:CONNECTING CID=%lu\n", (unsigned long)net_onboarding::ActiveCid());
      } else {
        Serial.println("CTRL: NET:CONNECTING");
      }
    } else if (s.state == State::CONNECTED) {
      if (net_onboarding::HasActiveCid()) {
        String qssid = quote_str_(s.ssid.data());
        Serial.printf("CTRL: NET:CONNECTED CID=%lu ssid=%s ip=%s rssi=%d\n", (unsigned long)net_onboarding::ActiveCid(), qssid.c_str(), s.ip.data(), s.rssi_dbm);
        // CONNECTED after a connect attempt ends the flow
        net_onboarding::ClearActiveCid();
      } else {
        String qssid = quote_str_(s.ssid.data());
        Serial.printf("CTRL: NET:CONNECTED ssid=%s ip=%s rssi=%d\n", qssid.c_str(), s.ip.data(), s.rssi_dbm);
      }
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
