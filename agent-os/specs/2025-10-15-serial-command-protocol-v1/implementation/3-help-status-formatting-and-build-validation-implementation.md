# Task 3: HELP/STATUS Formatting and Build Validation

## Overview
Task Reference: Task Group 3 from `agent-os/specs/2025-10-15-serial-command-protocol-v1/tasks.md`

### Task Description
Validate HELP and STATUS formats via focused Unity tests, ensure firmware builds cleanly for `esp32dev`.

## Implementation Summary
- Added Unity tests to verify HELP usage lines and confirm no error codes are listed.
- Added Unity tests to verify STATUS prints 8 lines (one per motor) and includes id, pos, speed, accel, moving, awake.
- Performed build validation for `esp32dev`.

## Files Changed/Created
### New Files
- `test/test_protocol/test_help_status.cpp` — HELP/STATUS formatting tests

### Modified Files
- `test/test_protocol/test_core.cpp` — single test runner invokes all tests

## Test Details
- HELP tests check presence of:
  - `MOVE:<id|ALL>,<abs_steps>[,<speed>][,<accel>]`
  - `HOME:<id|ALL>[,<overshoot>][,<backoff>][,<speed>][,<accel>][,<full_range>]`
  - `STATUS`, `WAKE:<id|ALL>`, `SLEEP:<id|ALL>`
  - and absence of `E0`/`ERR`
- STATUS tests check:
  - total 8 lines
  - first line contains all keys: `id= pos= speed= accel= moving= awake=`

## Results
- Build: `pio run -e esp32dev` — success
- Tests: `pio test -e native` — 9 tests passed

## User Standards & Preferences Compliance
- Frontend/Serial Interface: ensured HELP clarity and STATUS readability as per `agent-os/standards/frontend/serial-interface.md`.
- Build Validation: followed `agent-os/standards/testing/build-validation.md` with a clean build on the target env.

## Notes
- The test runner is consolidated in `test_core.cpp` to avoid multiple `main()` definitions in native env.

