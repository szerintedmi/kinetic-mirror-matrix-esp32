## Summary
Implemented HOME command end-to-end: parsing with comma-skip support, 4 unit tests covering parsing, busy rule, concurrency, and post-state; and a hardware-backed homing sequence in `HardwareMotorController` that performs a negative run, backoff, center to midpoint, and sets runtime zero. Added parameterized on-device tests (TEST_MOTOR_ID) and simplified PlatformIO test routing.

## Files Touched
- `lib/MotorControl/include/MotorControl/HardwareMotorController.h` - Added homing plan state, helper, and declarations.
- `lib/MotorControl/src/HardwareMotorController.cpp` - Implemented HOME sequencing state machine and single-motor move helper.
- `test/test_MotorControl/test_HomeSequencing.cpp` - New unit tests for HOME parsing and behavior.
- `test/test_MotorControl/test_Core.cpp` - Registered new HOME tests with the suite runner.
- `agent-os/specs/2025-10-17-homing-baseline-calibration-bump-stop/tasks.md` - Marked 1.1–1.4 as completed.
- `test/test_OnDevice/test_Esp32Hardware.cpp` - New on-device tests using real backend; motor target parameterized via `TEST_MOTOR_ID`.
- `platformio.ini` - Route native vs on-device tests; enable easy motor selection via `-DTEST_MOTOR_ID` and keep native runs fast.

## Implementation Details

### Hardware HOME Sequencing
Location: `lib/MotorControl/src/HardwareMotorController.cpp`
- Added per-motor `HomingPlan {active, phase, overshoot, backoff, full_range, speed, accel}`.
- `homeMask(...)` now rejects if busy, derives `full_range` (default 2400), initializes plans, prepares DIR/SLEEP, latches once (native), and starts the first leg: negative run by `full_range + overshoot`.
- `tick(...)` advances the sequence when a leg completes:
  - Phase 0 → backoff: move positive by `backoff`.
  - Phase 1 → center: move positive by `full_range/2`.
  - Phase 2 → finalize: `setCurrentPosition(0)`, clear `moving`, keep WAKE/SLEEP parity with MOVE via existing logic.
- `startMoveSingle_` helper mirrors MOVE semantics for DIR/SLEEP and WAKE handling.

Rationale: Implements the requested bump-stop homing flow using absolute moves (adapter API) and internal state machine, while preserving existing WAKE/SLEEP behavior and BUSY rule.

### Unit Tests
Location: `test/test_MotorControl/test_HomeSequencing.cpp`
- `test_home_parsing_comma_skips` verifies comma-skip parsing (`HOME:2,,50`), immediate motion, and final zero/asleep state.
- `test_home_busy_rule_reject_when_moving` enforces BUSY rule when a targeted motor is moving.
- `test_home_all_concurrency_and_post_state` starts all eight, checks concurrent `moving=1`, then verifies `pos=0`, `moving=0`, `awake=0` for all after completion.
- `test_help_includes_home_line_again` confirms HELP includes the HOME grammar.

Rationale: Focused tests cover parsing, concurrency, busy rule, and post-state as specified; they run on the native stub backend for fast feedback.

## Testing

### Test Files Created/Updated
- `test/test_MotorControl/test_HomeSequencing.cpp` - New HOME tests as above.
- `test/test_MotorControl/test_Core.cpp` - Added RUN_TEST entries for new tests.
- `test/test_OnDevice/test_Esp32Hardware.cpp` - On-device WAKE/SLEEP, short MOVE, and HOME validation on real hardware.

### Test Coverage
- Unit tests (native): Added 4 tests for parsing, busy rule, concurrency, post-state.
- On-device tests (esp32dev): Added 3 hardware tests (WAKE/SLEEP, short MOVE, HOME to zero). Parameterizable motor under test.
- Edge cases covered: comma-skip parsing; ALL addressing; BUSY rejection; final state assertions.

### Manual Testing Performed
- Verified native unit tests: `pio test -e native` → all MotorControl and Drivers tests passed.
- Verified on-device tests on motor 7: `pio test -e esp32dev -O "build_flags = -Iinclude -DTEST_MOTOR_ID=7"`.
  - WAKE/SLEEP toggled correctly.
  - Short MOVE to +100 completed with expected final position.
  - HOME completed with final logical position 0 and asleep.

## User Standards & Preferences Compliance
- `@agent-os/standards/backend/hardware-abstraction.md`: Kept behavior behind `MotorController` interface; used adapter for motion and state.
- `@agent-os/standards/testing/unit-testing.md`: Small, focused tests; native stub used; deterministic timing.
- `@agent-os/standards/testing/hardware-validation.md`: Added on-device tests and parameterization for repeatable bench validation.
- `@agent-os/standards/global/conventions.md` and `coding-style.md`: Names, structure, and state machine align with existing patterns.

## Known Issues & Limitations
- The hardware homing timing and mid-point movement are heuristic without sensors; final `setCurrentPosition(0)` establishes logical baseline per spec.
- Native latch count for homing is not explicitly tested; existing latch behavior tests target MOVE.
 - On-device tests assume safe travel for the configured motor; adjust `TEST_MOTOR_ID` and ensure mechanics are clear before running.
