## Implementation Summary

### Components Changed
**Location:** lib/MotorControl/include/MotorControl/MotionKinematics.h, lib/MotorControl/src/MotionKinematics.cpp, lib/MotorControl/src/StubMotorController.cpp, test/test_MotorControl/test_KinematicsAndStub.cpp, test/test_MotorControl/test_ProtocolBasicsAndMotion.cpp, test/test_MotorControl/test_StatusAndBudget.cpp, test/test_MotorControl/test_Runner.cpp

- Added MotionKinematics helper with `estimateMoveTimeMs(distance, speed, accel)` and `estimateHomeTimeMs(overshoot, backoff, speed, accel)`.
- Estimator uses triangular/trapezoidal profiles with integer math and ceil rounding; avoids floating point and provides conservative durations.
- Integrated estimator into `StubMotorController` for MOVE and HOME duration planning.
- Adjusted tests that assumed fixed HOME duration to use estimator-derived timing and added new estimator-specific tests.

## Testing
- New tests in `test/test_MotorControl/test_KinematicsAndStub.cpp`:
  - `test_estimator_trapezoidal_matches_simple_formula` validates trapezoidal formula equivalence.
  - `test_estimator_triangular_above_naive_bound` ensures conservative result vs simple distance/speed bound.
  - `test_stub_move_uses_estimator_duration` and `test_stub_home_uses_estimator_duration` verify stub schedules align with estimator outputs.
- Updated timing in existing tests to use estimator for HOME completion:
  - `test_home_defaults` (ProtocolBasicsAndMotion)
  - `test_home_and_steps_since_home`, `test_homed_resets_on_reboot`, `test_steps_since_home_resets_after_second_home` (StatusAndBudget)
- Consolidated runner: `test/test_MotorControl/test_Runner.cpp` invokes all tests in a fixed order.
- Ran native test suite: all tests passed.

## Standards Compliance
- Followed backend hardware abstraction by keeping kinematics in a shared helper under `lib/MotorControl`.
- Preserved response formats and existing behavior; only internal scheduling changed to use estimator.
- Unit tests align with testing standards and validate conservative timing and stub integration.
