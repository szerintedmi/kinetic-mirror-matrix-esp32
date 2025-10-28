#pragma once

// NetOnboarding — minimal Wi‑Fi onboarding helper for ESP32 (Arduino)
// Public API is intentionally small and portable. The header avoids
// hard dependencies on Arduino types so it also compiles for `native` tests.

#include <stdint.h>
#include <stddef.h>

#if defined(__cplusplus)
#include <array>
#include <string>
#include <memory>

#include "net_onboarding/Platform.h"
#endif

namespace net_onboarding {

// Connection state exposed to callers.
enum class State : uint8_t {
  AP_ACTIVE = 0,
  CONNECTING = 1,
  CONNECTED = 2,
};

struct Status {
  State state;
  int rssi_dbm; // valid when CONNECTED, else 0
  // IPv4 dotted-quad as a small fixed buffer; "0.0.0.0" when not connected
  std::array<char, 16> ip; // enough for xxx.xxx.xxx.xxx + NUL
};

class NetOnboarding {
public:
  // Begin the library. Attempts STA if credentials exist; otherwise starts AP.
  // `connect_timeout_ms` controls how long to wait for STA before falling back to AP.
  void begin(uint32_t connect_timeout_ms = 10000);

  // Non-blocking maintenance. Call from Arduino loop() frequently.
  void loop();

  // Save credentials and immediately attempt to connect in STA mode.
  // Returns true if credentials persisted successfully.
  bool setCredentials(const char* ssid, const char* pass);

  // Clear credentials and switch to AP mode.
  void resetCredentials();

  // Snapshot of current state, RSSI, and IP (connected only).
  Status status() const;

  // NVS helpers (exposed primarily for tests/tools)
  bool saveCredentials(const char* ssid, const char* pass);
  bool loadCredentials(std::string& out_ssid, std::string& out_pass);
  void clearCredentials();

  // Optional: override connect timeout at runtime
  void setConnectTimeoutMs(uint32_t ms) { connect_timeout_ms_ = ms; }

#if defined(USE_STUB_BACKEND)
  // Test hooks for native environment (no real Wi‑Fi):
  // - If set to true, next connect attempt will flip to CONNECTED after `simulate_connect_delay_ms_`.
  // - If false, CONNECTING will time out to AP.
  void setTestSimulation(bool will_connect, uint32_t delay_ms) {
    simulate_will_connect_ = will_connect; simulate_connect_delay_ms_ = delay_ms;
    ConfigureStubWifi(will_connect, delay_ms);
  }
#endif

private:
  // Platform-neutral helpers
  uint32_t nowMs_() const;
  void enterApMode_();
  void enterConnecting_(const char* ssid, const char* pass);
  void enterConnected_();

  // SoftAP SSID utility (prefix + last-3-bytes(MAC) in hex)
  void buildApSsid_(std::array<char, 32>& out) const;

private:
  Status st_ { State::AP_ACTIVE, 0, { '0','.', '0','.', '0','.', '0', '\0' } };
  uint32_t connect_timeout_ms_ { 10000 };
  uint32_t connecting_since_ms_ { 0 };

  // Platform adapters (ESP32 or stub)
  std::unique_ptr<IWifi> wifi_;
  std::unique_ptr<INvs> nvs_;

#if defined(USE_STUB_BACKEND)
  // Simulation (native/unit-test) only
  bool simulate_will_connect_ { false };
  uint32_t simulate_connect_delay_ms_ { 0 };
  uint32_t simulate_connect_start_ms_ { 0 };
#endif
};

} // namespace net_onboarding
