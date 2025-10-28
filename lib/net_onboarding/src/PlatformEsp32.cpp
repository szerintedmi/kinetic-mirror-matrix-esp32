#if !defined(USE_STUB_BACKEND) && defined(ARDUINO) && (defined(ESP32) || defined(ARDUINO_ARCH_ESP32))

#include "net_onboarding/Platform.h"
#include <WiFi.h>
#include <Preferences.h>
#include <cstdio>
#include "secrets.h" // SOFT_AP_SSID_PREFIX, SOFT_AP_PASS

namespace net_onboarding {

class Esp32Wifi : public IWifi {
public:
  void persistent(bool enable) override { WiFi.persistent(enable); }
  void setModeOff() override { WiFi.mode(WIFI_OFF); }
  void setModeSta() override { WiFi.mode(WIFI_STA); }
  void setModeAp() override { WiFi.mode(WIFI_AP); }
  void disconnect(bool erase) override { WiFi.disconnect(erase); }
  void beginSta(const char* ssid, const char* pass) override { WiFi.begin(ssid, pass); }
  bool staConnected() const override { return WiFi.status() == WL_CONNECTED; }
  int staRssi() const override { return WiFi.RSSI(); }
  void staLocalIp(std::array<char,16>& out) const override {
    auto ip = WiFi.localIP();
    std::snprintf(out.data(), out.size(), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  }
  void softApStart(const char* ssid, const char* pass) override { WiFi.softAP(ssid, pass); }
  void softApIp(std::array<char,16>& out) const override {
    auto ip = WiFi.softAPIP();
    std::snprintf(out.data(), out.size(), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  }
  void macAddress(uint8_t mac[6]) const override { WiFi.macAddress(mac); }
  std::string softApSsid() const override { return WiFi.softAPSSID().c_str(); }
  const char* apDefaultPassword() const override { return SOFT_AP_PASS; }
  const char* apSsidPrefix() const override { return SOFT_AP_SSID_PREFIX; }
};

class Esp32Nvs : public INvs {
public:
  bool begin(const char* ns, bool readOnly) override { return prefs_.begin(ns, readOnly); }
  bool putString(const char* key, const char* value) override { return prefs_.putString(key, value ? value : "") > 0; }
  std::string getString(const char* key, const char* def) override { return std::string(prefs_.getString(key, def ? def : "").c_str()); }
  void remove(const char* key) override { prefs_.remove(key); }
  void end() override { prefs_.end(); }
private:
  Preferences prefs_;
};

std::unique_ptr<IWifi> MakeWifi() { return std::unique_ptr<IWifi>(new Esp32Wifi()); }
std::unique_ptr<INvs> MakeNvs() { return std::unique_ptr<INvs>(new Esp32Nvs()); }

} // namespace net_onboarding

#endif // ARDUINO ESP32
