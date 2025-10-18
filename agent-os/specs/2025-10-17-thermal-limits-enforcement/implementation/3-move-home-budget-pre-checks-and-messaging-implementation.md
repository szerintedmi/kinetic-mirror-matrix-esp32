## Implementation Summary

### Components Changed
**Location:** lib/MotorControl/src/MotorCommandProcessor.cpp, lib/MotorControl/include/MotorControl/MotorController.h, lib/MotorControl/src/StubMotorController.cpp, lib/MotorControl/src/HardwareMotorController.cpp, tools/serial_cli/__init__.py, test/test_MotorControl/test_Thermal.cpp, test/test_MotorControl/test_Runner.cpp

- Added MOVE/HOME pre-flight budget checks using `MotionKinematics`:
  - MOVE: Per-target motor distance estimated via `estimateMoveTimeMs` with ceil-to-seconds.
  - HOME: Estimate via `estimateHomeTimeMsWithFullRange` (overshoot + backoff + center), ceil-to-seconds; default `full_range` if omitted.
- Implemented checks:
  - E10: `CTRL:ERR E10 THERMAL_REQ_GT_MAX id=<id> req_ms=<N> max_budget_s=<MAX>` when runtime exceeds max.
  - E11: `CTRL:ERR E11 THERMAL_NO_BUDGET id=<id> req_ms=<N> budget_s=<cur> ttfc_s=<ttfc>` when current budget is insufficient.
  - When limits disabled: `CTRL:WARN THERMAL_* ...` then proceed; final `CTRL:OK` includes `est_ms`.
- Parser flow unaffected; STATUS format unchanged; only MOVE/HOME handlers changed.
 - CLI alignment:
   - HOME serialization preserves empty placeholders (e.g., `HOME:0,,,1`).
   - `check-move`/`check-home` poll `GET LAST_OP_TIMING` and use `actual_ms` for ±100 ms window.

## Testing
- Added tests in `test/test_MotorControl/test_Thermal.cpp`:
  - `test_preflight_e10_move_enabled_err` (MOVE too long at `speed=1` → E10)
  - `test_preflight_e11_move_enabled_err` (budget drained via WAKE + time → E11)
  - `test_preflight_warn_when_disabled_then_ok` (limits OFF → WARN then OK for E10/E11 scenarios)
  - `test_preflight_e10_home_enabled_err` (HOME at `speed=1` → E10)
  - `test_last_op_timing_move`, `test_last_op_timing_home` (device timing endpoint)
- Runner executes these along with existing suites.
- Verified via `pio test -e native`: all tests passed.
 - CLI mapping tests updated; added case for `home 0 --speed 1` to ensure placeholders (`HOME:0,,,1`).

## Standards Compliance
- Kept user-facing response format consistent with `CTRL:` prefix and simple key/value payloads.
- No changes to STATUS lines per acceptance criteria.
- Pre-flight checks run deterministically before controller invocation; warnings honor the global disable flag.
