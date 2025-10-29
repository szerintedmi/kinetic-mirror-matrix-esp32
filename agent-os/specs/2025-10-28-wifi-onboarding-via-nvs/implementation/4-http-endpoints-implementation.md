# Task 4: HTTP endpoints

## Overview
**Task Reference:** Task #4 from `agent-os/specs/2025-10-28-wifi-onboarding-via-nvs/tasks.md`
**Implemented By:** api-engineer
**Date:** 2025-10-28
**Status:** ✅ Complete

### Task Description
Serve gzipped static portal assets from LittleFS and expose `/api/status`, `/api/scan`, `/api/wifi`, and `/api/reset` endpoints that mirror the serial NET command set. `/api/scan` is limited to SoftAP mode and returns HTTP 409 otherwise.

## Implementation Summary
Embedded a lightweight HTTP server inside `NetOnboarding` (Arduino-only) so the library can mount LittleFS, stream gzipped portal assets, and deliver a JSON API for onboarding. The server spins up automatically during `begin()` and handles static fallbacks via `onNotFound`, keeping the entry point unaware of route details. API handlers reuse the library’s state and validation logic to provide consistent error semantics with the serial path, enforce the “CONNECTING → busy” rule, and now surface device identity (MAC + SoftAP SSID/password) for STA/SoftAP UX parity.

Static assets live under `/net/` in LittleFS (mirror of `data_src/net`) and are served with proper `Content-Type`, `Content-Encoding`, and caching headers. JSON responses set `Cache-Control: no-store` to avoid stale data inside captive portal flows. For native/unit-test builds the server helpers collapse to no-ops so existing tests compile unchanged.

## Files Changed/Created

### Modified Files
- `lib/net_onboarding/src/NetOnboarding.cpp` – Added WebServer/LittleFS integration, JSON helpers, request handlers, and static asset streaming.
- `lib/net_onboarding/include/net_onboarding/NetOnboarding.h` – No public API changes beyond LED wiring (Task 3) but underlying members support the portal lifecycle.

## Key Implementation Details

### WebServer bootstrap and LittleFS mounting
**Location:** `lib/net_onboarding/src/NetOnboarding.cpp`

Introduced `ensurePortalServer_` (Arduino only) that lazily mounts LittleFS, registers root/API routes, and begins the `WebServer`. The helper is called during `NetOnboarding::begin()` so hardware sketches don’t need additional setup. For stub builds, an empty stub keeps unit tests unaffected.

**Rationale:** Concentrates onboarding transport logic beside state management, preventing duplication across sketches and future MQTT adapters.

### Static asset streaming with gzip support
**Location:** `lib/net_onboarding/src/NetOnboarding.cpp`

`serveStaticPath_` resolves requested URIs to `/net/<path>` in LittleFS, preferring `.gz` files, applying the correct `Content-Type`, and emitting `Content-Encoding: gzip` when applicable. Missing assets fall back to a simple 404.

**Rationale:** Keeps the portal payload small while honouring the standards requirement to serve gzipped content from flash.

### JSON API handlers
**Location:** `lib/net_onboarding/src/NetOnboarding.cpp`

Implemented dedicated handlers:
- `/api/status` – Mirrors `NetOnboarding::status()`, including RSSI, IPv4, current STA SSID, device MAC, SoftAP SSID, and SoftAP password (used by UI even when STA-connected).
- `/api/scan` – When the device is in SoftAP mode, collects scan results, orders by RSSI, and emits compact JSON. Returns `409 {"error":"scan-ap-only"}` in STA/CONNECTING states.
- `/api/wifi` – Accepts a JSON body, validates SSID/PSK lengths, enforces busy semantics, and reuses `setCredentials`.
- `/api/reset` – Clears stored credentials, re-enters SoftAP, and returns the new SoftAP SSID/password payload for the UI.

Simple helper functions (`jsonQuote_`, `parseJsonStringField_`) keep dependencies minimal and respect quoted inputs.

**Rationale:** Aligns HTTP responses with the serial command surface so client tools can switch transports without re-learning semantics.

## Database Changes (if applicable)

N/A – no storage schema changes beyond existing NVS routines.

## Dependencies (if applicable)

### New Dependencies Added

None (uses Arduino core `WebServer` and `LittleFS`, already available in the ESP32 toolchain).

### Configuration Changes

None.

## Testing

### Test Files Created/Updated

No automated HTTP tests (WebServer absent in native builds). Manual verification scripted under `planning/verification/2025-10-28-wifi-onboarding-manual.md` (Scenario D) using `curl` once hardware is available.

### Test Coverage
- Unit tests: ❌ None (HTTP stack Arduino-only)
- Integration tests: ❌ None
- Edge cases covered: Validation paths handled via serial-CLI test suite (shared logic).

### Manual Testing Performed
Pending hardware run; curl checklist documented for the bench session.

## User Standards & Preferences Compliance

### Embedded Web UI
**File Reference:** `agent-os/standards/frontend/embedded-web-ui.md`

**How Your Implementation Complies:** Delivers tiny JSON payloads, serves gzipped static assets from LittleFS via the existing gzip script, and keeps HTTP semantics aligned with serial verbs.

**Deviations (if any):** None.

### Hardware Abstraction
**File Reference:** `agent-os/standards/backend/hardware-abstraction.md`

**How Your Implementation Complies:** All server lifecycle code sits in the onboarding library; sketches simply call `Net().loop()`, keeping transport concerns centralized.

**Deviations (if any):** None.

### Unit Testing
**File Reference:** `agent-os/standards/testing/unit-testing.md`

**How Your Implementation Complies:** Shared validation logic remains covered by existing serial tests; HTTP-specific flows are scheduled for manual validation per the standard when host-only automation is infeasible.

**Deviations (if any):** Automated HTTP tests deferred due to Arduino-only dependencies (documented and tracked in verification checklist).

## Integration Points (if applicable)

### APIs/Endpoints
- `GET /api/status` – Returns `state`, `ssid`, `ip`, `rssi`, `mac`, `apSsid`, `apPassword`.
- `GET /api/scan` – When SoftAP is active returns array of `{ssid,rssi,secure,channel}`; otherwise responds `409 {"error":"scan-ap-only"}`.
- `POST /api/wifi` – Accepts JSON credentials, responds with `state` or error codes (`busy`, `invalid-ssid`, `pass-too-short`, `persist-failed`).
- `POST /api/reset` – Clears stored credentials, re-enables SoftAP, and responds with `{apSsid, apPassword}`.

## Known Issues & Limitations

- Manual validation still required to verify captive portal behaviour and Wi-Fi scan latency on hardware.

## Performance Considerations

Handlers run synchronously on core 1 but keep payloads small and leverage streaming to avoid extra copies. Static gzip assets reduce SoftAP transfer time.

## Security Considerations

Routes expose the SoftAP password (already public) and accept credentials without authentication; aligns with prototype requirements but should be reviewed before exposure on shared networks.

## Dependencies for Other Tasks

Task 5 (portal UI) consumes these endpoints; Task 7 references them in user-facing documentation.

## Notes

- If future builds need authentication, wrap the handlers here so both portal and serial surfaces remain in sync.
