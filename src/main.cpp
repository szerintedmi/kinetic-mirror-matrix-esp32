// Arduino entrypoints; excluded during PlatformIO unit tests to avoid duplicate setup/loop
#if defined(ARDUINO) && !defined(UNIT_TEST)
#include "boards/Esp32Dev.hpp"
#include "console/SerialConsole.h"
#include "net_onboarding/NetOnboarding.h"
#include "net_onboarding/NetSingleton.h"
#include "transport/MessageId.h"
#include "transport/ResponseDispatcher.h"
#include "transport/ResponseModel.h"

#include <Arduino.h>
#include <cstring>
#include <string>

#if defined(ARDUINO) && (defined(ESP32) || defined(ARDUINO_ARCH_ESP32))
#include <WiFi.h>
#endif

using net_onboarding::Net;
using net_onboarding::State;
using ResponseAttribute = std::pair<const char*, std::string>;
using ResponseAttributeList = std::initializer_list<ResponseAttribute>;

namespace {
State& LastKnownNetState() {
  static State last_state = State::AP_ACTIVE;
  return last_state;
}
}  // namespace
constexpr uint32_t kResetHoldMs = 5000;
constexpr uint32_t kConnectTimeoutMs = 8000;

static void EmitNetEvent(transport::response::EventType type,
                         const std::string& cmd_id,
                         const char* code,
                         const char* reason,
                         ResponseAttributeList attributes) {
  transport::response::Event evt;
  evt.type = type;
  evt.action = "NET";
  evt.cmd_id = cmd_id;
  if (code != nullptr) {
    evt.code = code;
  }
  if (reason != nullptr) {
    evt.reason = reason;
  }
  for (const auto& kv : attributes) {
    evt.attributes[kv.first] = kv.second;
  }
  transport::response::ResponseDispatcher::Instance().Emit(evt);
}

static void
EmitNetDone(const std::string& cmd_id, const char* status, ResponseAttributeList attributes) {
  if (cmd_id.empty()) {
    return;
  }
  transport::response::Event evt;
  evt.type = transport::response::EventType::kDone;
  evt.action = "NET";
  evt.cmd_id = cmd_id;
  evt.attributes["status"] = (status != nullptr) ? status : "done";
  for (const auto& kv : attributes) {
    evt.attributes[kv.first] = kv.second;
  }
  transport::response::ResponseDispatcher::Instance().Emit(evt);
}

static void ResetButtonSetup() {
#if defined(ARDUINO)
  // cppcheck-suppress knownConditionTrueFalse
  if (RESET_BUTTON_PIN >= 0) {
    pinMode(RESET_BUTTON_PIN, RESET_BUTTON_ACTIVE_LOW ? INPUT_PULLUP : INPUT_PULLDOWN);
  }
#endif
}

static void ResetButtonTick(uint32_t now_ms) {
#if defined(ARDUINO)
  // cppcheck-suppress knownConditionTrueFalse
  if (RESET_BUTTON_PIN < 0) {
    return;
  }
  bool pressed = digitalRead(RESET_BUTTON_PIN) == (RESET_BUTTON_ACTIVE_LOW ? LOW : HIGH);
  static bool was_pressed = false;
  static uint32_t press_started = 0;  // cppcheck-suppress variableScope
  static bool fired = false;

  if (pressed) {
    if (!was_pressed) {
      press_started = now_ms;
      fired = false;
    } else if (!fired && (now_ms - press_started) >= kResetHoldMs) {
      fired = true;
      EmitNetEvent(transport::response::EventType::kInfo, "", "NET:RESET_BUTTON", "LONG_PRESS", {});
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
static String QuoteString(const char* value) {
  if (value == nullptr) {
    return String("\"");
  }
  String escaped;
  escaped.reserve(strlen(value) + 2);
  escaped += '"';
  for (const char* cursor = value; *cursor != '\0'; ++cursor) {
    if (*cursor == '"' || *cursor == '\\') {
      escaped += '\\';
    }
    escaped += *cursor;
  }
  escaped += '"';
  return escaped;
}

void setup() {
  serial_console_setup();
  State& last_state = LastKnownNetState();

  // Configure Wi‑Fi onboarding
  Net().configureStatusLed(STATUS_LED_PIN, STATUS_LED_ACTIVE_LOW);
  Net().setConnectTimeoutMs(kConnectTimeoutMs);
#ifdef SEED_NVS_CLEAR
  // Optional one-time build flag to force AP path
  Net().clearCredentials();
#endif
  Net().begin(kConnectTimeoutMs);
  last_state = Net().status().state;
  ResetButtonSetup();

  // Emit initial state as control event
#if defined(ARDUINO) && (defined(ESP32) || defined(ARDUINO_ARCH_ESP32))
  if (last_state == State::AP_ACTIVE) {
    auto soft_ap_ip = WiFi.softAPIP();
    String quoted_ssid = QuoteString(WiFi.softAPSSID().c_str());
    std::string ssid = std::string(quoted_ssid.c_str());
    std::string ip_address = std::to_string(soft_ap_ip[0]) + "." + std::to_string(soft_ap_ip[1]) +
                             "." + std::to_string(soft_ap_ip[2]) + "." +
                             std::to_string(soft_ap_ip[3]);
    std::string active_request_id =
        transport::message_id::HasActive() ? transport::message_id::Active() : std::string();
    EmitNetEvent(transport::response::EventType::kInfo,
                 active_request_id,
                 "NET:AP_ACTIVE",
                 "",
                 {{"state", "AP_ACTIVE"}, {"ssid", ssid}, {"ip", ip_address}});
    if (!active_request_id.empty()) {
      EmitNetDone(
          active_request_id, "done", {{"state", "AP_ACTIVE"}, {"ssid", ssid}, {"ip", ip_address}});
      // RESET path may emit AP_ACTIVE immediately; if so, clear the correlation now
      transport::message_id::ClearActive();
    }
  } else if (last_state == State::CONNECTING) {
    std::string active_request_id =
        transport::message_id::HasActive() ? transport::message_id::Active() : std::string();
    EmitNetEvent(transport::response::EventType::kInfo,
                 active_request_id,
                 "NET:CONNECTING",
                 "",
                 {{"state", "CONNECTING"}});
  }
#endif
}

void loop() {
  serial_console_tick();
  Net().loop();
  ResetButtonTick(millis());
  State& last_state = LastKnownNetState();

  auto status_snapshot = Net().status();
  if (status_snapshot.state != last_state) {
    State previous_state = last_state;
    last_state = status_snapshot.state;
#if defined(ARDUINO) && (defined(ESP32) || defined(ARDUINO_ARCH_ESP32))
    if (status_snapshot.state == State::AP_ACTIVE) {
      if (previous_state == State::CONNECTING) {
        std::string active_error =
            transport::message_id::HasActive() ? transport::message_id::Active() : std::string();
        EmitNetEvent(
            transport::response::EventType::kError, active_error, "NET_CONNECT_FAILED", "", {});
      }
      auto soft_ap_ip = WiFi.softAPIP();
      String quoted_ssid = QuoteString(WiFi.softAPSSID().c_str());
      std::string ssid = std::string(quoted_ssid.c_str());
      std::string ip_address = std::to_string(soft_ap_ip[0]) + "." + std::to_string(soft_ap_ip[1]) +
                               "." + std::to_string(soft_ap_ip[2]) + "." +
                               std::to_string(soft_ap_ip[3]);
      std::string active_request_id =
          transport::message_id::HasActive() ? transport::message_id::Active() : std::string();
      EmitNetEvent(transport::response::EventType::kInfo,
                   active_request_id,
                   "NET:AP_ACTIVE",
                   "",
                   {{"state", "AP_ACTIVE"}, {"ssid", ssid}, {"ip", ip_address}});
      if (!active_request_id.empty()) {
        EmitNetDone(active_request_id,
                    (previous_state == State::CONNECTING) ? "error" : "done",
                    {{"state", "AP_ACTIVE"}, {"ssid", ssid}, {"ip", ip_address}});
        transport::message_id::ClearActive();
      }
    } else if (status_snapshot.state == State::CONNECTING) {
      std::string active_request_id =
          transport::message_id::HasActive() ? transport::message_id::Active() : std::string();
      EmitNetEvent(transport::response::EventType::kInfo,
                   active_request_id,
                   "NET:CONNECTING",
                   "",
                   {{"state", "CONNECTING"}});
    } else if (status_snapshot.state == State::CONNECTED) {
      String quoted_ssid = QuoteString(status_snapshot.ssid.data());
      std::string ssid = std::string(quoted_ssid.c_str());
      std::string ip_address = std::string(status_snapshot.ip.data());
      std::string rssi_dbm = std::to_string(status_snapshot.rssi_dbm);
      std::string active_request_id =
          transport::message_id::HasActive() ? transport::message_id::Active() : std::string();
      EmitNetEvent(
          transport::response::EventType::kInfo,
          active_request_id,
          "NET:CONNECTED",
          "",
          {{"state", "CONNECTED"}, {"ssid", ssid}, {"ip", ip_address}, {"rssi", rssi_dbm}});
      if (!active_request_id.empty()) {
        EmitNetDone(
            active_request_id,
            "done",
            {{"state", "CONNECTED"}, {"ssid", ssid}, {"ip", ip_address}, {"rssi", rssi_dbm}});
        transport::message_id::ClearActive();
      }
    }
#endif
  }
}

#elif !defined(ARDUINO) && !defined(UNIT_TEST)
int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  return 0;
}
#endif
