# 2. NET commands parsing (Serial) — Implementation (updated)

## Summary
- Implemented NET serial verbs and integrated them with a shared `Net()` singleton:
  - `NET:STATUS` returns state, RSSI, IP, and SSID.
  - `NET:RESET` clears creds and returns `CTRL:ACK`; AP activation is announced asynchronously.
  - `NET:SET,"<ssid>","<pass>"` supports quoted fields/escapes; returns immediate `CTRL:ACK`, then async progress events.
- Standardized success token from `CTRL:OK` to `CTRL:ACK` across firmware and tests.
- Consolidated CTRL formats (CID-first):
  - `CTRL:ACK CID=<id> ...`
  - `CTRL:ERR CID=<id> E## NAME ...`
  - `CTRL:WARN CID=<id> NAME ...`
  - Async events: `CTRL: NET:<EVENT> CID=<id> ...`
- Clear validation error for short passwords: `CTRL:ERR NET_BAD_PARAM PASS_TOO_SHORT` (1–7 chars).
- Asynchronous state-change events from Arduino loop:
  - `CTRL: NET:CONNECTING`, `CTRL: NET:CONNECTED ssid=... ip=... rssi=...`, `CTRL: NET:AP_ACTIVE ssid=... ip=...`.
  - On connect failure/timeout: `CTRL:ERR NET_CONNECT_FAILED` emitted before `AP_ACTIVE`.
- Busy policy simplified: `NET:SET`/`NET:RESET` reject during `CONNECTING` with `CTRL:ERR NET_BUSY_CONNECTING`; `NET:STATUS` always allowed.
- Interactive TUI now:
  - Echoes `> command` once, strips device input echo, logs async `CTRL: NET:*`/`CTRL:ERR NET_*` lines, and suppresses polled `NET:STATUS` ACK lines to avoid spam.

## Extensions (implemented)
- Command Correlation IDs (CID): firmware generates a monotonic `u32` per accepted command. All `CTRL:ACK` responses now include `CID=<id>`. NET async lines (`NET:CONNECTING`, `NET:CONNECTED`, `NET:AP_ACTIVE`, and `CTRL:ERR NET_CONNECT_FAILED`) also carry `CID=<id>` for the in-flight NET command. CID clears when the flow completes (on `CONNECTED` or `AP_ACTIVE`).
- `NET:LIST`: returns a multi-line list of nearby networks ordered by RSSI, e.g.
  - First line: `CTRL:ACK CID=<id> scanning=1` (on-device this ACK is emitted immediately before the scan runs so the CLI never sees “(no response)”).
  - Payload header: `NET:LIST CID=<id>`, followed by one line per entry `SSID="<name>" rssi=<dbm> secure=<0|1> channel=<n>` (top 12 by RSSI).
  - Suspended during `CONNECTING` → `CTRL:ERR CID=<id> NET_BUSY_CONNECTING`.

## Files Touched (high-level)
- `lib/net_onboarding/include/net_onboarding/NetSingleton.h`, `lib/net_onboarding/src/NetSingleton.cpp` — Introduced `Net()` singleton.
- `src/main.cpp` — Emits concise async NET events; fixed transition detection for `NET_CONNECT_FAILED`.
- `lib/MotorControl/src/MotorCommandProcessor.cpp` — NET verb parsing, validation, `CTRL:ACK`, busy policy, `NET:STATUS` content.
- `lib/net_onboarding/include/net_onboarding/Cid.h`, `lib/net_onboarding/src/Cid.cpp` — CID allocator and active-CID tracker.
- `src/main.cpp` — Appends `CID=<id>` to async NET events when a command is active; clears CID on terminal events.
- `lib/net_onboarding/include/net_onboarding/Platform.h`, `src/PlatformEsp32.cpp`, `src/PlatformStub.cpp` — add Wi‑Fi scan API and AP+STA mode.
- `lib/net_onboarding/include/net_onboarding/NetOnboarding.h`, `src/NetOnboarding.cpp` — expose `scanNetworks()` helper.
- `tools/serial_cli/runtime.py` — Command echo, async-event forwarding during polls, suppression of polled `NET:STATUS` ACK lines.
- Tests updated across `test/test_NetCommands/*`, `test/test_MotorControl/*`, and CLI tests to expect `CTRL:ACK`.

## Behavior Details (final)
- `NET:RESET` → returns `CTRL:ACK`. If already in AP mode, emits one immediate `CTRL: NET:AP_ACTIVE ...`; otherwise the loop emits on transition.
- `NET:STATUS` → `CTRL:ACK CID=<id> state=<AP_ACTIVE|CONNECTING|CONNECTED> rssi=<dBm|NA> ip=<x.x.x.x> ssid="<...>" pass="<****|SOFT_AP_PASS>"`.
- `NET:SET,<ssid>,<pass>` → `CTRL:ACK` (non‑blocking). Async flow:
  - `CTRL: NET:CONNECTING` then either `CTRL: NET:CONNECTED ...` or `CTRL:ERR NET_CONNECT_FAILED` + `CTRL: NET:AP_ACTIVE ...`.
- Validation:
  - SSID: 1–32 chars; PASS: 0 or 8–63 chars. If 1–7 → `CTRL:ERR NET_BAD_PARAM PASS_TOO_SHORT`.
- Busy rule: `NET:SET`/`RESET` rejected with `CTRL:ERR NET_BUSY_CONNECTING` during CONNECTING; `NET:STATUS` always allowed.

Formatting/quoting details:
- All `CTRL:ACK`/`ERR`/`WARN` responses place `CID=<id>` immediately after the token.
- SSIDs and any exposed passwords are double-quoted and escape embedded `"` and `\`.
- Multi-line responses (e.g., `STATUS`, `GET LAST_OP_TIMING` list, `NET:LIST`) send an ACK line first (`CTRL:ACK CID=<id>`) followed by payload lines.

## Testing
- Native: `pio test -e native` — all suites pass (89 tests).
- Interactive CLI: streaming worker rebuilt; ACKs and NET:LIST rows appear immediately in both raw serial and TUI logs, background polls stay out of the transcript.
- Added native tests for CID presence in ACK lines and `NET:LIST` output shape.
- Manual: verified direct serial and TUI both show `CTRL:ACK` for NET:RESET and async NET events for SET/RESET.

## Forward-looking extensions (added to tasks/spec)
- Command Correlation ID (CID), device‑generated: on accepting a command, firmware assigns a monotonic `u32` CID (wrap allowed), replies `CTRL:ACK CID=<id>`, and includes `CID=<id>` on all subsequent lines for that command (including async NET events). The interactive TUI uses CID only to attribute async responses to the current user command and suppress background‑poll responses while that command is active.
- `NET:LIST`: list nearby Wi‑Fi networks ordered by RSSI for onboarding UI.
