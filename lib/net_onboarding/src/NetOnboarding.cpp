#include "net_onboarding/NetOnboarding.h"

#if defined(ARDUINO)
#include <Arduino.h>
#endif

#include <cstdio>
#include <cstring>
#include <chrono>
#include <cctype>
#include <string>
#include <vector>

#if defined(ARDUINO) && (defined(ESP32) || defined(ARDUINO_ARCH_ESP32))
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#endif

namespace net_onboarding
{

#if defined(ARDUINO) && (defined(ESP32) || defined(ARDUINO_ARCH_ESP32))

  static AsyncWebServer g_portal_server(80);
  static bool g_portal_server_started = false;
  static bool g_portal_fs_ok = false;
  static NetOnboarding *g_portal_net = nullptr;

  static const char *stateToString_(State s)
  {
    switch (s)
    {
    case State::AP_ACTIVE:
      return "AP_ACTIVE";
    case State::CONNECTING:
      return "CONNECTING";
    case State::CONNECTED:
      return "CONNECTED";
    default:
      return "UNKNOWN";
    }
  }

  static void sendJsonError_(AsyncWebServerRequest *request, int status, const char *code)
  {
    JsonDocument doc;
    doc["error"] = code ? code : "unknown";
    std::string out;
    serializeJson(doc, out);
    request->send(status, "application/json", out.c_str());
  }

  static void handleApiStatus_(AsyncWebServerRequest *request)
  {
    if (!g_portal_net)
    {
      sendJsonError_(request, 503, "net-unavailable");
      return;
    }
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    Status st = g_portal_net->status();
    root["state"] = stateToString_(st.state);
    root["ssid"] = st.ssid.data();
    root["ip"] = st.ip.data();
    if (st.state == State::CONNECTED)
    {
      root["rssi"] = st.rssi_dbm;
    }
    else
    {
      root["rssi"] = nullptr;
    }
    root["mac"] = st.mac.data();
    root["apSsid"] = st.ap_ssid.data();
    std::array<char, 65> ap_pass{};
    g_portal_net->apPassword(ap_pass);
    root["apPassword"] = ap_pass.data();
    std::string out;
    serializeJson(doc, out);
    request->send(200, "application/json", out.c_str());
  }

  static void handleApiScan_(AsyncWebServerRequest *request)
  {
    if (!g_portal_net)
    {
      sendJsonError_(request, 503, "net-unavailable");
      return;
    }
    if (g_portal_net->status().state != State::AP_ACTIVE)
    {
      sendJsonError_(request, 409, "scan-ap-only");
      return;
    }
    std::vector<WifiScanResult> results;
    g_portal_net->scanNetworks(results);
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (const auto &r : results)
    {
      JsonObject item = arr.add<JsonObject>();
      item["ssid"] = r.ssid.c_str();
      item["rssi"] = r.rssi;
      item["secure"] = r.secure;
      item["channel"] = r.channel;
    }
    std::string out;
    serializeJson(doc, out);
    request->send(200, "application/json", out.c_str());
  }

  static void handleApiWifiLogic_(AsyncWebServerRequest *request, const String &ssid, const String &pass)
  {
    if (!g_portal_net)
    {
      sendJsonError_(request, 503, "net-unavailable");
      return;
    }
    if (g_portal_net->status().state == State::CONNECTING)
    {
      sendJsonError_(request, 409, "busy");
      return;
    }
    if (ssid.length() == 0 || ssid.length() > 32)
    {
      sendJsonError_(request, 400, "invalid-ssid");
      return;
    }
    if (pass.length() > 0 && (pass.length() < 8 || pass.length() > 63))
    {
      if (pass.length() < 8)
      {
        sendJsonError_(request, 400, "pass-too-short");
      }
      else
      {
        sendJsonError_(request, 400, "invalid-pass");
      }
      return;
    }
    if (!g_portal_net->setCredentials(ssid.c_str(), pass.c_str()))
    {
      sendJsonError_(request, 500, "persist-failed");
      return;
    }
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    root["status"] = "connecting";
    root["state"] = stateToString_(State::CONNECTING);
    std::string out;
    serializeJson(doc, out);
    request->send(200, "application/json", out.c_str());
  }

  static void registerWifiHandler_(AsyncWebServer &server)
  {
    server.on("/api/wifi", HTTP_POST,
              [](AsyncWebServerRequest *request) {
                // The actual response will be sent after body parsed.
              },
              NULL,
              [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
                auto **buf_ptr = reinterpret_cast<std::string **>(&request->_tempObject);
                if (index == 0) {
                  // first chunk
                  *buf_ptr = new std::string();
                  (*buf_ptr)->reserve(total);
                }
                if (*buf_ptr) {
                  (*buf_ptr)->append(reinterpret_cast<const char *>(data), len);
                }
                if (index + len == total) {
                  String ssid;
                  String pass;
                  bool ok = false;
                  if (*buf_ptr) {
                    JsonDocument doc;
                    auto err = deserializeJson(doc, **buf_ptr);
                    if (!err && doc.is<JsonObject>()) {
                      JsonObject obj = doc.as<JsonObject>();
                      const char *ssid_c = obj["ssid"].as<const char *>();
                      if (ssid_c) {
                        ssid = String(ssid_c);
                        const char *pass_c = obj["pass"].as<const char *>();
                        if (pass_c) pass = String(pass_c);
                        ok = true;
                      }
                    }
                  }
                  delete *buf_ptr;
                  *buf_ptr = nullptr;
                  if (!ok) {
                    sendJsonError_(request, 400, "invalid-payload");
                    return;
                  }
                  handleApiWifiLogic_(request, ssid, pass);
                }
              });
  }

  static void handleApiReset_(AsyncWebServerRequest *request)
  {
    if (!g_portal_net)
    {
      sendJsonError_(request, 503, "net-unavailable");
      return;
    }
    if (g_portal_net->status().state == State::CONNECTING)
    {
      sendJsonError_(request, 409, "busy");
      return;
    }
    g_portal_net->resetCredentials();
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    root["status"] = "reset";
    std::array<char, 32> ap_ssid{};
    g_portal_net->softApSsid(ap_ssid);
    root["apSsid"] = ap_ssid.data();
    std::array<char, 65> ap_pass{};
    g_portal_net->apPassword(ap_pass);
    root["apPassword"] = ap_pass.data();
    std::string out;
    serializeJson(doc, out);
    request->send(200, "application/json", out.c_str());
  }

  static void handleNotFound_(AsyncWebServerRequest *request)
  {
    String uri = request->url();
    if (uri.startsWith("/api/"))
    {
      sendJsonError_(request, 404, "not-found");
      return;
    }
    request->send(404, "text/plain", "Not Found");
  }

  static void ensurePortalServer_(NetOnboarding &net)
  {
    g_portal_net = &net;
    if (g_portal_server_started)
      return;
    if (!g_portal_fs_ok)
    {
      g_portal_fs_ok = LittleFS.begin(false);
      if (!g_portal_fs_ok)
      {
        Serial.println("LittleFS mount failed; portal assets unavailable");
      }
    }
    g_portal_server.on("/api/status", HTTP_GET, handleApiStatus_);
    g_portal_server.on("/api/scan", HTTP_GET, handleApiScan_);
    registerWifiHandler_(g_portal_server);
    g_portal_server.on("/api/reset", HTTP_POST, handleApiReset_);
    g_portal_server.onNotFound(handleNotFound_);
    auto &root_handler = g_portal_server.serveStatic("/", LittleFS, "/");
    root_handler.setDefaultFile("net/index.html");
    root_handler.setCacheControl("public, max-age=300");
    auto &css_handler = g_portal_server.serveStatic("/css/", LittleFS, "/css/");
    css_handler.setCacheControl("public, max-age=300");
    auto &js_handler = g_portal_server.serveStatic("/js/", LittleFS, "/js/");
    js_handler.setCacheControl("public, max-age=300");
    auto &favicon_handler = g_portal_server.serveStatic("/favicon.ico", LittleFS, "/favicon.ico");
    favicon_handler.setCacheControl("public, max-age=86400");
    g_portal_server_started = true;
    g_portal_server.begin();
  }

#else

  static void ensurePortalServer_(NetOnboarding &) {}

#endif

  static void ip_to_str_(uint32_t ip, std::array<char, 16> &out)
  {
    unsigned b1 = (ip) & 0xFFu;
    unsigned b2 = (ip >> 8) & 0xFFu;
    unsigned b3 = (ip >> 16) & 0xFFu;
    unsigned b4 = (ip >> 24) & 0xFFu;
    // Arduino IPAddress::toString renders normal order; our bytes are little-endian depending on platform
    // Using WiFi.localIP() path on Arduino instead. This helper is used only for non-Arduino paths.
    std::snprintf(out.data(), out.size(), "%u.%u.%u.%u", b1, b2, b3, b4);
  }

  uint32_t NetOnboarding::nowMs_() const
  {
#if defined(ARDUINO)
    return millis();
#else
    using namespace std::chrono;
    static steady_clock::time_point t0 = steady_clock::now();
    return (uint32_t)duration_cast<milliseconds>(steady_clock::now() - t0).count();
#endif
  }

  void NetOnboarding::begin(uint32_t connect_timeout_ms)
  {
    connect_timeout_ms_ = connect_timeout_ms;

#if defined(ARDUINO) && (defined(ESP32) || defined(ARDUINO_ARCH_ESP32))
    if (!nvs_)
      nvs_ = MakeNvs();
#endif
    if (!wifi_)
      wifi_ = MakeWifi();
#if defined(ARDUINO) && (defined(ESP32) || defined(ARDUINO_ARCH_ESP32))
    wifi_->persistent(false); // we manage creds in our own store
#endif
    if (wifi_)
    {
      wifi_->setModeOff();
      wifi_->setModeSta();
    }

    std::string ssid, pass;
    bool have = loadCredentials(ssid, pass);
    if (have && !ssid.empty())
    {
      enterConnecting_(ssid.c_str(), pass.c_str());
    }
    else
    {
      enterApMode_();
    }

#if defined(ARDUINO) && (defined(ESP32) || defined(ARDUINO_ARCH_ESP32))
    ensurePortalServer_(*this);
#endif
  }

  void NetOnboarding::loop()
  {
#if defined(UNIT_TEST) || !defined(ARDUINO)
    if (st_.state == State::CONNECTING)
    {
      if (simulate_will_connect_)
      {
        if (simulate_connect_start_ms_ == 0)
          simulate_connect_start_ms_ = nowMs_();
        if (nowMs_() - simulate_connect_start_ms_ >= simulate_connect_delay_ms_)
        {
          enterConnected_();
        }
      }
    }
#endif

    if (st_.state == State::CONNECTING)
    {
      uint32_t now = nowMs_();
      if (now - connecting_since_ms_ >= connect_timeout_ms_)
      {
        enterApMode_();
        return;
      }

      if (wifi_ && wifi_->staConnected())
      {
        enterConnected_();
        return;
      }
    }

    if (st_.state == State::CONNECTED)
    {
#if !defined(USE_STUB_BACKEND)
      if (!(wifi_ && wifi_->staConnected()))
      {
        enterApMode_();
      }
#endif
    }

    updateLed_();
  }

  void NetOnboarding::configureStatusLed(int pin, bool active_low)
  {
    led_pin_ = pin;
    led_active_low_ = active_low;
    led_configured_ = (pin >= 0);
    led_pattern_ = LedPattern::OFF;
    led_logical_on_ = false;
    led_last_toggle_ms_ = nowMs_();
#if defined(ARDUINO)
    if (led_configured_)
    {
      pinMode(led_pin_, OUTPUT);
      applyLedState_(false);
    }
#endif
    refreshLedPattern_();
  }

  bool NetOnboarding::setCredentials(const char *ssid, const char *pass)
  {
    if (!ssid || !pass)
      return false;
    if (!saveCredentials(ssid, pass))
      return false;
    enterConnecting_(ssid, pass);
    return true;
  }

  void NetOnboarding::resetCredentials()
  {
    clearCredentials();
    enterApMode_();
  }

  Status NetOnboarding::status() const { return st_; }

  void NetOnboarding::apPassword(std::array<char, 65> &out) const
  {
    const char *p = nullptr;
    if (wifi_)
      p = wifi_->apDefaultPassword();
    if (!p)
      p = "";
    std::snprintf(out.data(), out.size(), "%s", p);
  }

  void NetOnboarding::deviceMac(std::array<char, 18> &out) const
  {
    std::snprintf(out.data(), out.size(), "%s", st_.mac.data());
  }

  void NetOnboarding::softApSsid(std::array<char, 32> &out) const
  {
    std::snprintf(out.data(), out.size(), "%s", st_.ap_ssid.data());
  }

  int NetOnboarding::scanNetworks(std::vector<WifiScanResult> &out, int max_results, bool include_hidden)
  {
    if (!wifi_)
      wifi_ = MakeWifi();
    if (st_.state != State::AP_ACTIVE)
    {
      out.clear();
      return 0;
    }
    // Prefer AP+STA to keep SoftAP responsive while scanning.
    wifi_->setModeApSta();
    return wifi_->scanNetworks(out, max_results, include_hidden);
  }

  // NVS persistence ----------------------------------------------------------

  bool NetOnboarding::saveCredentials(const char *ssid, const char *pass)
  {
    if (!nvs_)
      nvs_ = MakeNvs();
    if (!nvs_->begin("net", false))
      return false;
    bool ok1 = nvs_->putString("ssid", ssid ? ssid : "");
    bool ok2 = nvs_->putString("psk", pass ? pass : "");
    nvs_->end();
    if (ok1)
    {
      last_ssid_ = ssid ? ssid : "";
    }
    return ok1 && ok2;
  }

  bool NetOnboarding::loadCredentials(std::string &out_ssid, std::string &out_pass)
  {
    if (!nvs_)
      nvs_ = MakeNvs();
    if (!nvs_->begin("net", true))
      return false;
    out_ssid = nvs_->getString("ssid", "");
    out_pass = nvs_->getString("psk", "");
    nvs_->end();
    last_ssid_ = out_ssid;
    return !out_ssid.empty();
  }

  void NetOnboarding::clearCredentials()
  {
    if (!nvs_)
      nvs_ = MakeNvs();
    if (nvs_->begin("net", false))
    {
      nvs_->remove("ssid");
      nvs_->remove("psk");
      nvs_->end();
    }
    last_ssid_.clear();
  }

  // Transitions --------------------------------------------------------------

  void NetOnboarding::enterApMode_()
  {
    st_.state = State::AP_ACTIVE;
    st_.rssi_dbm = 0;
    std::snprintf(st_.ip.data(), st_.ip.size(), "0.0.0.0");
    st_.ssid[0] = '\0';

    if (wifi_)
    {
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

    updateIdentity_();
    refreshLedPattern_();
  }

  void NetOnboarding::enterConnecting_(const char *ssid, const char *pass)
  {
    (void)ssid;
    (void)pass;
    st_.state = State::CONNECTING;
    st_.rssi_dbm = 0;
    std::snprintf(st_.ip.data(), st_.ip.size(), "0.0.0.0");
    // Track requested SSID
    if (ssid)
      last_ssid_ = ssid;
    else
      last_ssid_.clear();
    std::snprintf(st_.ssid.data(), st_.ssid.size(), "%s", last_ssid_.c_str());
    connecting_since_ms_ = nowMs_();

#if defined(UNIT_TEST) || !defined(ARDUINO)
    simulate_connect_start_ms_ = nowMs_();
#endif

    if (wifi_)
    {
      wifi_->setModeOff();
      wifi_->setModeSta();
      wifi_->disconnect(true);
#if defined(ARDUINO)
      delay(20);
#endif
      wifi_->beginSta(ssid, pass);
    }

    updateIdentity_();
    refreshLedPattern_();
  }

  void NetOnboarding::enterConnected_()
  {
    st_.state = State::CONNECTED;
    st_.rssi_dbm = 0;
    std::snprintf(st_.ip.data(), st_.ip.size(), "0.0.0.0");
    std::snprintf(st_.ssid.data(), st_.ssid.size(), "%s", last_ssid_.c_str());
    if (wifi_)
    {
      st_.rssi_dbm = wifi_->staRssi();
      wifi_->staLocalIp(st_.ip);
      // stop AP if any (implicit when STA only)
    }

    updateIdentity_();
    refreshLedPattern_();
  }

  void NetOnboarding::buildApSsid_(std::array<char, 32> &out) const
  {
    std::snprintf(out.data(), out.size(), "SOFTAP");
    if (wifi_)
    {
      uint8_t mac[6] = {0};
      wifi_->macAddress(mac);
      const char *prefix = wifi_->apSsidPrefix();
      std::snprintf(out.data(), out.size(), "%s%02X%02X%02X", prefix ? prefix : "AP-",
                    mac[3], mac[4], mac[5]);
    }
  }

  void NetOnboarding::updateIdentity_()
  {
    if (!wifi_)
    {
      st_.mac[0] = '\0';
      st_.ap_ssid[0] = '\0';
      return;
    }
    uint8_t mac_bytes[6] = {0};
    wifi_->macAddress(mac_bytes);
    std::snprintf(st_.mac.data(), st_.mac.size(), "%02X:%02X:%02X:%02X:%02X:%02X",
                  mac_bytes[0], mac_bytes[1], mac_bytes[2],
                  mac_bytes[3], mac_bytes[4], mac_bytes[5]);
    std::array<char, 32> ap{};
    buildApSsid_(ap);
    std::snprintf(st_.ap_ssid.data(), st_.ap_ssid.size(), "%s", ap.data());
  }

  void NetOnboarding::refreshLedPattern_()
  {
    if (!led_configured_)
      return;
    LedPattern target = LedPattern::OFF;
    switch (st_.state)
    {
    case State::AP_ACTIVE:
      target = LedPattern::BLINK_FAST;
      break;
    case State::CONNECTING:
      target = LedPattern::BLINK_SLOW;
      break;
    case State::CONNECTED:
      target = LedPattern::SOLID;
      break;
    }
    if (target == led_pattern_)
      return;
    led_pattern_ = target;
    led_last_toggle_ms_ = nowMs_();
    if (led_pattern_ == LedPattern::OFF)
    {
      applyLedState_(false);
    }
    else if (led_pattern_ == LedPattern::SOLID)
    {
      applyLedState_(true);
    }
    else
    {
      applyLedState_(true); // start blink cycle in ON state
    }
  }

  void NetOnboarding::updateLed_()
  {
    if (!led_configured_)
      return;
    switch (led_pattern_)
    {
    case LedPattern::OFF:
      applyLedState_(false);
      return;
    case LedPattern::SOLID:
      applyLedState_(true);
      return;
    case LedPattern::BLINK_FAST:
    case LedPattern::BLINK_SLOW:
      break;
    }

    uint32_t interval = (led_pattern_ == LedPattern::BLINK_FAST) ? 125u : 400u;
    uint32_t now = nowMs_();
    if (now - led_last_toggle_ms_ >= interval)
    {
      led_last_toggle_ms_ = now;
      applyLedState_(!led_logical_on_);
    }
  }

  void NetOnboarding::applyLedState_(bool on)
  {
    led_logical_on_ = on;
#if defined(ARDUINO)
    if (!led_configured_)
      return;
    int level = HIGH;
    if (led_active_low_)
    {
      level = on ? LOW : HIGH;
    }
    else
    {
      level = on ? HIGH : LOW;
    }
    digitalWrite(led_pin_, level);
#else
    (void)on;
#endif
  }

} // namespace net_onboarding
