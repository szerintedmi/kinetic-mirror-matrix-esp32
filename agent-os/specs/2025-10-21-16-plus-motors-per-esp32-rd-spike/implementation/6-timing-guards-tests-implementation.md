# Task 6: Unit Tests for Timing and Guards

## Overview
**Task Reference:** Task #6 from `agent-os/specs/2025-10-21-16-plus-motors-per-esp32-rd-spike/tasks.md`
**Implemented By:** testing-engineer
**Date:** 2025-10-23
**Status:** ✅ Complete

### Task Description
Consolidate and extend unit tests for shared-STEP timing helpers and DIR/SLEEP guard windows. Add edge-case coverage and ensure host-native tests run under `env:native`.

## Implementation Summary
Reviewed existing timing and guards tests from earlier groups and added six focused edge-case tests. These validate guard feasibility thresholds, flip window spacing, alignment boundaries, zero-speed behavior, and rounding for stopping distances. Registered the new tests in the MotorControl Unity harness and validated the `env:native` test run.

## Files Changed/Created

### New Files
- `test/test_MotorControl/test_SharedStepEdgeCases.cpp` - Six new edge-case tests for timing and guard helpers.

### Modified Files
- `test/test_MotorControl/test_main.cpp` - Declared and registered the new tests in the Unity test runner.
- `agent-os/specs/2025-10-21-16-plus-motors-per-esp32-rd-spike/tasks.md` - Marked Task Group 6 items complete.

## Key Implementation Details

### Edge-case timing/guards tests
**Location:** `test/test_MotorControl/test_SharedStepEdgeCases.cpp`

- `test_shared_timing_period_zero`: Confirms `step_period_us(0)` returns 0 (generator stop signal).
- `test_guard_fit_thresholds`: Validates strict threshold at `pre+post+2` (fails) and `+3` (passes).
- `test_compute_flip_window_too_tight_period`: Ensures no flip window is produced when period is too small.
- `test_compute_flip_window_spacing_matches_constants`: Asserts spacing equals `DIR_GUARD_US_PRE/POST`.
- `test_align_next_edge_near_boundary`: Confirms alignment from just-before-edge to the next edge.
- `test_stop_distance_round_up_small_values`: Checks ceil rounding of stopping distance at very low v/a.

**Rationale:** These tests harden correctness at boundaries and build confidence in guard scheduling during tight margins and changing speeds.

## Testing

### Native Test Run
Executed: `pio test -e native -f test_MotorControl`
- Result: 77 tests passed, 0 failures.

### Coverage
- Unit tests: ✅ Complete (timing helpers and DIR guard windows)
- Integration tests: ❌ None (out of scope)

## User Standards & Preferences Compliance

### Unit Testing
**File Reference:** `agent-os/standards/testing/unit-testing.md`

**How Your Implementation Complies:**
Focused, deterministic, host-only tests using Unity. Clear naming, each test validates one behavior.

### Build Validation
**File Reference:** `agent-os/standards/testing/build-validation.md`

**How Your Implementation Complies:**
Used `env:native` to compile and run tests without device dependencies; ensured no new libs required.

### Conventions
**File Reference:** `agent-os/standards/global/conventions.md`

**How Your Implementation Complies:**
Followed existing test structure, added prototypes to centralized `test_main.cpp`, kept diffs minimal.

## Integration Points
- Timing helpers: `SharedStepTiming` (periods, alignment, stopping distance)
- Guard helpers: `SharedStepGuards` (flip window computation)

