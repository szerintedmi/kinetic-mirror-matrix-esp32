# Task 1: Parser, State, and Core Handlers

## Overview
Task Reference: Task Group 1 from `agent-os/specs/2025-10-15-serial-command-protocol-v1/tasks.md`

### Task Description
Implement protocol core: robust line reader, command parser, in-memory motor backend, handlers for HELP/STATUS/WAKE/SLEEP/MOVE/HOME, busy logic, defaults, error taxonomy, and canonical open-loop timing. Add focused unit tests for core flows.

## Implementation Summary
- Added `Protocol` with parser and dispatcher implementing grammar `<CMD>:param1,param2` with optional params.
- Added `MotorBackend` interface and an in-memory `StubBackend` with deterministic time-based completion and auto-sleep on completion.
- Implemented response format `CTRL:OK` / `CTRL:ERR <code> <message>`, multi-line only for HELP and STATUS.
- Added `SerialConsole` module to encapsulate Serial I/O, echo/backspace handling, command dispatch, and `tick()`; main delegates to it.

## Files Changed/Created
### New Files
- `src/protocol/MotorBackend.h`
- `src/protocol/StubBackend.h`
- `src/protocol/StubBackend.cpp`
- `src/protocol/Protocol.h`
- `src/protocol/Protocol.cpp`
- `test/test_protocol/test_core.cpp`
- `agent-os/specs/2025-10-15-serial-command-protocol-v1/implementation/prompts/1-parser-state-and-core-handlers.md`
- `src/console/SerialConsole.h`
- `src/console/SerialConsole.cpp`

### Modified Files
- `src/main.cpp`

## Key Implementation Details
### Protocol and Grammar
- Verbs: HELP, STATUS, WAKE, SLEEP, MOVE, HOME
- Grammar:
  - `MOVE:<id|ALL>,<abs_steps>[,<speed>][,<accel>]`
  - `HOME:<id|ALL>[,<overshoot>][,<backoff>][,<speed>][,<accel>][,<full_range>]`
  - `WAKE:<id|ALL>`, `SLEEP:<id|ALL>`, `STATUS`, `HELP`
- IDs: `0–7` or `ALL`
- Defaults: speed=4000, accel=16000; HOME defaults overshoot=800, backoff=150
- Limits: MOVE target rejected if outside [-1200,+1200] → `E07 POS_OUT_OF_RANGE`

### Backend and Timing
- `MotorBackend` abstraction; `StubBackend` tracks per-motor state: id, position, speed, accel, moving, awake.
- Busy: if any targeted motor is moving, MOVE/HOME return `E04 BUSY` atomically.
- Auto‑WAKE on MOVE/HOME start; auto‑SLEEP when completion occurs.
- Deterministic open‑loop timing: `duration_ms = ceil(1000 * |target-current| / max(speed,1))` (HOME uses overshoot+backoff for duration). Accel ignored in v1 timing.

### HELP/STATUS
- HELP lists commands and parameter grammar (no error code list).
- STATUS prints one line per motor: `id=<n> pos=<steps> speed=<steps/s> accel=<steps/s^2> moving=<0|1> awake=<0|1>`.

## Dependencies (if applicable)
- None added.

## Testing
### Test Files Created/Updated
- `test/test_protocol/test_core.cpp` — Core behaviors and errors.

### Test Coverage
- Unit tests: ✅ Focused core tests (BAD_CMD/BAD_ID/BAD_PARAM, POS_OUT_OF_RANGE, ALL addressing, BUSY and completion)
- Integration tests: ❌ None
- Edge cases: basic overflow guard in main, optional params parsing

### Manual Testing Performed
- Built firmware for `esp32dev` successfully; validated native unit tests locally.

### Results Summary
- Build: `pio run -e esp32dev` — success
- Tests: `pio test -e native` — 7 tests passed

## User Standards & Preferences Compliance

### Serial Interface
File Reference: `agent-os/standards/frontend/serial-interface.md`
- Enforced newline-terminated input, guard parsing, and `HELP` documentation. Prefixed responses with `CTRL:`.

### Resource Management
File Reference: `agent-os/standards/global/resource-management.md`
- Bounded input buffer, finite parsing, and simple timing without busy loops; `tick()` used for progression.

### Build Validation
File Reference: `agent-os/standards/testing/build-validation.md`
- Structured to compile under PlatformIO for `esp32dev`; tests created for targeted verification.

## Known Issues & Limitations
- No real motor driver integration (deferred to roadmap item 2).
- Acceleration ignored in timing for v1 (per spec); a start delay may be added later to mirror FastAccelStepper behavior.

## Dependencies for Other Tasks
- Task Group 3 depends on HELP/STATUS formatting — core is in place; formatting tests pending in that group.
- Task Group 2 (CLI) depends on grammar implemented here.

## Notes
- The protocol and backend were designed for a clean swap to a `HardwareBackend` in roadmap item 2 while keeping the same command surface.
