# 2. NET commands parsing (Serial) — Implementation (updated)

## Summary
- Implemented NET serial verbs and integrated them with a shared `Net()` singleton:
  - `NET:STATUS` returns state, RSSI, IP, and SSID.
  - `NET:RESET` clears creds and returns `CTRL:ACK`; AP activation is announced asynchronously.
  - `NET:SET,"<ssid>","<pass>"` supports quoted fields/escapes; returns immediate `CTRL:ACK`, then async progress events.
- Standardized success token from `CTRL:OK` to `CTRL:ACK` across firmware and tests.
- Clear validation error for short passwords: `CTRL:ERR NET_BAD_PARAM PASS_TOO_SHORT` (1–7 chars).
- Asynchronous state-change events from Arduino loop:
  - `CTRL: NET:CONNECTING`, `CTRL: NET:CONNECTED ssid=... ip=... rssi=...`, `CTRL: NET:AP_ACTIVE ssid=... ip=...`.
  - On connect failure/timeout: `CTRL:ERR NET_CONNECT_FAILED` emitted before `AP_ACTIVE`.
- Busy policy simplified: `NET:SET`/`NET:RESET` reject during `CONNECTING` with `CTRL:ERR NET_BUSY_CONNECTING`; `NET:STATUS` always allowed.
- Interactive TUI now:
  - Echoes `> command` once, strips device input echo, logs async `CTRL: NET:*`/`CTRL:ERR NET_*` lines, and suppresses polled `NET:STATUS` ACK lines to avoid spam.

## Files Touched (high-level)
- `lib/net_onboarding/include/net_onboarding/NetSingleton.h`, `lib/net_onboarding/src/NetSingleton.cpp` — Introduced `Net()` singleton.
- `src/main.cpp` — Emits concise async NET events; fixed transition detection for `NET_CONNECT_FAILED`.
- `lib/MotorControl/src/MotorCommandProcessor.cpp` — NET verb parsing, validation, `CTRL:ACK`, busy policy, `NET:STATUS` content.
- `tools/serial_cli/__init__.py` — Command echo, async-event forwarding during polls, suppression of polled `NET:STATUS` ACK lines.
- Tests updated across `test/test_NetCommands/*`, `test/test_MotorControl/*`, and CLI tests to expect `CTRL:ACK`.

## Behavior Details (final)
- `NET:RESET` → returns `CTRL:ACK`. If already in AP mode, emits one immediate `CTRL: NET:AP_ACTIVE ...`; otherwise the loop emits on transition.
- `NET:STATUS` → `CTRL:ACK state=<AP_ACTIVE|CONNECTING|CONNECTED> rssi=<dBm|NA> ip=<x.x.x.x> ssid=<...> pass=<****|SOFT_AP_PASS>`.
- `NET:SET,<ssid>,<pass>` → `CTRL:ACK` (non‑blocking). Async flow:
  - `CTRL: NET:CONNECTING` then either `CTRL: NET:CONNECTED ...` or `CTRL:ERR NET_CONNECT_FAILED` + `CTRL: NET:AP_ACTIVE ...`.
- Validation:
  - SSID: 1–32 chars; PASS: 0 or 8–63 chars. If 1–7 → `CTRL:ERR NET_BAD_PARAM PASS_TOO_SHORT`.
- Busy rule: `NET:SET`/`RESET` rejected with `CTRL:ERR NET_BUSY_CONNECTING` during CONNECTING; `NET:STATUS` always allowed.

## Testing
- Native: `pio test -e native` — all suites pass (88 tests).
- Manual: verified direct serial and TUI both show `CTRL:ACK` for NET:RESET and async NET events for SET/RESET.

## Forward-looking extensions (added to tasks/spec)
- Command Correlation ID (CID), device‑generated: on accepting a command, firmware assigns a monotonic `u32` CID (wrap allowed), replies `CTRL:ACK CID=<id>`, and includes `CID=<id>` on all subsequent lines for that command (including async NET events). The interactive TUI uses CID only to attribute async responses to the current user command and suppress background‑poll responses while that command is active.
- `NET:LIST`: list nearby Wi‑Fi networks ordered by RSSI for onboarding UI.
