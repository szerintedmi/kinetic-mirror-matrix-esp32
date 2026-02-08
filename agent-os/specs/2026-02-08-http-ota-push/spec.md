# Specification: HTTP OTA Push

## Goal

Add an HTTP-based firmware upload endpoint to the ESP32 web server so the deploy tool can push firmware directly to devices via an outbound HTTP POST, eliminating the reverse-connection requirement of espota.py that is blocked by managed firewalls. Keep espota.py as a fallback for environments where it still works.

## Motivation

The current OTA deployment uses espota.py, which requires the ESP32 to connect *back* to the host machine on a random port. This fails on managed Macs (and corporate networks) where the firewall blocks all incoming connections (macOS firewall State=2). An HTTP push model inverts the flow: the host initiates an outbound connection to the ESP32's existing web server on port 80 — no incoming connections needed.

## User Stories

- As an operator on a managed Mac, I can deploy firmware to devices via `ota_deploy` without firewall workarounds, because the upload is a standard outbound HTTP POST.
- As an operator, I can fall back to the legacy espota method via `--method espota` when HTTP push is unavailable (e.g., devices running older firmware).
- As a developer, I can deploy both firmware and filesystem images via the HTTP endpoint using the same deploy tool workflow.

## Core Requirements

### Functional Requirements

#### 1. Firmware HTTP Endpoint (ESP32)

- Add `POST /api/ota` to the existing `AsyncWebServer` on port 80.
- Authentication: require `X-OTA-Password` header matching `OTA_PASSWORD` from `secrets.h`. Reject with `403 Forbidden` and JSON error if missing or wrong.
- Accept `application/octet-stream` body containing the raw firmware binary.
- Query parameter `type=firmware` (default) or `type=filesystem` to select update target (`U_FLASH` or `U_SPIFFS`).
- Use the ESP32 `Update` library (`Update.begin()` / `Update.write()` / `Update.end()`) to apply the image. This library is already linked via ArduinoOTA.
- Reject upload if `OtaManager::isUpdating()` returns true (espota upload already in progress). Return `409 Conflict`.
- Set `OtaManager::updating_` to true during HTTP upload so ArduinoOTA also rejects concurrent attempts.
- On success: respond with `200 OK` and `{"status": "ok", "message": "Update complete, rebooting"}`, then reboot after a 500ms delay (allow response to flush).
- On failure: respond with `400 Bad Request` and `{"status": "error", "message": "<Update library error>"}`.
- Use `Update.onProgress()` callback to feed progress to `OtaManager`'s existing progress callback chain (serial logging, MQTT state events).
- Maximum upload size: validate against partition size via `Update.begin(contentLength, type)` return value.
- Block motor commands during OTA by setting the updating flag before `Update.begin()`.

#### 2. Route Registration

- Add a public free function in `OtaManager`:
  ```cpp
  void registerHttpOtaRoute(AsyncWebServer& server, const char* password);
  ```
- Call this from `NetOnboarding::ensurePortalServer_()` after existing route registration, passing `g_portal_server` and `OTA_PASSWORD`.
- This keeps OTA logic in the `ota_manager` library while reusing the existing web server instance without exposing it globally.

#### 3. Deploy Tool — HTTP Upload Method (Python)

- Add `upload_http()` async method to `PioWrapper` alongside existing `upload()`:
  ```python
  async def upload_http(self, ip: str, log_file: Path, firmware_path: Path) -> AsyncIterator[int]:
  ```
- Implementation: use `aiohttp` to `POST` firmware binary to `http://{ip}/api/ota` with:
  - Header: `X-OTA-Password: <password>`
  - Header: `Content-Type: application/octet-stream`
  - Query: `?type=firmware` (or `?type=filesystem`)
  - Stream the file body with chunked reads for progress tracking.
- Yield progress percentage based on bytes sent vs total file size.
- Add `upload_filesystem_http()` using the same endpoint with `?type=filesystem`.
- On non-200 response: parse JSON error body and raise `RuntimeError` with the message.

#### 4. Deploy Tool — Method Selection

- Add `--method` CLI argument to `ota_deploy.py`:
  - `http` (default): use HTTP push via `/api/ota`.
  - `espota`: use legacy espota.py reverse-connection method.
- Add `method` field to `ota_devices.toml` under `[ota]`:
  ```toml
  [ota]
  password = "kinetic-mirror-ota"
  method = "http"  # "http" (default) or "espota"
  ```
- CLI `--method` flag overrides the config file value.
- `OtaDeployer` selects `PioWrapper.upload_http()` or `PioWrapper.upload()` based on method.
- Keep all existing espota code paths unchanged.

#### 5. Backward Compatibility

- ArduinoOTA (espota) continues to run in `Ota().loop()` — both OTA methods coexist.
- Devices running older firmware (without `/api/ota`) can still be updated via `--method espota`.
- The HTTP endpoint is additive; no existing behavior is changed or removed.

### Non-Functional Requirements

- HTTP upload should complete within the same time envelope as espota (~30s for a 1.4MB firmware image on LAN).
- The endpoint must handle the full firmware image (~1.4MB) without exhausting heap. `AsyncWebServer` handles chunked upload natively; the handler writes chunks directly to flash via `Update.write()`.
- No new firmware dependencies. `Update.h` and `AsyncWebServer` are already linked.
- Python side: `aiohttp` is already a dependency in the project's Poetry environment. If not, add it.

## Technical Approach

### Firmware Side

- **File**: `lib/ota_manager/src/OtaManager.cpp` — add `registerHttpOtaRoute()` free function.
- **Handler**: Register on the `AsyncWebServer` using `.on()` with `HTTP_POST`, body handler for streaming chunks.
- **Upload handler pattern** for `ESPAsyncWebServer`:
  ```cpp
  server.on("/api/ota", HTTP_POST,
    // Response handler (called after all body received)
    [](AsyncWebServerRequest* request) { ... },
    // File upload handler (not used)
    nullptr,
    // Body handler (called per chunk)
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      if (index == 0) {
        // First chunk: validate auth, call Update.begin()
      }
      Update.write(data, len);
      if (index + len == total) {
        // Last chunk: call Update.end()
      }
    }
  );
  ```
- **Wiring**: In `NetOnboarding.cpp`'s `ensurePortalServer_()`, add:
  ```cpp
  #include "ota/OtaManager.h"
  ota::registerHttpOtaRoute(g_portal_server, OTA_PASSWORD);
  ```
  This requires `NetOnboarding.cpp` to include `secrets.h` for `OTA_PASSWORD` (it already includes other secret defines for AP credentials).

### Deploy Tool Side

- **File**: `tools/deploy/pio_wrapper.py` — add `upload_http()` and `upload_filesystem_http()`.
- **File**: `tools/deploy/ota_deploy.py` — add `--method` arg, route to correct upload method.
- **File**: `tools/deploy/ota_devices.toml` — add `method = "http"` to `[ota]` section.
- **Progress**: Calculate from `bytes_sent / total_bytes * 100`, yielded via the same async generator pattern as `upload()`.

### Testing

- **Firmware**: Verify endpoint exists by checking `GET /api/ota` returns `405 Method Not Allowed` (only POST accepted).
- **Deploy tool**: Add Python tests for `upload_http()` with mocked `aiohttp` responses (success, auth failure, conflict, server error).
- **Integration**: Deploy to a test device via HTTP, verify via existing `/api/status` check that firmware version matches.

## Out of Scope

- HTTPS/TLS on the ESP32 (same security model as existing ArduinoOTA — password auth over local network).
- OTA progress streaming from ESP32 back to the deploy tool during upload (progress is estimated client-side from bytes sent; post-upload verification confirms success).
- Auto-detection of device firmware version to choose method automatically.
- Rollback on failed HTTP OTA (the existing `esp_ota_mark_app_valid_cancel_rollback()` mechanism handles this).

## Success Criteria

- `poetry run python -m tools.deploy.ota_deploy --device <IP>` successfully uploads firmware via HTTP POST on a Mac with "Block all incoming connections" firewall enabled.
- `poetry run python -m tools.deploy.ota_deploy --method espota --device <IP>` continues to work via the legacy espota path on hosts without firewall restrictions.
- Firmware and filesystem uploads both work via the HTTP endpoint.
- Deployment speed is comparable to espota (~30s for firmware on LAN).
- Existing ArduinoOTA (mDNS discovery, espota protocol) continues to function alongside the new HTTP endpoint.
