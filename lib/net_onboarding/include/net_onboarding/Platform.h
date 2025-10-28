#pragma once

#include <stdint.h>
#include <string>
#include <array>
#include <memory>

namespace net_onboarding {

class IWifi {
public:
  virtual ~IWifi() = default;
  virtual void persistent(bool enable) = 0;
  virtual void setModeOff() = 0;
  virtual void setModeSta() = 0;
  virtual void setModeAp() = 0;
  virtual void disconnect(bool erase) = 0;
  virtual void beginSta(const char* ssid, const char* pass) = 0;
  virtual bool staConnected() const = 0;
  virtual int staRssi() const = 0;
  virtual void staLocalIp(std::array<char,16>& out) const = 0;
  virtual void softApStart(const char* ssid, const char* pass) = 0;
  virtual void softApIp(std::array<char,16>& out) const = 0;
  virtual void macAddress(uint8_t mac[6]) const = 0;
  virtual std::string softApSsid() const = 0;
  virtual const char* apDefaultPassword() const = 0;
  virtual const char* apSsidPrefix() const = 0;
};

class INvs {
public:
  virtual ~INvs() = default;
  virtual bool begin(const char* ns, bool readOnly) = 0;
  virtual bool putString(const char* key, const char* value) = 0;
  virtual std::string getString(const char* key, const char* defaultVal) = 0;
  virtual void remove(const char* key) = 0;
  virtual void end() = 0;
};

// Factory helpers; implemented per-platform
std::unique_ptr<IWifi> MakeWifi();
std::unique_ptr<INvs> MakeNvs();

#if defined(USE_STUB_BACKEND)
// Test-only helper to configure the stub Wi‑Fi connection behavior
void ConfigureStubWifi(bool will_connect, uint32_t delay_ms);
#endif

} // namespace net_onboarding
