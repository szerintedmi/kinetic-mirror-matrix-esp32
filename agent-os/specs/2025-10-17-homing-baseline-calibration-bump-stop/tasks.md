# Task Breakdown: Homing & Baseline Calibration (Bump‑Stop)

## Overview
Total Tasks: 12
Assigned roles: api-engineer, testing-engineer

Notes
- Keep scope tight: implement HOME sequencing with MOVE‑aligned defaults and WAKE/SLEEP behavior. Ignore soft limits during HOME.
- Reuse existing parser and controller patterns; use StubMotorController in native tests for fast feedback.

## Task List

### Firmware Implementation (Controller + Parser)

#### Task Group 1: HOME sequencing and parsing
Assigned implementer: api-engineer
Dependencies: None
Standards: `@agent-os/standards/backend/hardware-abstraction.md`, `@agent-os/standards/testing/unit-testing.md`, `@agent-os/standards/global/conventions.md`

- [ ] 1.0 Complete HOME implementation end‑to‑end
  - [ ] 1.1 Write 4–6 focused unit tests (native)
    - Parsing: `HOME:<id|ALL>[,<overshoot>][,<backoff>][,<speed>][,<accel>][,<full_range>]` with comma‑skips (e.g., `HOME:2,,50`)
    - Busy rule: reject HOME when any targeted motor is already moving
    - Concurrency: `HOME:ALL` starts all eight with the stub backend (no deadlocks)
    - Post‑state: after simulated completion, `position==0`, `moving==0`, `awake==0` for targeted motors
    - HELP includes HOME grammar line
  - [ ] 1.2 Implement controller HOME
    - In `HardwareMotorController::homeMask`: orchestrate negative run (full_range+overshoot), backoff, center to soft midpoint, then `setCurrentPosition(0)`; ignore soft limits during HOME
    - Use absolute targets computed from current positions if only abs moves are available; otherwise relative moves are fine
    - Mirror MOVE semantics for auto‑WAKE/SLEEP and WAKE override handling
  - [ ] 1.3 Align defaults and parsing
    - Ensure MOVE defaults apply to HOME: speed=4000, accel=16000
    - Derive `full_range = kMaxPos - kMinPos` (currently 2400)
  - [ ] 1.4 Ensure unit tests pass
    - Run ONLY the tests from 1.1

Acceptance Criteria
- 4–6 unit tests from 1.1 pass (parsing, busy rule, concurrency, post‑state, HELP grammar)
- HOME command sets runtime zero and ignores normal soft limits during its sequence
- WAKE/SLEEP behavior matches MOVE semantics

### Build & Bench Validation

#### Task Group 2: Build validation and homing bench checks
Assigned implementer: testing-engineer
Dependencies: Task Group 1
Standards: `@agent-os/standards/testing/build-validation.md`, `@agent-os/standards/testing/hardware-validation.md`

- [ ] 2.0 Validate build and execute homing bench checklist
  - [ ] 2.1 Build firmware for `esp32dev` via PlatformIO
  - [ ] 2.2 Bench checklist (manual)
    - Issue `HOME:<id>`: observe negative run to hard stop, backoff to negative soft boundary, center move; final logical position reads 0 in STATUS
    - Issue `HOME:ALL`: confirm concurrent motion across 8 motors without brownouts; STEP pulses and DIR/SLEEP latching look stable
    - Verify auto‑WAKE before motion and auto‑SLEEP after completion; respect manual `WAKE` (stay awake) and `SLEEP` (error if moving)
    - Confirm ignoring soft limits during HOME does not affect MOVE’s soft limits after homing

Acceptance Criteria
- `pio run -e esp32dev` completes successfully
- Bench checklist executed and documented; observed behavior matches spec (sequence, concurrency, WAKE/SLEEP parity, final zero)

## Execution Order
1. Firmware Implementation (Task Group 1)
2. Build & Bench Validation (Task Group 2)

