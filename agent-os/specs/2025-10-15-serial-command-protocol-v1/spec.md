# Specification: Serial Command Protocol v1

## Goal

Define a simple, human-readable USB serial protocol to control 8 motors on an ESP32, enabling immediate end-to-end testing with a Python CLI while establishing a stable command surface for later hardware bring-up and diagnostics.

## User Stories

- As a maker, I can send simple text commands over USB to move or home motors so I can test patterns quickly.
- As a developer, I can rely on defaults for speed/accel and only override them when needed for diagnostics.
- As an operator, I can call STATUS and HELP to understand the current state and available commands.

## Core Requirements

### Functional Requirements

- Grammar: `<CMD>:param1,param2` with optional trailing parameters omitted to use defaults.
- Commands:
  - `HELP` → Multi-line list of commands and parameter grammar (no error codes listed).
  - `MOVE:<id|ALL>,<abs_steps>[,<speed>][,<accel>]` → Absolute move to target steps.
  - `HOME:<id|ALL>[,<overshoot>][,<backoff>][,<speed>][,<accel>][,<full_range>]` → Bump-stop homing sequence; overshoot/backoff/speed/accel/full_range are optional and default when omitted; no clamp/limits on parameters.
  - `STATUS` → Multi-line, one line per motor including id, position, speed, accel, moving, awake.
  - `WAKE:<id|ALL>` → Enable driver(s) immediately.
  - `SLEEP:<id|ALL>` → Disable driver(s) immediately.
- Addressing: IDs `0–7` and `ALL` broadcast.
- Defaults:
  - MOVE: `speed=4000` steps/s, `accel=16000` steps/s^2 when omitted.
  - Limits: motion target range `[-1200, +1200]` full steps (2400 total).
  - HOME: `overshoot=800`, `backoff=150`, optional `speed/accel/full_range` use firmware defaults when omitted.
- Power semantics: Auto‑WAKE before motion; auto‑SLEEP only after completion of `MOVE` and `HOME` (no idle-timeout autosleep in v1). In later hardware integration, this maps to FastAccelStepper's per-motor auto-enable/auto-sleep; manual wake/sleep will not be required for normal operation.
- Busy behavior: While any commanded motion/homing is active, new `MOVE`/`HOME` for those motors returns busy.
- Responses: `CTRL:OK` or `CTRL:ERR <code> <message>`; `HELP` and `STATUS` return multi-line blocks, otherwise single-line.
- On startup, print a small `CTRL:READY` banner
- Error codes: `E01 BAD_CMD`, `E02 BAD_ID`, `E03 BAD_PARAM`, `E04 BUSY`, `E06 INTERNAL`, `E07 POS_OUT_OF_RANGE`. Do not show codes in HELP.

### Non-Functional Requirements

- Serial: 115200 baud; guard parsing with payload length checks per standards.
- Reliability: Reject malformed payloads, unknown commands, invalid IDs, and out-of-range positions with clear `CTRL:ERR` responses.
- Concurrency: No queuing; enforce single in-flight motion affecting target motors.
- Readability: HELP output is concise and skimmable; parameter order is strictly defined.

## Visual Design

- No visual assets provided for this spec.

## Reusable Components

### Existing Code to Leverage

- Concepts from PoC `StepperMotor` (awake state management, active move tracking) located at:
  - `agent-os/specs/2025-10-15-serial-command-protocol-v1/planning/code_examples/StepperMotor.hpp`
  - `agent-os/specs/2025-10-15-serial-command-protocol-v1/planning/code_examples/StepperMotor.cpp`

### New Components Required

- Parser: Parse `<CMD>:param1,param2` with optional parameters and validation.
- Dispatcher: Route commands (`MOVE/HOME/STATUS/WAKE/SLEEP/HELP`) to handlers.
- MotorManager: Manage ID `0–7`, broadcast `ALL`, and busy state across motors.
- Motion Controller: Apply defaults, validate targets vs. limits, and trigger auto‑WAKE/SLEEP.
- Status Reporter: Produce per‑motor lines including id, pos, speed, accel, moving, awake.
- Help Generator: Emit terse usage lines without exposing numeric error codes.
- Host CLI: Python 3 script `tools/serial_cli.py` to issue commands, print results, and run small E2E checks.
- Backend abstraction: `MotorBackend` interface with `StubBackend` in this item; `HardwareBackend` using FastAccelStepper and 74HC595 in roadmap item 2.

## Technical Approach

- Firmware (ESP32/Arduino):
  - Implement a robust line reader with max payload length.
  - Command parser validates verb, addressing (`0–7|ALL`), numeric params, and ordering.
  - MOVE absolute: reject targets beyond `[-1200,+1200]` with `E07 POS_OUT_OF_RANGE`; otherwise apply defaults if missing.
  - HOME: execute bump‑stop sequence; accept overrides with no clamping; apply defaults if missing.
  - Auto‑WAKE before motion; auto‑SLEEP immediately after MOVE/HOME completes (later mapped to FastAccelStepper auto-enable).
  - STATUS: print `id=<n> pos=<steps> speed=<steps/s> accel=<steps/s^2> moving=<0|1> awake=<0|1>` one per line.
  - Responses prefixed with `CTRL:` per standards; multi-line only for HELP/STATUS.
  - Deterministic open-loop timing (canonical for v1): `duration_ms = ceil(1000 * |target - current| / max(speed,1))`. Ignore acceleration in v1 timing. A small `start_delay_ms` may be added in a later item to mirror FastAccelStepper startup behavior.
- Host CLI (Python):
  - Provide subcommands: `help`, `move`, `home`, `status`, `wake`, `sleep`.
  - Map to grammar exactly; omit optional params by default; pretty-print responses.
  - Include a minimal test mode to validate round-trips and busy/error handling.

## Out of Scope

- Relative moves (beyond internal HOME steps).
- Command queues or pipelines.
- Checksums, persistent config, or idle autosleep timers.
- Extended diagnostics (fault codes, thermal counters) beyond minimal STATUS.

## Success Criteria

- HELP lists all verbs with correct parameter grammar.
- MOVE accepts defaults, executes within limits, and rejects out-of-range with `E07 POS_OUT_OF_RANGE`.
- HOME accepts overrides without clamping and completes with correct auto‑SLEEP behavior.
- STATUS prints one line per motor including id, pos, speed, accel, moving, awake.
- WAKE/SLEEP change driver state and reflect in STATUS.
- Host CLI runs on Python 3 with pyserial and completes a basic E2E script against the firmware.
