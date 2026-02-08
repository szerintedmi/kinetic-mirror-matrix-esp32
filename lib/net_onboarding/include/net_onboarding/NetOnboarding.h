#pragma once

// NetOnboarding — minimal Wi‑Fi onboarding helper for ESP32 (Arduino)
// Public API is intentionally small and portable. The header avoids
// hard dependencies on Arduino types so it also compiles for `native` tests.

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
#include "net_onboarding/ConnectionSequence.h"
#include "net_onboarding/Platform.h"
#include "net_onboarding/WifiCredentials.h"
#include "net_onboarding/WifiCredentialsStore.h"

#include <array>
#include <memory>
#include <string>
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
  int rssi_dbm;  // valid when CONNECTED, else 0
  // IPv4 dotted-quad
  std::array<char, 16> ip;    // xxx.xxx.xxx.xxx + NUL
  std::array<char, 33> ssid;  // up to 32 chars + NUL
  std::array<char, 18> mac;   // XX:XX:XX:XX:XX:XX + NUL
  std::array<char, 32> ap_ssid;
  CredentialSlot connected_slot;  // Which slot we're connected to (valid when CONNECTED)
};

class NetOnboarding {
public:
  // Begin the library. Attempts STA if credentials exist; otherwise starts AP.
  // `timeout_per_network_ms` is how long to wait for each network before trying the next.
  void begin(uint32_t timeout_per_network_ms = kDefaultTimeoutPerNetworkMs);

  // Non-blocking maintenance. Call from Arduino loop() frequently.
  void loop();

  // Configure status LED pin and polarity (active-low true means logical "on"
  // drives the pin low). Call before begin(); optional.
  void configureStatusLed(int pin, bool active_low);

  // Save primary credentials and immediately attempt to connect in STA mode.
  // Returns true if credentials persisted successfully.
  // This is the backward-compatible API (equivalent to setPrimaryCredentials).
  bool setCredentials(const char* ssid, const char* pass);

  // Save credentials for a specific slot.
  bool setPrimaryCredentials(const char* ssid, const char* pass);
  bool setSecondaryCredentials(const char* ssid, const char* pass);

  // Get credentials for a specific slot (for NET:GET_CONFIG).
  // Returns true if slot has valid credentials.
  bool getPrimaryCredentials(std::string& out_ssid, std::string& out_pass) const;
  bool getSecondaryCredentials(std::string& out_ssid, std::string& out_pass) const;

  // Clear primary credentials and switch to AP mode (clears both slots).
  void resetCredentials();

  // Clear only secondary credentials (keeps primary).
  void clearSecondaryCredentials();

  // Snapshot of current state, RSSI, and IP (connected only).
  Status status() const;
  void apPassword(std::array<char, 65>& out) const;
  void deviceMac(std::array<char, 18>& out) const;
  void softApSsid(std::array<char, 32>& out) const;
  // Scan nearby Wi‑Fi networks, strongest first. Returns count (<= max_results).
  int scanNetworks(std::vector<WifiScanResult>& out,
                   int max_results = 12,
                   bool include_hidden = true);

  // NVS helpers (exposed primarily for tests/tools)
  bool saveCredentials(const char* ssid, const char* pass);
  bool loadCredentials(std::string& out_ssid, std::string& out_pass);
  void clearCredentials();

  // Optional: override timeout per network at runtime
  void setTimeoutPerNetworkMs(uint32_t ms) {
    timeout_per_network_ms_ = ms;
  }

#if defined(USE_STUB_BACKEND)
  // Test hooks for native environment (no real Wi‑Fi):
  // - If set to true, next connect attempt will flip to CONNECTED after
  // `simulate_connect_delay_ms_`.
  // - If false, CONNECTING will time out to AP.
  void setTestSimulation(bool will_connect, uint32_t delay_ms) {
    simulate_will_connect_ = will_connect;
    simulate_connect_delay_ms_ = delay_ms;
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
  void updateIdentity_();

  void refreshLedPattern_();
  void updateLed_();
  void applyLedState_(bool on);

private:
  Status st_{State::AP_ACTIVE, 0, {'0', '.', '0', '.', '0', '.', '0', '\0'}, {}, {}, {}, CredentialSlot::PRIMARY};
  uint32_t timeout_per_network_ms_{kDefaultTimeoutPerNetworkMs};
  uint32_t connecting_since_ms_{0};

  // Platform adapters (ESP32 or stub)
  std::unique_ptr<IWifi> wifi_;

  // Multi-network support
  std::unique_ptr<WifiCredentialsStore> creds_store_;
  std::unique_ptr<ConnectionSequence> connection_seq_;
  std::string last_ssid_;

  enum class LedPattern : uint8_t { OFF, SOLID, BLINK_SLOW, BLINK_FAST };
  bool led_configured_{false};
  int led_pin_{-1};
  bool led_active_low_{false};
  LedPattern led_pattern_{LedPattern::OFF};
  uint32_t led_last_toggle_ms_{0};
  bool led_logical_on_{false};

#if defined(USE_STUB_BACKEND)
  // Simulation (native/unit-test) only
  bool simulate_will_connect_{false};
  uint32_t simulate_connect_delay_ms_{0};
  uint32_t simulate_connect_start_ms_{0};
#endif

#if defined(UNIT_TEST)
public:
  bool debugLedOn() const {
    return led_logical_on_;
  }
  uint8_t debugLedPattern() const {
    return (uint8_t)led_pattern_;
  }
#endif
};

}  // namespace net_onboarding

// HTTP OTA route registration on the portal web server.
// Call from main setup() after Net().begin() and Ota().begin().
#if defined(ARDUINO) && (defined(ESP32) || defined(ARDUINO_ARCH_ESP32))
namespace ota {
class OtaManager;
}
namespace net_onboarding {
void registerHttpOtaOnPortal(ota::OtaManager& mgr);
}
#endif
