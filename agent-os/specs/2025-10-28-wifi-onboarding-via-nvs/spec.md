# Specification: Wi‑Fi Onboarding via NVS

## Goal
Enable reliable Wi‑Fi onboarding for ESP32 nodes with a simple SoftAP portal and serial fallback, persisting credentials in NVS, and exposing a minimal status/command surface. Deliver as an independent library reusable across sketches.

## User Stories
- As a bench tester, I want to set Wi‑Fi via serial (`NET:SET`) so I can configure devices without joining their SoftAP.
- As a first‑time user, I want a SoftAP portal that (while the SoftAP is active) scans nearby networks and lets me enter SSID/PSK to get the device online quickly.
- As an operator, I want clear LED and serial feedback (AP/connecting/connected) so I can diagnose at a glance.
- As a developer, I want a decoupled onboarding library with a small API and no coupling to motion control or serial parsing so I can reuse it in other projects.

## Core Requirements
### Functional Requirements
- Boot: Try STA if credentials exist; on timeout, fall back to SoftAP until saved creds succeed.
- SoftAP: SSID = `SOFT_AP_SSID_PREFIX + last-3-bytes(MAC)`; PSK = `SOFT_AP_PASS`.
- Portal: Serve minimal page at `192.168.4.1` to scan 2.4 GHz networks while SoftAP is active and accept manual SSID+PSK; store to NVS Preferences.
- Commands:
  - `NET:RESET` clears Wi‑Fi creds and reboots into SoftAP.
  - `NET:SET,<ssid>,<pass>` writes creds and attempts STA connect; accepted in AP or connected states; while connecting return `CTRL:ERR NET_BUSY_CONNECTING`. Support quoted fields for commas/spaces.
- `NET:STATUS` → `CTRL:ACK CID=<id> state=<AP_ACTIVE|CONNECTING|CONNECTED> rssi=<dBm|NA> ip=<x.x.x.x>`.
- (Extension) `NET:LIST` → when SoftAP is active, list nearby SSIDs ordered by RSSI (strongest first) for onboarding UI. Device emits `CTRL:ACK CID=<id> scanning=1` immediately, then streams `NET:LIST CID=<id>` followed by `SSID="…" …` payload lines once the scan completes. If SoftAP is not active, return `CTRL:ERR CID=<id> NET_SCAN_AP_ONLY`.
- `/api/status` (and serial `NET:STATUS`) expose state, RSSI, IPv4, current STA SSID, SoftAP SSID, SoftAP password, and device MAC so clients can guide users through STA/SoftAP transitions.
- `/api/reset` clears stored credentials, re-enables the SoftAP, and returns the SoftAP SSID/password for UI messaging.
- Feedback: LED patterns — fast blink = AP active, slow blink = connecting, solid = connected. Also print serial logs for state changes.
- Identity: Print MAC‑derived device ID at boot; SoftAP suffix uses last 3 MAC bytes.
- Single network only; DHCP; reconnect/backoff on disconnect.

### Non-Functional Requirements
- Independent library under `lib/net_onboarding/` with API: `begin()`, `loop()`, `setCredentials(ssid, pass)`, `resetCredentials()`, `status()`; no coupling to motion control or serial command processing.
- Static assets gzipped from `data_src/` to LittleFS via `tools/gzip_fs.py`.
- Keep portal light: consider Pico.css + Alpine.js (or htmx) but avoid heavy JS.
- Serial at 115200 baud.
- Responsive UI: verify usability on common desktop and mobile breakpoints (≥320px width), including touch targets and form controls.
 - Motor timing isolation: SoftAP and steady‑state STA operation must not materially interfere with motor control timing. Network work should be non‑blocking/async and scheduled at lower priority. Brief jitter is acceptable during initial CONNECTING only.

## Visual Design
No mockups provided. Portal includes:
- SSID list with refresh (SoftAP only); manual SSID + PSK inputs; submit button with confirmation and post-action guidance; minimal success/failure banner.
- Responsive layout for desktop and mobile browsers; touch‑friendly controls and viewport meta tag. Single‑column on narrow screens using Pico.css defaults.

## Reusable Components
### Existing Code to Leverage
- Serial console loop and response patterns: `src/console/SerialConsole.cpp` and `lib/MotorControl/include/MotorControl/MotorCommandProcessor.h` for HELP/CTRL formatting.
- Build and FS pipeline: `platformio.ini` LittleFS config and `tools/gzip_fs.py`.
- Secrets: `include/secrets.h` for `SOFT_AP_SSID_PREFIX` and `SOFT_AP_PASS`.

### New Components Required
- `lib/net_onboarding/` library: task/state machine, NVS wrapper, AP/STA orchestrator, LED driver shim, tiny HTTP portal.
- Public header `lib/net_onboarding/include/net_onboarding/NetOnboarding.h` exposing the API and status enums.
- Portal assets in `data_src/net/` (HTML/CSS/JS) built to `data/` gzipped.

## Technical Approach
- States: `AP_ACTIVE`, `CONNECTING`, `CONNECTED` (plus transitional `RESETTING` internal only).
- Transitions: boot → STA attempt (timeout → AP); `NET:SET` (if AP/CONNECTED) → cancel connect, write creds, enter `CONNECTING`; success → `CONNECTED`; failure/timeout → AP.
- Concurrency: run Wi‑Fi in its own task/ticker; guard NVS `Preferences` writes; cancel in‑flight connects before applying new creds.
- Serial integration: extend `MotorCommandProcessor` with `NET:*` verbs, but call into the library API (no library awareness of serial). Suspend `NET:SET`/`NET:RESET` while connecting → `CTRL:ERR NET_BUSY_CONNECTING`; `NET:STATUS` always allowed.
- (Extension) Command Correlation IDs (CID): device‑generated per accepted command (monotonic `u32` with wrap). Device replies `CTRL:ACK CID=<id>` and includes `CID=<id>` in all subsequent lines for that command (including async NET events). Host/TUI may use CID to pair responses and suppress background‑poll noise while a command is active.
- LED: define default pin/polarity in `include/boards/Esp32Dev.hpp` (ESP32 DevKit 30‑pin: GPIO 2, active‑low). Blink timing constants owned by the library.
- Portal: serve `index.html` at `/`; REST endpoints: `POST /api/wifi` for creds, `GET /api/scan` for networks (available only in SoftAP mode), `GET /api/status` for state/identity, and `POST /api/reset` for returning to SoftAP. Serve gzipped with correct content types.
- Error codes: add specific NET errors: `NET_BUSY_CONNECTING`, `NET_BAD_PARAM`, `NET_SAVE_FAILED`, `NET_CONNECT_FAILED`.

## Out of Scope
- WPS, TLS, captive‑portal DNS interception, mDNS.
- Multiple networks, static IP configuration.
- NVS encryption and ESP‑IDF/Arduino hybrid.

## Success Criteria
- Cold boot with saved creds connects within configurable timeout; otherwise SoftAP is available and portal loads under 2 seconds.
- `NET:SET` accepted from serial in AP/connected states; while connecting returns `CTRL:ERR NET_BUSY_CONNECTING`.
- `NET:STATUS`/`/api/status` accurately report state, RSSI when connected, IP, MAC, and SoftAP credentials for UI guidance.
- LED patterns match specified states; serial logs show transitions.
- Library builds and links standalone; no dependencies on motion control or serial parsing code.
 - With SoftAP active or STA connected (steady-state), representative motion sequences show no observable stutter or missed steps versus baseline; temporary motion smoothness degradation during CONNECTING is acceptable.
- Portal distinguishes STA vs SoftAP flows (scanning disabled in STA mode, reset guidance surfaced) and `/api/reset` reliably re-enables the SoftAP with returned SSID/password.
