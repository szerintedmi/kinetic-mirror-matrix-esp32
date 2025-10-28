# Task Breakdown: Wi‑Fi Onboarding via NVS

## Overview

Total Tasks: 7 groups (with focused sub‑tasks)
Assigned roles: api-engineer, ui-designer, testing-engineer

## Task List

### Connectivity Core (merged)

#### Task Group 1: Library + NVS + STA/AP orchestration

**Assigned implementer:** api-engineer
**Dependencies:** None

- [x] 1.0 Create library skeleton `lib/net_onboarding/`
  - Public header: `include/net_onboarding/NetOnboarding.h`
  - Source: `src/NetOnboarding.cpp`
  - API: `begin()`, `loop()`, `setCredentials(ssid,pass)`, `resetCredentials()`, `status()`; enums for `AP_ACTIVE|CONNECTING|CONNECTED`
- [x] 1.1 NVS persistence using `Preferences`
  - Namespace `net`; keys `ssid`, `psk`
  - Functions: `saveCredentials`, `loadCredentials`, `clearCredentials`
- [x] 1.2 STA connect with configurable timeout; fallback/hand‑over to SoftAP
  - On timeout → start AP; on success → stop AP
  - `status()` exposes state, RSSI, IP (when connected)
- [x] 1.3 SoftAP identity & password
  - SSID `SOFT_AP_SSID_PREFIX + last-3-bytes(MAC)`; PSK from `SOFT_AP_PASS`
  - Verify DHCP defaults
- [x] 1.4 Tests (focused)
  - 2–4 native tests for basic state transitions and status enum
  - 2–4 on‑device checks for timeout → AP and happy‑path connect
- [x] 1.5 Motor timing isolation checks
  - Script a simple motion loop; compare baseline vs AP active vs CONNECTED steady‑state
  - Document acceptable jitter during CONNECTING

Acceptance

- Cold boot with saved creds connects within timeout; else AP starts and portal IP reachable
- NVS save/load/clear works across reboot
- No observable motor stutter in AP/CONNECTED steady‑state vs baseline

Manual tests

- AP path: Ensure no creds in NVS → boot → verify AP SSID format and joinable; reach `192.168.4.1`
- STA path (before `NET:SET` exists): Pre‑seed NVS credentials using one of:
  1) Temporary one‑off "seed" sketch that calls `saveCredentials(TEST_STA_SSID, TEST_STA_PASS)` from `include/secrets.h`, then reflash normal firmware
  2) A small temporary seeding function behind `#ifdef SEED_NVS` compiled once
  3) A standalone minimal tool that writes `ssid`/`psk` via `Preferences` (Arduino sketch) and reboots
- After seeding, reboot → verify CONNECTED state, RSSI, and IP reported by `status()`; run motor loop and observe isolation

### Serial Surface

#### Task Group 2: NET commands parsing (Serial)

**Assigned implementer:** api-engineer
**Dependencies:** Task Group 1

- [ ] 2.0 Extend `MotorCommandProcessor` HELP with NET verbs
- [ ] 2.1 Implement `NET:RESET` (clear creds + reboot into AP)
- [ ] 2.2 Implement `NET:STATUS` (state/RSSI/IP)
- [ ] 2.3 Implement `NET:SET,<ssid>,<pass>` with quoted fields and validation
- [ ] 2.4 Suspend `NET:*` while connecting → `CTRL:ERR NET_BUSY_CONNECTING`
- [ ] 2.5 2–6 focused tests (native) for parsing/formatting and busy path

Acceptance

- HELP output lists NET verbs (`NET:RESET`, `NET:STATUS`, `NET:SET`) with payload syntax (including quoting rules) and documents busy error `NET_BUSY_CONNECTING`.
- Manual serial session exercises all NET verbs; responses match spec

Manual test

- Use `tools/serial_cli` to send HELP/NET:* and verify responses

### Feedback & Hardware Signals

#### Task Group 3: LED patterns

**Assigned implementer:** api-engineer
**Dependencies:** Task Group 1

- [ ] 3.0 Define LED pin/polarity in `include/boards/Esp32Dev.hpp` (GPIO 2, active‑low)
- [ ] 3.1 Implement non‑blocking blinker (fast=AP, slow=connecting, solid=connected)
- [ ] 3.2 2–4 focused tests (timing windows) or visual confirmation script

Acceptance

- Patterns match states; does not block main loop

Manual test

- Observe LED through each state; verify blink rates

### Embedded Web UI

#### Task Group 4: HTTP endpoints

**Assigned implementer:** api-engineer
**Dependencies:** Task Group 1

- [ ] 4.0 Serve gzipped static files from LittleFS (`/index.html`, css, js)
- [ ] 4.1 Implement `GET /api/scan` (list SSIDs)
- [ ] 4.2 Implement `GET /api/status` (state/RSSI/IP)
- [ ] 4.3 Implement `POST /api/wifi` (save creds → connect attempt)
- [ ] 4.4 2–6 focused tests (manual with curl) for endpoints

Acceptance

- Endpoints respond as specified; POST triggers connect attempt

Manual test

- `curl http://192.168.4.1/api/scan` etc.; observe JSON and state change

#### Task Group 5: Responsive Portal UI

**Assigned implementer:** ui-designer
**Dependencies:** Task Group 4

- [ ] 5.0 HTML with Pico.css + Alpine.js (or htmx); mobile‑first responsive
- [ ] 5.1 Network list with refresh; manual SSID+PSK form
- [ ] 5.2 Success/failure banners; field validation; touch‑friendly controls
- [ ] 5.3 Build pipeline integration from `data_src/` → gzipped to `data/`
- [ ] 5.4 2–6 focused checks (Playwright/manual) at 320px/768px/1024px

Acceptance

- Portal usable on phone and desktop; assets served gzipped

Manual test

- Connect phone to AP; complete onboarding flow end‑to‑end

### Buttons & Resets

#### Task Group 6: Long‑press reset

**Assigned implementer:** api-engineer
**Dependencies:** Task Group 1

- [ ] 6.0 Implement 5s button hold → `resetCredentials()` + reboot to AP
- [ ] 6.1 Debounce and ignore short taps

Acceptance

- Long press reliably clears creds; short press ignored

Manual test

- Hold BOOT button ~5s; verify AP comes up with default SSID

### Docs & Standards Alignment

Note: HELP text updates are part of Task Group 2 when implementing commands. README updates move to the final E2E task.

### Steel Thread / E2E

#### Task Group 7: End‑to‑end verification

**Assigned implementer:** testing-engineer
**Dependencies:** Task Groups 1–6

- [ ] 7.0 Cold boot (no creds) → AP; portal sets creds → CONNECTED
- [ ] 7.1 `NET:STATUS` reports CONNECTED with IP/RSSI
- [ ] 7.2 Long‑press reset restores AP flow
- [ ] 7.3 Update README with onboarding guide (SoftAP and Serial alternatives) and LED patterns

Acceptance

- All steps succeed without code changes; logs/LEDs match spec; README updated

Manual test

- Follow 7.0–7.2; record outcomes under `planning/verification/`

## Execution Order

Recommended sequence:

1. Connectivity Core (1)
2. Serial Surface (2)
3. LED patterns (3)
4. HTTP Endpoints (4)
5. Portal UI (5)
6. Long‑press Reset (6)
7. E2E + README (7)
