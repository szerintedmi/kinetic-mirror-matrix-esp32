# Task 4: Test Review & Gap Analysis

## Overview
Task Reference: Task Group 4 from `agent-os/specs/2025-10-15-serial-command-protocol-v1/tasks.md`

### Task Description
Review tests from Task Groups 1–3, identify gaps, add up to 10 strategic tests, and run only spec-related tests.

## Gaps Identified
- SLEEP busy path not covered.
- Out-of-range rejection for `MOVE:ALL` not tested.
- WAKE/SLEEP single-id path with STATUS reflection not covered.
- HOME with all parameters provided (no clamping) not verified.
- MOVE with explicit speed/accel not reflected in STATUS checked.

## Additions
Added 5 focused tests in `test/test_protocol/test_core.cpp`:
1. `test_sleep_busy_then_ok` — SLEEP returns BUSY while moving, then OK after completion
2. `test_move_all_out_of_range` — `MOVE:ALL,1300` rejected with `E07 POS_OUT_OF_RANGE`
3. `test_wake_sleep_single_and_status` — WAKE:1 sets `awake=1`; SLEEP:1 sets `awake=0`
4. `test_home_with_params_acceptance` — `HOME:0,900,150,500,1000,2400` accepted
5. `test_move_sets_speed_accel_in_status` — MOVE sets speed/accel visible in STATUS for id=0

## Results
- Total tests now: 14 (within limit)
- Native tests: `pio test -e native` — all passed
- Build validation: `pio run -e esp32dev` — success

## Acceptance Criteria
- All feature-specific tests pass: ✅
- Critical workflows covered (busy, ALL, out-of-range, HELP/STATUS formatting): ✅
- Additional tests ≤ 10: ✅ (added 5)

## Notes
- Tests are consolidated under a single runner to avoid duplicate `main()` on native builds.

