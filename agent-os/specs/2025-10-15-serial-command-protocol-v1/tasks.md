# Task Breakdown: Serial Command Protocol v1

## Overview
Total Tasks: 22
Assigned roles: api-engineer, ui-designer, testing-engineer

Notes:
- No visuals available for this spec.
- Reuse references: PoC StepperMotor files (concepts only), serial-interface standards, build/testing standards.

## Task List

### Firmware Protocol Layer

#### Task Group 1: Parser, State, and Core Handlers
Assigned implementer: api-engineer
Dependencies: None
Standards: `@agent-os/standards/frontend/serial-interface.md`, `@agent-os/standards/global/resource-management.md`, `@agent-os/standards/testing/unit-testing.md`

- [x] 1.0 Complete protocol core (parser + handlers)
  - [x] 1.1 Write 2-8 focused unit tests (PlatformIO Unity) for protocol core
    - Cover: BAD_CMD, BAD_ID, BAD_PARAM; MOVE out-of-range → E07; ALL vs single-id addressing; busy rule
    - Limit to 2-8 highly focused tests maximum (target 6)
    - Run ONLY these tests for verification in this group
  - [x] 1.2 Implement robust line reader with max payload guard
    - New module, bounded buffers, newline-terminated commands
    - Enforce payload length per serial-interface standard
  - [x] 1.3 Implement command parser for `<CMD>:param1,param2` with optional params
    - Validate actions, comma-separated numeric params, strict ordering
    - IDs: `0–7` or `ALL`
  - [x] 1.4 Define `MotorBackend` interface and `StubBackend` with in-memory state
    - State per motor: id, position, speed, accel, moving, awake
    - No queues; single in-flight move/home per motor
  - [x] 1.5 Implement handlers: `HELP`, `STATUS`, `WAKE`, `SLEEP`, `MOVE`, `HOME`
    - Responses prefixed with `CTRL:`; multi-line only for HELP/STATUS
    - MOVE/HOME: auto‑WAKE on start; auto‑SLEEP on completion
  - [x] 1.6 Deterministic open‑loop timing (canonical)
    - `duration_ms = ceil(1000 * |target - current| / max(speed,1))`; ignore accel in v1 timing
    - Simulate completion to toggle `moving` and update `position`
  - [x] 1.7 Ensure protocol core tests pass
    - Run ONLY tests from 1.1
    - Do NOT run entire test suite

Acceptance Criteria:
- 6 unit tests from 1.1 pass
- Parser enforces grammar and errors as specified
- Handlers return correct `CTRL:` responses; HELP/STATUS multi-line
- Busy and ALL addressing semantics verified

### Host Tooling (CLI)

#### Task Group 2: Python Serial CLI
Assigned implementer: ui-designer
Dependencies: Task Group 1
Standards: `@agent-os/standards/frontend/serial-interface.md`, `@agent-os/standards/testing/build-validation.md`

- [x] 2.0 Complete host CLI
  - [x] 2.1 Write 2-8 focused tests for CLI argument→command mapping
    - Use a stub serial transport to avoid hardware dependency
    - Cover subcommands: help, status, wake, sleep, move (defaults), home (param order)
    - Limit to 2-8 tests (target 4)
    - Run ONLY these tests for this group
  - [x] 2.2 Implement `tools/serial_cli.py`
    - Subcommands: `help`, `status`, `wake`, `sleep`, `move`, `home`
    - Map to exact grammar; omit optional params by default
  - [x] 2.3 Provide example E2E script
    - `tools/e2e_demo.py` invoking typical flows; print responses
  - [x] 2.4 Ensure CLI tests pass
    - Run ONLY tests from 2.1

Acceptance Criteria:
- 4 CLI mapping tests pass
- CLI prints responses and handles multi-line outputs cleanly
- Examples run against firmware serial port

### Build & Output Formatting

#### Task Group 3: HELP/STATUS formatting and Build Validation
Assigned implementer: api-engineer
Dependencies: Task Group 1
Standards: `@agent-os/standards/testing/build-validation.md`, `@agent-os/standards/global/platformio-project-setup.md`, `@agent-os/standards/frontend/serial-interface.md`

- [x] 3.0 Finalize HELP and STATUS outputs and validate build
  - [x] 3.1 Write 2-8 focused unit tests (Unity)
    - Validate HELP contains all actions with correct param grammar (no error codes listed)
    - Validate STATUS has one line per motor including id, pos, speed, accel, moving, awake
    - Limit to 2-8 tests (target 4)
  - [x] 3.2 Implement HELP generator and STATUS reporter
    - Terse, skimmable formatting per spec; strictly ordered params
  - [x] 3.3 Build validation
    - Compile firmware for `esp32dev` via PlatformIO
    - No additional targets needed at this phase
  - [x] 3.4 Ensure tests pass and build succeeds
    - Run ONLY tests from 3.1

Acceptance Criteria:
- 4 HELP/STATUS tests pass
- `pio run` succeeds for `esp32dev`
- Output formats match spec exactly

### Testing

#### Task Group 4: Test Review & Gap Analysis
Assigned implementer: testing-engineer
Dependencies: Task Groups 1–3
Standards: `@agent-os/standards/testing/unit-testing.md`, `@agent-os/standards/testing/hardware-validation.md`

- [x] 4.0 Review feature tests and fill critical gaps only
  - [x] 4.1 Review tests from Task Groups 1–3 (expected 14 tests)
  - [x] 4.2 Analyze coverage gaps specific to this spec
    - Focus on: busy handling across ALL targets, out‑of‑range rejection, HOME parameter parsing without clamping
  - [x] 4.3 Write up to 10 additional strategic tests maximum
    - Prioritize end‑to‑end protocol flows through the parser and handlers
  - [x] 4.4 Run feature‑specific tests only
    - Run ONLY tests from 1.1, 2.1, 3.1, and 4.3 (expected total 16–24)

Acceptance Criteria:
- All feature-specific tests pass (≤ 24 total)
- Critical flows covered (busy, ALL, POS_OUT_OF_RANGE, HELP/STATUS formatting)
- No more than 10 additional tests added

## Execution Order
1. Firmware Protocol Layer (Task Group 1)
2. Host Tooling CLI (Task Group 2)
3. Build & Output Formatting (Task Group 3)
4. Test Review & Gap Analysis (Task Group 4)
