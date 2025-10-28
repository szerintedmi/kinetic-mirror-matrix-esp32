# Spec Requirements: Wi‑Fi Onboarding via NVS

## Initial Description
7. [ ] Wi‑Fi Onboarding via NVS — Provide SoftAP onboarding for SSID/PSK, persist credentials in NVS Preferences, and auto‑connect on boot. Add resets via serial `NET:RESET` and long‑press boot button. Acceptance: cold boot → AP provision → joins LAN; reset paths verified. Depends on: none. `S`

## Requirements Discussion

### First Round Questions

**Q1:** Boot behavior fallback timing and policy.
**Answer:** 1. fall back to SoftAP after a timeout (set in code constant )

**Q2:** SoftAP SSID/prefix/password and uniqueness concerns.
**Answer:** 2. Mirror-XXXX is fine. For prefix use  SOFT_AP_SSID_PREFIX  definined in gitignored include/secrets.h.  As for password: use SOFT_AP_SSID_PREFIX from secrets.h 
 I wonder what are chances of SSID clash if only use last 3 bytes of mac address? shall we use full mac address? 

**Q3:** Onboarding UI: scan + manual entry and LittleFS/gzip pipeline.
**Answer:** 3. scanning with option to manually enter SSID too. make sure the static pages stored in littlefs using our existing tooling for building gzipped /data from /data_src

**Q4:** Credential storage/encryption scope.
**Answer:** 4. no additional fields stored and no NVS encryption for now (encrypt would require to change to `framework = espidf, arduino`, we should try that later) 

**Q5:** Reset semantics and hold time.
**Answer:** 5. just simple softAP reset for now with 5s hold

**Q6:** Feedback modes (LED/serial).
**Answer:** 6. both LED and serial feedback

**Q7:** Device identity strategy.
**Answer:** 7. Mac derived id. see my prev question about uniqueness (ie full mac as id or last x bytes)

**Q8:** Single vs multiple saved networks.
**Answer:** 8. single network only

**Q9:** Out‑of‑scope features confirmation.
**Answer:** 9. correct, those are out of scope.

### Follow-up Questions

**Follow-up 1:** SSID uniqueness vs readability; SSID suffix length and device ID length.
**Answer:** 1. SSID for softAP: 3 bytes is fine - it's only used at setup time, collosiona chance is low and managebale if happens

**Follow-up 2:** Confirmation of secrets macro names/updates.
**Answer:** 2. I've updated secrets.h 

**Follow-up 3:** Status LED pin/polarity placement.
**Answer:** 3. use the most common default GPIO and settings for ESP32 30 pin devkits. Define  it in [Esp32Dev.hpp](include/boards/Esp32Dev.hpp) 

## Visual Assets
No visual assets provided.

## Existing Code to Reference

**Similar Features/Utilities Identified:**
- LittleFS gzip pipeline: `tools/gzip_fs.py` with `extra_scripts = pre:tools/gzip_fs.py` in `platformio.ini`.
- Filesystem selection: `board_build.filesystem = littlefs` in `platformio.ini`.
- Secrets (gitignored): `include/secrets.h` defines `SOFT_AP_SSID_PREFIX` and `SOFT_AP_PASS` for SoftAP.

## Requirements Summary

### Functional Requirements
- Boot behavior: On cold boot, attempt STA connect if credentials exist; on timeout (config constant), fall back to SoftAP until credentials saved or STA succeeds.
- SoftAP identity: SSID = `SOFT_AP_SSID_PREFIX + last-3-bytes(MAC)`; password from `SOFT_AP_PASS` in `include/secrets.h`.
 - Onboarding UI: Serve minimal web page from LittleFS at SoftAP gateway (192.168.4.1) to scan 2.4 GHz networks and allow manual SSID entry; submit to save SSID/PSK to NVS Preferences.
  - Credential storage: Use Arduino `Preferences` (NVS), plaintext; keys for SSID/PSK only; no encryption.
  - Reset paths: Serial `NET:RESET` and long‑press (~5s) clear Wi‑Fi credentials and reboot into SoftAP.
 - Feedback: Provide both serial logs and status LED indications (define LED pin/polarity in `include/boards/Esp32Dev.hpp`; default to common ESP32 devkit pin/polarity). LED patterns: fast blink = AP active, slow blink = connecting, solid = connected.
  - Device identity: MAC‑derived device ID printed during onboarding and used later for MQTT; SoftAP uses last 3 bytes suffix for readability.
  - Networks: Support a single saved network with DHCP; reconnect/backoff on disconnect.
 - Serial command: `NET:SET,<ssid>,<pass>` updates credentials and attempts STA connect. Accepted in SoftAP or STA‑connected states; while connecting, respond with `CTRL:ERR NET_BUSY_CONNECTING`. Returns `CTRL:OK` on acceptance (no `est_ms`). Support quoted fields for commas/spaces.
 - Serial command: `NET:STATUS` returns current state and basics; e.g., `CTRL:OK state=<AP_ACTIVE|CONNECTING|CONNECTED> rssi=<dBm|NA> ip=<x.x.x.x|NA>`.

### Non-Functional Requirements
- Static assets stored in LittleFS as pre‑gzipped files built from `data_src/` via `tools/gzip_fs.py`.
- Keep pages small, responsive, and dependency‑light; consider Pico.css + Alpine.js (or htmx) for simple UI.
- Serial console at 115200 baud for logs and prompts.
 - Independent library: Ship onboarding as a standalone firmware module/library with a small C++ API (`begin()`, `loop()`, `setCredentials(ssid,pass)`, `resetCredentials()`, `status()`), minimal deps, and no coupling to motion control or serial command processing. Place under `lib/net_onboarding/` (preferred) or `src/net/onboarding/` with a single public header.

### Reusability Opportunities
- Reuse existing LittleFS gzip build tooling and PlatformIO setup.
- Follow existing serial command patterns for `NET:RESET` response formatting (`CTRL:OK` / `CTRL:ERR`).

### Scope Boundaries
**In Scope:**
  - SoftAP onboarding flow with scan and manual entry
  - NVS credential save/clear
  - Boot‑time STA connect with fallback
  - Serial and LED feedback
  - Suspend `NET:*` serial commands while connecting (return `CTRL:ERR NET_BUSY_CONNECTING`)

**Out of Scope:**
- WPS, TLS, captive‑portal DNS interception, mDNS
- Multiple saved networks, static IP config
- NVS encryption and ESP‑IDF/Arduino hybrid migration (future)

### Technical Considerations
- Timeouts and LED behavior governed by code constants with sane defaults.
- SSID suffix uses last 3 MAC bytes; device ID should use full MAC for global uniqueness (can be overridden later if needed).
- Place LED pin definition in `include/boards/Esp32Dev.hpp` using the common ESP32 DevKit default (GPIO 2, active‑low) unless board dictates otherwise.
- Ensure web UI assets are pre‑gzipped and served with correct content types.
 - Serial during Wi‑Fi transitions: suspend `NET:*` during STA connect and respond with `CTRL:ERR NET_BUSY_CONNECTING`. Keep other serial commands responsive. Run Wi‑Fi state machine in its own task; guard `Preferences` writes with a mutex; cancel in‑flight connects before applying `NET:SET` (e.g., `WiFi.disconnect(true, true)`), then attempt reconnect. If SoftAP is active, run `WIFI_AP_STA` during handover and disable AP after STA has an IP or on timeout.
 - Command grammar edge cases: SSIDs/passwords may include commas/spaces. Support quoted fields: `NET:SET,"ssid,with,commas","pa ss"`. Escape `"` and `\`. Optionally add `NET:SETB64,<ssid_b64>,<pass_b64>` later if needed.
