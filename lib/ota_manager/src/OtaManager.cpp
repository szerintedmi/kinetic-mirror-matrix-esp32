#include "ota/OtaManager.h"

#if defined(ARDUINO) && (defined(ESP32) || defined(ARDUINO_ARCH_ESP32))

#include <ArduinoOTA.h>
#include <esp_ota_ops.h>

#include "version.h"

namespace ota {

void OtaManager::begin(const char* hostname, const char* password) {
  if (initialized_)
    return;

  // Validate hostname
  if (!hostname || hostname[0] == '\0') {
    Serial.println("ERROR: OTA hostname is required");
    return;
  }

  ArduinoOTA.setHostname(hostname);

  // Validate password - OTA without password is a security risk
  if (password && password[0] != '\0') {
    ArduinoOTA.setPassword(password);
  } else {
    Serial.println("WARNING: OTA started without password - INSECURE!");
  }

  // Enable mDNS for device discovery
  ArduinoOTA.setMdnsEnabled(true);

  setupCallbacks_();
  ArduinoOTA.begin();

  initialized_ = true;

  Serial.println("=== OTA Ready ===");
  Serial.printf("  Firmware: %s\n", commitHash());
  Serial.printf("  Commit:   %s\n", commitDate());
  Serial.printf("  Hostname: %s\n", hostname);
  Serial.println("=================");
}

void OtaManager::loop() {
  if (!initialized_)
    return;
  ArduinoOTA.handle();
}

const char* OtaManager::commitHash() const {
#ifdef GIT_COMMIT_HASH
  return GIT_COMMIT_HASH;
#else
  return "unknown";
#endif
}

const char* OtaManager::commitDate() const {
#ifdef GIT_COMMIT_DATE
  return GIT_COMMIT_DATE;
#else
  return "unknown";
#endif
}

void OtaManager::markFirmwareValid() {
  esp_ota_mark_app_valid_cancel_rollback();
}

void OtaManager::setupCallbacks_() {
  ArduinoOTA.onStart([this]() {
    updating_ = true;
    last_error_.clear();
    const char* type = (ArduinoOTA.getCommand() == U_FLASH) ? "firmware" : "filesystem";
    Serial.printf("OTA Start: %s\n", type);
    emitState_("start", type);
  });

  ArduinoOTA.onEnd([this]() {
    updating_ = false;
    Serial.println("\nOTA Complete - Rebooting...");
    emitState_("complete", "rebooting");
  });

  ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
    uint32_t now = millis();
    // Rate-limit progress updates to every 500ms to avoid flooding
    if (now - last_progress_ms_ >= 500) {
      last_progress_ms_ = now;
      uint8_t percent = static_cast<uint8_t>((progress * 100) / total);
      Serial.printf("OTA Progress: %u%%\r", percent);
      emitProgress_(percent, total);
    }
  });

  ArduinoOTA.onError([this](ota_error_t error) {
    updating_ = false;
    const char* msg = "unknown";
    switch (error) {
    case OTA_AUTH_ERROR:
      msg = "auth_failed";
      break;
    case OTA_BEGIN_ERROR:
      msg = "begin_failed";
      break;
    case OTA_CONNECT_ERROR:
      msg = "connect_failed";
      break;
    case OTA_RECEIVE_ERROR:
      msg = "receive_failed";
      break;
    case OTA_END_ERROR:
      msg = "end_failed";
      break;
    }
    last_error_ = msg;
    Serial.printf("\nOTA Error: %s\n", msg);
    emitState_("error", msg);
  });
}

void OtaManager::emitState_(const char* state, const char* message) {
  if (state_cb_) {
    state_cb_(state, message);
  }
}

void OtaManager::emitProgress_(uint8_t progress, size_t total) {
  if (progress_cb_) {
    progress_cb_(progress, total);
  }
}

}  // namespace ota

#endif  // ARDUINO && ESP32
