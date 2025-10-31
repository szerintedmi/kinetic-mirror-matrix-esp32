// Arduino entrypoints; excluded during PlatformIO unit tests to avoid duplicate setup/loop
#if defined(ARDUINO) && !defined(UNIT_TEST)
#include <Arduino.h>
#include "console/SerialConsole.h"
#include "net_onboarding/NetOnboarding.h"
#include "net_onboarding/NetSingleton.h"
#include "transport/MessageId.h"
#include "transport/ResponseDispatcher.h"
#include "transport/ResponseModel.h"
#include "boards/Esp32Dev.hpp"

#include <string>

#if defined(ARDUINO) && (defined(ESP32) || defined(ARDUINO_ARCH_ESP32))
#include <WiFi.h>
#endif

using net_onboarding::State;
using net_onboarding::Net;

static State g_last_state = State::AP_ACTIVE; // will be updated after begin()
constexpr uint32_t kResetHoldMs = 5000;

static void EmitNetEvent(transport::response::EventType type,
                         const std::string &cmd_id,
                         const char *code,
                         const char *reason,
                         std::initializer_list<std::pair<const char *, std::string>> attributes) {
  transport::response::Event evt;
  evt.type = type;
  evt.action = "NET";
  evt.cmd_id = cmd_id;
  if (code) {
    evt.code = code;
  }
  if (reason) {
    evt.reason = reason;
  }
  for (const auto &kv : attributes) {
    evt.attributes[kv.first] = kv.second;
  }
  transport::response::ResponseDispatcher::Instance().Emit(evt);
}

static void EmitNetDone(const std::string &cmd_id,
                        const char *status,
                        std::initializer_list<std::pair<const char *, std::string>> attributes) {
  if (cmd_id.empty()) {
    return;
  }
  transport::response::Event evt;
  evt.type = transport::response::EventType::kDone;
  evt.action = "NET";
  evt.cmd_id = cmd_id;
  evt.attributes["status"] = status ? status : "done";
  for (const auto &kv : attributes) {
    evt.attributes[kv.first] = kv.second;
  }
  transport::response::ResponseDispatcher::Instance().Emit(evt);
}

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
      EmitNetEvent(transport::response::EventType::kInfo,
                   "",
                   "NET:RESET_BUTTON",
                   "LONG_PRESS",
                   {});
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
    String qssid = quote_str_(WiFi.softAPSSID().c_str());
    std::string ssid = std::string(qssid.c_str());
    std::string ip_str = std::to_string(ip[0]) + "." +
                         std::to_string(ip[1]) + "." +
                         std::to_string(ip[2]) + "." +
                         std::to_string(ip[3]);
    std::string active = transport::message_id::HasActive() ? transport::message_id::Active() : std::string();
    EmitNetEvent(transport::response::EventType::kInfo,
                 active,
                 "NET:AP_ACTIVE",
                 "",
                 {{"state", "AP_ACTIVE"},
                  {"ssid", ssid},
                  {"ip", ip_str}});
    if (!active.empty()) {
      EmitNetDone(active,
                  "done",
                  {{"state", "AP_ACTIVE"},
                   {"ssid", ssid},
                   {"ip", ip_str}});
      // RESET path may emit AP_ACTIVE immediately; if so, clear the correlation now
      transport::message_id::ClearActive();
    }
  } else if (g_last_state == State::CONNECTING) {
    std::string active = transport::message_id::HasActive() ? transport::message_id::Active() : std::string();
    EmitNetEvent(transport::response::EventType::kInfo,
                 active,
                 "NET:CONNECTING",
                 "",
                 {{"state", "CONNECTING"}});
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
        std::string active_err = transport::message_id::HasActive() ? transport::message_id::Active() : std::string();
        EmitNetEvent(transport::response::EventType::kError,
                     active_err,
                     "NET_CONNECT_FAILED",
                     "",
                     {});
      }
      auto ip = WiFi.softAPIP();
      String qssid = quote_str_(WiFi.softAPSSID().c_str());
      std::string ssid = std::string(qssid.c_str());
      std::string ip_str = std::to_string(ip[0]) + "." +
                           std::to_string(ip[1]) + "." +
                           std::to_string(ip[2]) + "." +
                           std::to_string(ip[3]);
      std::string active = transport::message_id::HasActive() ? transport::message_id::Active() : std::string();
      EmitNetEvent(transport::response::EventType::kInfo,
                   active,
                   "NET:AP_ACTIVE",
                   "",
                   {{"state", "AP_ACTIVE"},
                    {"ssid", ssid},
                    {"ip", ip_str}});
      if (!active.empty()) {
        const char *status = (prev == State::CONNECTING) ? "error" : "done";
        EmitNetDone(active,
                    status,
                    {{"state", "AP_ACTIVE"},
                     {"ssid", ssid},
                     {"ip", ip_str}});
        // AP_ACTIVE after a connect attempt (success or fallback) ends the flow
        transport::message_id::ClearActive();
      }
    } else if (s.state == State::CONNECTING) {
      std::string active = transport::message_id::HasActive() ? transport::message_id::Active() : std::string();
      EmitNetEvent(transport::response::EventType::kInfo,
                   active,
                   "NET:CONNECTING",
                   "",
                   {{"state", "CONNECTING"}});
    } else if (s.state == State::CONNECTED) {
      String qssid = quote_str_(s.ssid.data());
      std::string ssid = std::string(qssid.c_str());
      std::string ip_str = std::string(s.ip.data());
      std::string rssi = std::to_string(s.rssi_dbm);
      std::string active = transport::message_id::HasActive() ? transport::message_id::Active() : std::string();
      EmitNetEvent(transport::response::EventType::kInfo,
                   active,
                   "NET:CONNECTED",
                   "",
                   {{"state", "CONNECTED"},
                    {"ssid", ssid},
                    {"ip", ip_str},
                    {"rssi", rssi}});
      if (!active.empty()) {
        EmitNetDone(active,
                    "done",
                    {{"state", "CONNECTED"},
                     {"ssid", ssid},
                     {"ip", ip_str},
                     {"rssi", rssi}});
        // CONNECTED after a connect attempt ends the flow
        transport::message_id::ClearActive();
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
