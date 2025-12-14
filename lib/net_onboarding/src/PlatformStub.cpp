#if defined(USE_STUB_BACKEND)

#include "net_onboarding/Platform.h"

#include <cstring>

namespace net_onboarding {

class StubWifi : public IWifi {
public:
  void persistent(bool) override {}
  void setModeOff() override {
    mode_ = 0;
  }
  void setModeSta() override {
    mode_ = 1;
  }
  void setModeAp() override {
    mode_ = 2;
  }
  void setModeApSta() override {
    mode_ = 3;
  }
  void disconnect(bool) override {
    connected_ = false;
  }
  void beginSta(const char*, const char*) override { /* connection simulated by NetOnboarding */ }
  bool staConnected() const override {
    return connected_;
  }
  int staRssi() const override {
    return -48;
  }
  void staLocalIp(std::array<char, 16>& out) const override {
    std::snprintf(out.data(), out.size(), "10.0.0.2");
  }
  void softApStart(const char* ssid, const char*) override {
    ssid_ = ssid ? ssid : "AP";
  }
  void softApIp(std::array<char, 16>& out) const override {
    std::snprintf(out.data(), out.size(), "192.168.4.1");
  }
  void macAddress(uint8_t mac[6]) const override {
    uint8_t m[6] = {0x02, 0x12, 0x34, 0x56, 0x78, 0x9A};
    std::memcpy(mac, m, 6);
  }
  std::string softApSsid() const override {
    return ssid_;
  }
  const char* apDefaultPassword() const override {
    return "stub-pass";
  }
  const char* apSsidPrefix() const override {
    return "Mirror-";
  }
  int scanNetworks(std::vector<WifiScanResult>& out,
                   int max_results,
                   bool /*include_hidden*/) override {
    out.clear();
    std::vector<WifiScanResult> tmp = {
        {"Home-2G", -42, 1, true},
        {"Cafe-WiFi", -70, 6, false},
        {"Office-IoT", -55, 11, true},
        {"Printer-Setup", -80, 3, false},
    };
    std::sort(tmp.begin(), tmp.end(), [](const WifiScanResult& a, const WifiScanResult& b) {
      return a.rssi > b.rssi;
    });
    if (max_results > 0 && (int)tmp.size() > max_results)
      tmp.resize((size_t)max_results);
    out = std::move(tmp);
    return (int)out.size();
  }

  // Test hook
  void setConnected(bool c) {
    connected_ = c;
  }

private:
  int mode_ = 0;  // 0=off,1=sta,2=ap
  bool connected_ = false;
  std::string ssid_ = "Mirror-TEST";
};

class StubNvs : public INvs {
public:
  bool begin(const char*, bool) override {
    return true;
  }
  bool putString(const char* key, const char* value) override {
    if (!key)
      return false;
    // Legacy keys (for migration testing)
    if (std::strcmp(key, "ssid") == 0) {
      std::strncpy(ssid_, value ? value : "", sizeof(ssid_) - 1);
      ssid_[sizeof(ssid_) - 1] = '\0';
      return true;
    }
    if (std::strcmp(key, "psk") == 0) {
      std::strncpy(psk_, value ? value : "", sizeof(psk_) - 1);
      psk_[sizeof(psk_) - 1] = '\0';
      return true;
    }
    // New multi-slot keys
    if (std::strcmp(key, "ssid_0") == 0) {
      std::strncpy(ssid_0_, value ? value : "", sizeof(ssid_0_) - 1);
      ssid_0_[sizeof(ssid_0_) - 1] = '\0';
      return true;
    }
    if (std::strcmp(key, "psk_0") == 0) {
      std::strncpy(psk_0_, value ? value : "", sizeof(psk_0_) - 1);
      psk_0_[sizeof(psk_0_) - 1] = '\0';
      return true;
    }
    if (std::strcmp(key, "ssid_1") == 0) {
      std::strncpy(ssid_1_, value ? value : "", sizeof(ssid_1_) - 1);
      ssid_1_[sizeof(ssid_1_) - 1] = '\0';
      return true;
    }
    if (std::strcmp(key, "psk_1") == 0) {
      std::strncpy(psk_1_, value ? value : "", sizeof(psk_1_) - 1);
      psk_1_[sizeof(psk_1_) - 1] = '\0';
      return true;
    }
    // Last connected slot
    if (std::strcmp(key, "last_slot") == 0) {
      last_slot_ = value ? (uint8_t)std::atoi(value) : 0;
      return true;
    }
    return false;
  }
  std::string getString(const char* key, const char* def) override {
    if (!key)
      return def ? def : "";
    // Legacy keys
    if (std::strcmp(key, "ssid") == 0)
      return std::string(ssid_);
    if (std::strcmp(key, "psk") == 0)
      return std::string(psk_);
    // New multi-slot keys
    if (std::strcmp(key, "ssid_0") == 0)
      return std::string(ssid_0_);
    if (std::strcmp(key, "psk_0") == 0)
      return std::string(psk_0_);
    if (std::strcmp(key, "ssid_1") == 0)
      return std::string(ssid_1_);
    if (std::strcmp(key, "psk_1") == 0)
      return std::string(psk_1_);
    // Last connected slot
    if (std::strcmp(key, "last_slot") == 0) {
      char buf[4];
      std::snprintf(buf, sizeof(buf), "%d", last_slot_);
      return std::string(buf);
    }
    return def ? def : "";
  }
  void remove(const char* key) override {
    if (!key)
      return;
    // Legacy keys
    if (std::strcmp(key, "ssid") == 0) {
      ssid_[0] = '\0';
    }
    if (std::strcmp(key, "psk") == 0) {
      psk_[0] = '\0';
    }
    // New multi-slot keys
    if (std::strcmp(key, "ssid_0") == 0) {
      ssid_0_[0] = '\0';
    }
    if (std::strcmp(key, "psk_0") == 0) {
      psk_0_[0] = '\0';
    }
    if (std::strcmp(key, "ssid_1") == 0) {
      ssid_1_[0] = '\0';
    }
    if (std::strcmp(key, "psk_1") == 0) {
      psk_1_[0] = '\0';
    }
    // Last connected slot
    if (std::strcmp(key, "last_slot") == 0) {
      last_slot_ = 0;
    }
  }
  void end() override {}

private:
  // Legacy keys (for migration testing)
  char ssid_[33] = {0};
  char psk_[65] = {0};
  // New multi-slot keys
  char ssid_0_[33] = {0};
  char psk_0_[65] = {0};
  char ssid_1_[33] = {0};
  char psk_1_[65] = {0};
  // Last connected slot
  uint8_t last_slot_ = 0;
};

std::unique_ptr<IWifi> MakeWifi() {
  return std::unique_ptr<IWifi>(new StubWifi());
}
std::unique_ptr<INvs> MakeNvs() {
  return std::unique_ptr<INvs>(new StubNvs());
}

// Optional test hook used by NetOnboarding::setTestSimulation; no-op here as
// NetOnboarding drives the CONNECTED transition in native tests.
void ConfigureStubWifi(bool, uint32_t) {}

}  // namespace net_onboarding

#endif  // !ARDUINO || UNIT_TEST
