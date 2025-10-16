# Serial Command Protocol v1 — Operator Guide

## Quick Start
- Connect ESP32 over USB and open a serial terminal at 115200 baud.
- On boot you should see: `CTRL:READY Serial v1 — send HELP`.
- Type commands; characters echo and backspace is supported. Press Enter to send.

## Commands & Grammar
- HELP
  - `HELP`
  - Prints multi-line usage listing all commands and parameters.
- STATUS
  - `STATUS`
  - Multi-line: one line per motor with `id`, `pos`, `speed`, `accel`, `moving`, `awake`.
- WAKE / SLEEP
  - `WAKE:<id|ALL>`
  - `SLEEP:<id|ALL>` (errors with BUSY if the motor is moving)
- MOVE (absolute)
  - `MOVE:<id|ALL>,<abs_steps>[,<speed>][,<accel>]`
  - Defaults: `speed=4000` steps/s, `accel=16000` steps/s^2
  - Limits: target must be in `[-1200,+1200]` steps (else `E07 POS_OUT_OF_RANGE`)
- HOME (bump-stop; no clamping)
  - `HOME:<id|ALL>[,<overshoot>][,<backoff>][,<speed>][,<accel>][,<full_range>]`
  - Defaults: `overshoot=800`, `backoff=150` (speed/accel default to MOVE defaults)

## Semantics
- IDs are `0–7` or `ALL`.
- Motion is open-loop with deterministic duration; no queuing. While a motor (or any in ALL) is moving/homing, `MOVE`/`HOME` returns `CTRL:ERR E04 BUSY`.
- Auto‑WAKE before `MOVE`/`HOME`; auto‑SLEEP after completion. `SLEEP` errors during motion.
- Responses are single-line (`CTRL:OK`/`CTRL:ERR <code> <message>`) except `HELP` and `STATUS` which are multi-line.
- Error codes: `E01 BAD_CMD`, `E02 BAD_ID`, `E03 BAD_PARAM`, `E04 BUSY`, `E06 INTERNAL`, `E07 POS_OUT_OF_RANGE`.

## Examples (Serial)
```
HELP
WAKE:ALL
MOVE:0,100
HOME:0
STATUS
SLEEP:ALL
```

## Python CLI
- Dry run (no device):
  - `python3 tools/serial_cli.py move 0 1200 --dry-run`
- With device (Linux/Mac):
  - `python3 tools/serial_cli.py --port /dev/ttyUSB0 status`
- With device (Windows):
  - `python3 tools/serial_cli.py --port COM3 wake ALL`
- HOME with parameters:
  - `python3 tools/serial_cli.py home 1 --overshoot 900 --backoff 150 --speed 4000 --accel 16000`
- Install dependency for device use: `pip install pyserial`

## Troubleshooting
- `CTRL:ERR E07 POS_OUT_OF_RANGE`: target outside `[-1200,+1200]`.
- `CTRL:ERR E02 BAD_ID`: id not `0–7` or `ALL`.
- `CTRL:ERR E04 BUSY`: another motion/homing is in progress (no queuing).
- CRLF terminals: carriage return is ignored; Enter sends on newline.

## Dev Notes
- Build firmware: `pio run -e esp32dev`
- Run unit tests (host): `pio test -e native`
- Demo script: `python3 tools/e2e_demo.py --port <PORT>` (or without `--port` for dry-run)

