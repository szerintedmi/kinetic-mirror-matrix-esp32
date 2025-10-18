# Task Breakdown: Status & Diagnostics

## Overview
Total Tasks: 20
Assigned roles: api-engineer, ui-designer, testing-engineer

Notes:
- No visuals available for this spec.
- Extend existing firmware STATUS and host CLI per spec; avoid floating point; keep runtime overhead negligible.

## Task List

### Firmware Telemetry & STATUS Extensions

#### Task Group 1: Per‑Motor State + STATUS fields
Assigned implementer: api-engineer
Dependencies: None
Standards: `@agent-os/standards/frontend/serial-interface.md`, `@agent-os/standards/backend/task-structure.md`, `@agent-os/standards/global/resource-management.md`, `@agent-os/standards/testing/unit-testing.md`

- [x] 1.0 Complete firmware telemetry additions and STATUS output
  - [x] 1.1 Write 2–8 focused unit tests (Unity)
    - Validate STATUS includes new keys: `homed`, `steps_since_home`, `budget_s`, `ttfc_s`
    - Validate basic invariants under stubbed time: budget starts at 90, decrements while ON, may go negative when over‑budget, and ttfc_s ≥ 0
    - Target 6 tests; run ONLY these tests for this group
  - [x] 1.2 Extend MotorState and controllers
    - Add: `homed` (bool/bit), `steps_since_home` (int32), `budget_balance_s` (fixed‑point or int), `last_update_ms` (uint32)
    - Initialize sane defaults on boot; reset on reboot
  - [x] 1.3 Event‑driven budget bookkeeping
    - Update budget on MOVE/HOME/WAKE/SLEEP and before responding to STATUS
    - Spend 1.0 s/s while ON; refill 1.5 s/s while OFF up to max 90 s
    - Avoid floats; implement fixed‑point tenths (Q9.1) or integer math
  - [x] 1.4 Steps‑since‑home tracking
    - Reset on successful HOME completion
    - Accumulate absolute step deltas as position changes
  - [x] 1.5 Homed flag behavior
    - Set to 1 on successful HOME; cleared on reboot; MOVE does not change this flag
  - [x] 1.6 STATUS formatting
    - Reorder keys for readability if needed; include `homed`, `steps_since_home`, `budget_s` (1 decimal), `ttfc_s` (1 decimal)
    - Use integer/fixed‑point formatting; no heavy allocations in loop
  - [x] 1.7 Ensure firmware unit tests pass and build succeeds
    - Run ONLY tests from 1.1
    - Validate `pio run` for `esp32dev`

Acceptance Criteria:
- Unity tests from 1.1 pass (≈6 tests)
- STATUS prints all required keys; `budget_s` and `ttfc_s` formatted with ≤1 decimal
- Budget logic behaves as specified under stubbed timing
- Build succeeds without timing regressions

### Host Tooling (serial_cli.py)

#### Task Group 2: Interactive TUI with 2 Hz STATUS
Assigned implementer: ui-designer
Dependencies: Task Group 1
Standards: `@agent-os/standards/frontend/serial-interface.md`, `@agent-os/standards/testing/build-validation.md`

- [x] 2.0 Complete interactive mode in existing CLI
  - [x] 2.1 Write 2–6 focused tests
    - Argument parsing for `interactive` subcommand and options
    - Table rendering helper given sample STATUS lines (pure function; no serial)
    - Target 4 tests; run ONLY these tests for this group
  - [x] 2.2 Add `interactive` subcommand to `tools/serial_cli.py`
    - Options: `--port`, `--baud`, `--timeout`, `--rate` (default ~2 Hz)
  - [x] 2.3 Implement TUI loop (curses or graceful fallback)
    - Non‑blocking input line for commands (`HELP`, `MOVE`, `HOME`, `WAKE`, `SLEEP`)
    - Poll STATUS at configured rate; render compact fixed‑width table
    - Show command responses in a log pane without breaking the table
  - [x] 2.4 Robust serial I/O
    - Synchronize writes/reads; do not stall refresh while awaiting responses
    - Handle disconnects cleanly; show reconnect hints
  - [x] 2.5 Ensure CLI tests pass
    - Run ONLY tests from 2.1
  - [x] 2.6 Package + test harness alignment
    - Package CLI under `tools/serial_cli/` with `python -m serial_cli` entry
    - Move tests to `tools/tests/python/` and update PlatformIO wrapper to call `tools/tests/python/run_cli_tests.py`
    - Keep `serial_cli/` top‑level shim for repo‑root execution

Acceptance Criteria:
- CLI exposes `interactive` and runs without additional dependencies
- Table shows columns: `id pos moving awake homed steps_since_home budget_s ttfc_s`
- Commands can be sent while table refreshes; responses are visible
- Mapping/render tests pass (≈4 tests); PlatformIO native wrapper executes the Python test runner

### Formatting & Build Validation

#### Task Group 3: HELP/STATUS format updates and build
Assigned implementer: api-engineer
Dependencies: Task Group 1
Standards: `@agent-os/standards/testing/build-validation.md`, `@agent-os/standards/global/platformio-project-setup.md`

- [x] 3.0 Finalize formats and validate builds
  - [x] 3.1 Update/extend existing HELP/STATUS tests
    - Verify new keys appear in STATUS first line; maintain valid HELP
    - Target 3–4 assertions within existing test file
  - [x] 3.2 Build validation
    - Compile firmware for `esp32dev` via PlatformIO
  - [x] 3.3 Confirm serial output length remains manageable (no flooding)

Acceptance Criteria:
- Updated HELP/STATUS tests pass
- `pio run` succeeds for `esp32dev`
- STATUS output remains skimmable in one terminal width

### Testing

#### Task Group 4: Test Review & Gap Analysis
Assigned implementer: testing-engineer
Dependencies: Task Groups 1–3
Standards: `@agent-os/standards/testing/unit-testing.md`, `@agent-os/standards/testing/hardware-validation.md`

- [x] 4.0 Review feature tests and fill critical gaps only
  - [x] 4.1 Review tests from Task Groups 1–3
    - Unity tests from 1.1 and formatting tests from 3.1
    - Python CLI tests from 2.1
  - [x] 4.2 Analyze coverage gaps specific to STATUS extensions and TUI
    - Focus on boundary conditions: negative budget_s visibility (no clamp at 0), ttfc_s calc, homed + steps_since_home reset behavior
  - [x] 4.3 Write up to 8 additional strategic tests max
    - Prioritize end‑to‑end flows through parser/handlers and CLI parsing/rendering helpers
  - [x] 4.4 Run feature‑specific tests only
    - Run ONLY tests from 1.1, 2.1, 3.1, and 4.3 (expected total ≤ 20)

Acceptance Criteria:
- All feature‑specific tests pass (≤ 20 total)
- Critical flows covered (STATUS keys, budget boundaries, homing/step reset)
- No more than 8 additional tests added

## Execution Order
1. Firmware Telemetry & STATUS Extensions (Task Group 1)
2. Host Tooling (Interactive serial_cli.py) (Task Group 2)
3. Formatting & Build Validation (Task Group 3)
4. Test Review & Gap Analysis (Task Group 4)
