#pragma once

#if defined(ARDUINO) && (defined(ESP32) || defined(ARDUINO_ARCH_ESP32))

#include <Arduino.h>

#include <cstdint>
#include <functional>

class AsyncWebServer;

namespace ota {

/// Callback for OTA progress updates (progress 0-100, total bytes)
using ProgressCallback = std::function<void(uint8_t progress, size_t total)>;

/// Callback for OTA state changes
using StateCallback = std::function<void(const char* state, const char* message)>;

/// OTA Manager wrapping ArduinoOTA with progress callbacks and version info
class OtaManager {
public:
  /// Initialize OTA with hostname and password
  /// Call after WiFi is available (works in both AP and STA modes)
  void begin(const char* hostname, const char* password);

  /// Non-blocking maintenance - call from loop()
  void loop();

  /// Version info (injected at build time)
  const char* commitHash() const;
  const char* commitDate() const;

  /// Check if OTA update is in progress
  bool isUpdating() const { return updating_; }

  /// Get last error message (empty if no error)
  const String& lastError() const { return last_error_; }

  /// Set callback for progress updates
  void onProgress(ProgressCallback callback) { progress_cb_ = callback; }

  /// Set callback for state changes (start, end, error)
  void onState(StateCallback callback) { state_cb_ = callback; }

  /// Mark current firmware as valid (call after successful boot)
  /// Prevents auto-rollback to previous firmware
  void markFirmwareValid();

private:
  void setupCallbacks_();
  void emitState_(const char* state, const char* message);
  void emitProgress_(uint8_t progress, size_t total);

  bool initialized_{false};
  bool updating_{false};
  bool restart_pending_{false};
  uint32_t restart_at_ms_{0};
  String last_error_;
  uint32_t last_progress_ms_{0};
  ProgressCallback progress_cb_;
  StateCallback state_cb_;

  friend void registerHttpOtaRoute(AsyncWebServer& server, const char* password, OtaManager& mgr);
};

/// Register HTTP OTA endpoint (POST /api/ota) on existing AsyncWebServer.
/// Call from setup after both Net().begin() and Ota().begin().
void registerHttpOtaRoute(AsyncWebServer& server, const char* password, OtaManager& mgr);

}  // namespace ota

#endif  // ARDUINO && ESP32
