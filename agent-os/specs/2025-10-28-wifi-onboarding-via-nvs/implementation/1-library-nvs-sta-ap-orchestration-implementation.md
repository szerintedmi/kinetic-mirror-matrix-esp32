# Task 1: Library + NVS + STA/AP orchestration

## Overview
**Task Reference:** Task #1 from `agent-os/specs/2025-10-28-wifi-onboarding-via-nvs/tasks.md`
**Implemented By:** api-engineer
**Date:** 2025-10-28
**Status:** ✅ Complete (1.0–1.5)

### Task Description
Create a reusable onboarding library that persists Wi‑Fi credentials in NVS and orchestrates STA/AP modes with a configurable connection timeout. Expose a minimal API and status enum; add focused native and on‑device tests.

## Implementation Summary
The new `net_onboarding` library encapsulates Wi‑Fi onboarding for ESP32. On `begin()`, it loads credentials from NVS and attempts STA; if the timeout elapses without a connection, it falls back to SoftAP. On success, AP is stopped. The public `status()` reports state, RSSI, and IP when connected. A native build path avoids Arduino/ESP32 dependencies, enabling host tests for the state machine. We later refactored the platform boundary to adapters (`IWifi`/`INvs`) so production code is free of scattered `#ifdef`s.

Two small test suites were added:
- Native: validates AP default with no creds and timeout fallback from CONNECTING → AP.
- On‑device: verifies AP path and timeout fallback; a happy‑path test is included but skipped unless test credentials are defined.

## Files Changed/Created

### New Files
- `lib/net_onboarding/include/net_onboarding/NetOnboarding.h` — Public API (begin/loop/set/reset/status), enums, portable `Status`.
- `lib/net_onboarding/include/net_onboarding/Platform.h` — Adapter interfaces `IWifi`/`INvs` + factories.
- `lib/net_onboarding/src/NetOnboarding.cpp` — Library logic, orchestrating STA/AP via adapters.
- `lib/net_onboarding/src/PlatformEsp32.cpp` — ESP32 implementation (WiFi/Preferences) compiled when not using stub backend.
- `lib/net_onboarding/src/PlatformStub.cpp` — Minimal null‑object stub compiled when `USE_STUB_BACKEND` is set (native/tests).
- `test/test_NetOnboarding/test_NetOnboarding_native.cpp` — Native tests for state transitions and timeout fallback.
- `test/test_OnDevice/test_NetOnboarding.cpp` — On‑device tests for AP default and timeout fallback (+ optional happy‑path).
- `agent-os/specs/2025-10-28-wifi-onboarding-via-nvs/implementation/1-library-nvs-sta-ap-orchestration-implementation.md` — This document.

### Modified Files
- `src/main.cpp` — Integrated library into setup/loop; logs state changes; AP IP now queried via `WiFi.softAPIP()`; added test macros `NET_CONNECT_TIMEOUT_MS`, `SEED_NVS_BAD`, `SEED_NVS_CLEAR`.
- `agent-os/specs/2025-10-28-wifi-onboarding-via-nvs/tasks.md` — Marked 1.0–1.5 complete.
- `test/test_OnDevice/test_main.cpp` — Registered NetOnboarding tests in the on‑device runner.

## Key Implementation Details

### Public API and Status
**Location:** `lib/net_onboarding/include/net_onboarding/NetOnboarding.h`

- Enum `State { AP_ACTIVE, CONNECTING, CONNECTED }`.
- `Status { State state; int rssi_dbm; std::array<char,16> ip; }`.
- API: `begin(timeout_ms)`, `loop()`, `setCredentials(ssid,pass)`, `resetCredentials()`, `status()`.
- Extras: `saveCredentials/loadCredentials/clearCredentials` (explicit helpers), `setConnectTimeoutMs()` for runtime tuning. Test hooks compiled only for native/UNIT_TEST builds.

**Rationale:** Keeps the header Arduino‑agnostic (no `IPAddress` or `wl_status_t`) for host tests and portability; fixed‑size buffers avoid dynamic allocations in the hot path.

### Platform adapters (ESP32 vs. stub) and NVS
**Location:** `lib/net_onboarding/include/net_onboarding/Platform.h`, `lib/net_onboarding/src/PlatformEsp32.cpp`, `lib/net_onboarding/src/PlatformStub.cpp`

- `IWifi`/`INvs` interfaces decouple the library from Arduino headers; factories (`MakeWifi/MakeNvs`) choose the implementation.
- ESP32 path uses `WiFi` and `Preferences` (namespace `net`, keys `ssid`, `psk`).
- Stub path is a tiny null‑object: fixed buffers for NVS and a minimal Wi‑Fi that reports CONNECTED only after a simulated delay.
- Selection controlled by build flag `USE_STUB_BACKEND` (shared with MotorControl), aligning build patterns across modules.

### STA/AP orchestration with timeout
**Location:** `lib/net_onboarding/src/NetOnboarding.cpp`

- On `begin()`: load creds → CONNECTING or AP.
- `loop()`: while CONNECTING, if `WL_CONNECTED` → CONNECTED and stop AP; else if timeout elapsed → AP.
- CONNECTED state reverts to AP if link drops (simple recovery acceptable per spec).

**Rationale:** Non‑blocking polling keeps the library cooperative with the rest of the firmware and avoids timing interference with motion control.

### SoftAP identity & AP IP logging
**Location:** `lib/net_onboarding/src/NetOnboarding.cpp`

- SSID: `SOFT_AP_SSID_PREFIX + last-3-bytes(MAC)` as uppercase hex (e.g., `Mirror-A1B2C3`).
- PSK: `SOFT_AP_PASS` from `include/secrets.h`.
- `src/main.cpp` prints the actual SoftAP IP via `WiFi.softAPIP()` instead of a hardcoded default.

**Rationale:** Matches identity requirement and keeps secrets centralized.

## Dependencies (if applicable)

### New Dependencies Added
- None (uses ESP32 Arduino core Wi‑Fi and Preferences).

### Configuration Changes
- Build flag `USE_STUB_BACKEND` selects the stub adapter on native builds (already present in `[env:native]`).
- Optional test flags for motor‑timing isolation checks:
  - `NET_CONNECT_TIMEOUT_MS` (default 8000) — extend CONNECTING window (e.g., 30000).
  - `SEED_NVS_BAD` — one‑time seed of bogus creds to force CONNECTING for the timeout duration.
  - `SEED_NVS_CLEAR` — one‑time clear of Wi‑Fi creds to force AP.

## Testing

### Test Files Created/Updated
- `test/test_NetOnboarding/test_NetOnboarding_native.cpp` — native state/timeout tests.
- `test/test_OnDevice/test_NetOnboarding.cpp` — AP default, timeout fallback, optional happy‑path.
- `test/test_OnDevice/test_main.cpp` — added RUN_TEST entries for NetOnboarding.

### Test Coverage
- Unit tests: ✅ Native transitions and timeout fallback.
- Integration tests: ✅ On‑device AP and timeout fallback; happy‑path is opt‑in via secrets.
- Edge cases covered: Missing creds → AP; invalid creds → timeout → AP.

### Manual Testing Performed
- Verified SoftAP SSID format and joinability; AP IP printed from device.
- Motor timing isolation checks (Task 1.5): executed the bench runner while in AP, CONNECTING (bad creds + long timeout), and CONNECTED. No missed steps observed; AP/CONNECTED pass times within expected tolerance vs baseline; brief transient wobble acceptable only during CONNECTING.

### How to reproduce 1.5 checks (for future runs)
- Flash once with `-DSEED_NVS_BAD=1 -DNET_CONNECT_TIMEOUT_MS=30000` to hold CONNECTING ≈30 s; then remove `SEED_NVS_BAD`.
- Run `python tools/bench/manual_sequence_runner.py -p <PORT> -b 115200 -r 2` during CONNECTING; repeat under AP (with `SEED_NVS_CLEAR=1`) and CONNECTED (seed valid creds).

## User Standards & Preferences Compliance

### Coding style
**File Reference:** `agent-os/standards/global/coding-style.md`

**How Your Implementation Complies:** Small, focused methods (`enterApMode_`, `enterConnecting_`, `enterConnected_`), portable header, minimal allocations.

### Resource management
**File Reference:** `agent-os/standards/global/resource-management.md`

**How Your Implementation Complies:** Non‑blocking polling in `loop()`, fixed‑size buffers for status, AP/STA transitions kept simple.

### Hardware abstraction
**File Reference:** `agent-os/standards/backend/hardware-abstraction.md`

**How Your Implementation Complies:** Native path avoids Arduino types and provides test hooks, enabling host tests without hardware.

### Unit testing
**File Reference:** `agent-os/standards/testing/unit-testing.md`

**How Your Implementation Complies:** Host tests first with portable header; hardware tests minimal and fast.

## Known Issues & Limitations
1. Reconnect/backoff after unexpected disconnect is simplified (drops to AP); later refinement possible.
2. Happy‑path on‑device test remains opt‑in via `TEST_STA_SSID/TEST_STA_PASS`.

## Performance Considerations
Polling in `loop()` does trivial work per tick; AP/STA operations rely on ESP32 core. No dynamic memory in steady state; only fixed buffers for status strings.

## Security Considerations
Credentials stored in NVS without encryption (per spec). Passwords handled via Preferences and not logged.

## Dependencies for Other Tasks
Provides the foundation for Task Groups 2–4 (Serial NET commands, LED patterns, HTTP endpoints/portal).

## Notes
- Define `TEST_STA_SSID` / `TEST_STA_PASS` in `include/secrets.h` to enable the on‑device happy‑path test.
- Timeouts are tunable via `begin(ms)` or `setConnectTimeoutMs(ms)`.
