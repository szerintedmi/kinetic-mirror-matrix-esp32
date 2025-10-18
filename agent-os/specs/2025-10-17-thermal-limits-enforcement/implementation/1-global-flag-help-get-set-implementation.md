## Implementation Summary

### Components Changed
**Location:** lib/MotorControl/include/MotorControl/MotorCommandProcessor.h, lib/MotorControl/src/MotorCommandProcessor.cpp, test/test_MotorControl/test_Thermal.cpp, test/test_MotorControl/test_Runner.cpp

- Added global `thermal_limits_enabled_` flag (default ON) to `MotorCommandProcessor`.
- Implemented `GET THERMAL_LIMITING` and `SET THERMAL_LIMITING=OFF|ON` handlers.
- Updated parser to accept colon or space separation for GET/SET (e.g., `GET THERMAL_LIMITING` or `GET:THERMAL_LIMITING`).
- Extended `HELP` to list the new GET/SET commands and `GET LAST_OP_TIMING[:<id|ALL>]`.
- No changes were made to the per‑motor `STATUS` output.

## Testing
- Added `test/test_MotorControl/test_Thermal.cpp`:
  - Verifies `HELP` lists `GET THERMAL_LIMITING` and `SET THERMAL_LIMITING=OFF|ON`.
  - Verifies `GET THERMAL_LIMITING` returns `CTRL:OK THERMAL_LIMITING=ON max_budget_s=<MAX_RUNNING_TIME_S>` by default and toggles to OFF after a `SET`.
- Integrated into suite runner `test/test_MotorControl/test_Runner.cpp` (centralizes RUN_TEST order).

## Standards Compliance
- Followed serial interface convention of `<verb>[:payload]` while also supporting space for GET/SET to match the spec wording.
- Kept response formatting prefixed with `CTRL:` per interface standards.
- Maintained existing `STATUS` output keys and line structure (no meta/footer lines), per acceptance criteria.
