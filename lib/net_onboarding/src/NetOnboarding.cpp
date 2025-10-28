#include "net_onboarding/NetOnboarding.h"

#if defined(ARDUINO)
#include <Arduino.h>
#endif

#include <cstdio>
#include <cstring>

namespace net_onboarding {

static void ip_to_str_(uint32_t ip, std::array<char,16>& out) {
  unsigned b1 = (ip) & 0xFFu;
  unsigned b2 = (ip >> 8) & 0xFFu;
  unsigned b3 = (ip >> 16) & 0xFFu;
  unsigned b4 = (ip >> 24) & 0xFFu;
  // Arduino IPAddress::toString renders normal order; our bytes are little-endian depending on platform
  // Using WiFi.localIP() path on Arduino instead. This helper is used only for non-Arduino paths.
  std::snprintf(out.data(), out.size(), "%u.%u.%u.%u", b1, b2, b3, b4);
}

uint32_t NetOnboarding::nowMs_() const {
#if defined(ARDUINO)
  return millis();
#else
  using namespace std::chrono;
  static steady_clock::time_point t0 = steady_clock::now();
  return (uint32_t)duration_cast<milliseconds>(steady_clock::now() - t0).count();
#endif
}

void NetOnboarding::begin(uint32_t connect_timeout_ms) {
  connect_timeout_ms_ = connect_timeout_ms;

#if defined(ARDUINO) && (defined(ESP32) || defined(ARDUINO_ARCH_ESP32))
  // Initialize platform adapters
  if (!wifi_) wifi_ = MakeWifi();
  if (!nvs_) nvs_ = MakeNvs();
  wifi_->persistent(false); // we manage creds in our own store
  wifi_->setModeOff();
  wifi_->setModeSta();
#endif

  std::string ssid, pass;
  bool have = loadCredentials(ssid, pass);
  if (have && !ssid.empty()) {
    enterConnecting_(ssid.c_str(), pass.c_str());
  } else {
    enterApMode_();
  }
}

void NetOnboarding::loop() {
#if defined(UNIT_TEST) || !defined(ARDUINO)
  if (st_.state == State::CONNECTING) {
    if (simulate_will_connect_) {
      if (simulate_connect_start_ms_ == 0) simulate_connect_start_ms_ = nowMs_();
      if (nowMs_() - simulate_connect_start_ms_ >= simulate_connect_delay_ms_) {
        enterConnected_();
      }
    }
  }
#endif

  if (st_.state == State::CONNECTING) {
    uint32_t now = nowMs_();
    if (now - connecting_since_ms_ >= connect_timeout_ms_) {
      enterApMode_();
      return;
    }

    if (wifi_ && wifi_->staConnected()) { enterConnected_(); return; }
  }

  if (st_.state == State::CONNECTED) {
    if (!(wifi_ && wifi_->staConnected())) {
      // simple reconnect attempt: drop to AP for now (spec allows backoff later)
      enterApMode_();
    }
  }
}

bool NetOnboarding::setCredentials(const char* ssid, const char* pass) {
  if (!ssid || !pass) return false;
  if (!saveCredentials(ssid, pass)) return false;
  enterConnecting_(ssid, pass);
  return true;
}

void NetOnboarding::resetCredentials() {
  clearCredentials();
  enterApMode_();
}

Status NetOnboarding::status() const { return st_; }

void NetOnboarding::apPassword(std::array<char,65>& out) const {
  const char* p = nullptr;
  if (wifi_) p = wifi_->apDefaultPassword();
  if (!p) p = "";
  std::snprintf(out.data(), out.size(), "%s", p);
}

int NetOnboarding::scanNetworks(std::vector<WifiScanResult>& out, int max_results, bool include_hidden) {
  if (!wifi_) wifi_ = MakeWifi();
  // Prefer AP+STA when AP is active so we don't drop the portal
  if (st_.state == State::AP_ACTIVE) {
    wifi_->setModeApSta();
  } else {
    wifi_->setModeSta();
  }
  return wifi_->scanNetworks(out, max_results, include_hidden);
}

// NVS persistence ----------------------------------------------------------

bool NetOnboarding::saveCredentials(const char* ssid, const char* pass) {
  if (!nvs_) nvs_ = MakeNvs();
  if (!nvs_->begin("net", false)) return false;
  bool ok1 = nvs_->putString("ssid", ssid ? ssid : "");
  bool ok2 = nvs_->putString("psk", pass ? pass : "");
  nvs_->end();
  if (ok1) { last_ssid_ = ssid ? ssid : ""; }
  return ok1 && ok2;
}

bool NetOnboarding::loadCredentials(std::string& out_ssid, std::string& out_pass) {
  if (!nvs_) nvs_ = MakeNvs();
  if (!nvs_->begin("net", true)) return false;
  out_ssid = nvs_->getString("ssid", "");
  out_pass = nvs_->getString("psk", "");
  nvs_->end();
  last_ssid_ = out_ssid;
  return !out_ssid.empty();
}

void NetOnboarding::clearCredentials() {
  if (!nvs_) nvs_ = MakeNvs();
  if (nvs_->begin("net", false)) {
    nvs_->remove("ssid");
    nvs_->remove("psk");
    nvs_->end();
  }
  last_ssid_.clear();
}

// Transitions --------------------------------------------------------------

void NetOnboarding::enterApMode_() {
  st_.state = State::AP_ACTIVE;
  st_.rssi_dbm = 0;
  std::snprintf(st_.ip.data(), st_.ip.size(), "0.0.0.0");
  st_.ssid[0] = '\0';

  if (wifi_) {
    // Ensure clean re-init path to avoid ESP errors when switching modes rapidly
    wifi_->setModeOff();
    wifi_->setModeAp();
    // Build SSID after AP mode is active (ensures MAC available)
    std::array<char, 32> ap;
    buildApSsid_(ap);
    std::snprintf(st_.ssid.data(), st_.ssid.size(), "%s", ap.data());
    wifi_->softApStart(ap.data(), wifi_->apDefaultPassword());
    wifi_->softApIp(st_.ip);
  }
}

void NetOnboarding::enterConnecting_(const char* ssid, const char* pass) {
  (void)ssid; (void)pass;
  st_.state = State::CONNECTING;
  st_.rssi_dbm = 0;
  std::snprintf(st_.ip.data(), st_.ip.size(), "0.0.0.0");
  // Track requested SSID
  if (ssid) last_ssid_ = ssid; else last_ssid_.clear();
  std::snprintf(st_.ssid.data(), st_.ssid.size(), "%s", last_ssid_.c_str());
  connecting_since_ms_ = nowMs_();

#if defined(UNIT_TEST) || !defined(ARDUINO)
  simulate_connect_start_ms_ = nowMs_();
#endif

  if (wifi_) {
    wifi_->setModeOff();
    wifi_->setModeSta();
    wifi_->disconnect(true);
#if defined(ARDUINO)
    delay(20);
#endif
    wifi_->beginSta(ssid, pass);
  }
}

void NetOnboarding::enterConnected_() {
  st_.state = State::CONNECTED;
  st_.rssi_dbm = 0;
  std::snprintf(st_.ip.data(), st_.ip.size(), "0.0.0.0");
  std::snprintf(st_.ssid.data(), st_.ssid.size(), "%s", last_ssid_.c_str());
  if (wifi_) {
    st_.rssi_dbm = wifi_->staRssi();
    wifi_->staLocalIp(st_.ip);
    // stop AP if any (implicit when STA only)
  }
}

void NetOnboarding::buildApSsid_(std::array<char, 32>& out) const {
  std::snprintf(out.data(), out.size(), "SOFTAP");
  if (wifi_) {
    uint8_t mac[6] = {0};
    wifi_->macAddress(mac);
    const char* prefix = wifi_->apSsidPrefix();
    std::snprintf(out.data(), out.size(), "%s%02X%02X%02X", prefix ? prefix : "AP-",
                  mac[3], mac[4], mac[5]);
  }
}

} // namespace net_onboarding
