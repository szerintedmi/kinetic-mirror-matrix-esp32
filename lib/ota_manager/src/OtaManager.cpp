#include "ota/OtaManager.h"

#if defined(ARDUINO) && (defined(ESP32) || defined(ARDUINO_ARCH_ESP32))

#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
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

// ---------------------------------------------------------------------------
// HTTP OTA endpoint: POST /api/ota?type=firmware|filesystem
// ---------------------------------------------------------------------------

static void sendOtaJson_(AsyncWebServerRequest* request, int status, const char* ota_status,
                         const char* message) {
  JsonDocument doc;
  doc["status"] = ota_status;
  doc["message"] = message;
  std::string out;
  serializeJson(doc, out);
  request->send(status, "application/json", out.c_str());
}

/// Per-request state stored in request->_tempObject during chunked upload.
struct HttpOtaState {
  bool begun{false};
  bool failed{false};
  String error_msg;
};

void registerHttpOtaRoute(AsyncWebServer& server, const char* password, OtaManager& mgr) {
  // Capture OtaManager pointer for use in lambdas
  static OtaManager* s_mgr = &mgr;
  static const char* s_password = password;

  server.on(
      "/api/ota",
      HTTP_POST,
      // --- Response handler (called after all body received) ---
      [](AsyncWebServerRequest* request) {
        auto* state = reinterpret_cast<HttpOtaState*>(request->_tempObject);
        if (!state) {
          return;
        }

        bool ok = state->begun && !state->failed && !Update.hasError();
        String error = state->error_msg;
        delete state;
        request->_tempObject = nullptr;

        if (ok) {
          s_mgr->updating_ = false;
          s_mgr->emitState_("complete", "rebooting");
          Serial.println("\nHTTP OTA Complete - Rebooting...");
          sendOtaJson_(request, 200, "ok", "Update complete, rebooting");
          delay(500);
          ESP.restart();
        } else {
          s_mgr->updating_ = false;
          s_mgr->emitState_("error", error.c_str());
          Serial.printf("\nHTTP OTA Error: %s\n", error.c_str());
          sendOtaJson_(request, 400, "error", error.c_str());
        }
      },
      nullptr, // no file upload handler
      // --- Body handler (called per chunk) ---
      [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
        auto** state_ptr = reinterpret_cast<HttpOtaState**>(&request->_tempObject);

        if (index == 0) {
          // --- First chunk: authenticate and begin update ---

          // Allocate state early so subsequent chunks safely no-op on failure
          auto* state = new HttpOtaState();
          *state_ptr = state;

          // Check password
          if (!request->hasHeader("X-OTA-Password") ||
              request->header("X-OTA-Password") != String(s_password)) {
            state->failed = true;
            sendOtaJson_(request, 403, "error", "Forbidden");
            return;
          }

          // Check for concurrent OTA
          if (s_mgr->isUpdating()) {
            state->failed = true;
            sendOtaJson_(request, 409, "error", "OTA already in progress");
            return;
          }

          // Parse update type from query parameter
          int cmd = U_FLASH;
          if (request->hasArg("type") && request->arg("type") == "filesystem") {
            cmd = U_SPIFFS;
          }

          // Block motors and other OTA attempts
          s_mgr->updating_ = true;
          s_mgr->last_error_.clear();
          s_mgr->last_progress_ms_ = 0;

          const char* type_str = (cmd == U_FLASH) ? "firmware" : "filesystem";
          Serial.printf("HTTP OTA Start: %s (%u bytes)\n", type_str, (unsigned)total);
          s_mgr->emitState_("start", type_str);

          if (!Update.begin(total, cmd)) {
            state->error_msg = Update.errorString();
            state->failed = true;
            s_mgr->updating_ = false;
            s_mgr->emitState_("error", state->error_msg.c_str());
            Serial.printf("HTTP OTA begin failed: %s\n", state->error_msg.c_str());
            return;
          }

          state->begun = true;
        }

        auto* state = *state_ptr;
        if (!state || state->failed) {
          return;
        }

        // --- Write chunk to flash ---
        if (Update.write(data, len) != len) {
          state->error_msg = Update.errorString();
          state->failed = true;
          Update.abort();
          return;
        }

        // --- Progress ---
        uint32_t now = millis();
        if (now - s_mgr->last_progress_ms_ >= 500) {
          s_mgr->last_progress_ms_ = now;
          uint8_t percent = static_cast<uint8_t>(((index + len) * 100) / total);
          Serial.printf("HTTP OTA Progress: %u%%\r", percent);
          s_mgr->emitProgress_(percent, total);
        }

        // --- Last chunk: finalize ---
        if (index + len == total) {
          if (!Update.end(true)) {
            state->error_msg = Update.errorString();
            state->failed = true;
          }
        }
      });
}

}  // namespace ota

#endif  // ARDUINO && ESP32
