Implementation Notes: Task Group 4 — Auto-sleep overrun and WAKE handling

Summary
- Added grace constant `AUTO_SLEEP_IF_OVER_BUDGET_S` to `MotorControlConstants`.
- Propagated global thermal flag to controllers via `MotorController::setThermalLimitsEnabled`.
- Enforced auto-sleep in controllers’ `tick()` when `budget_tenths < -AUTO_SLEEP_IF_OVER_BUDGET_S*10` and limits enabled.
- Implemented WAKE rejection (`E12`) when no budget and WARN+OK when disabled.
- Added unit tests for WAKE rejection/WARN and auto-sleep cancellation on overrun.

Key Changes
- lib/MotorControl/include/MotorControl/MotorControlConstants.h
  - New `constexpr int32_t AUTO_SLEEP_IF_OVER_BUDGET_S = 5`.

- lib/MotorControl/include/MotorControl/MotorController.h
  - Added `setThermalLimitsEnabled(bool)` to surface the global flag to controllers.

- lib/MotorControl/src/StubMotorController.*
  - Stored `thermal_limits_enabled_`; cancel active plan and force sleep when overrun exceeds grace.
  - Idempotent cancellation: only finalizes `last_op_*` once.

- lib/MotorControl/include/MotorControl/HardwareMotorController.h/.cpp
  - Stored `thermal_limits_enabled_` and enforced forced sleep on overrun.
  - On hardware, outputs are disabled and WAKE override cleared. Homing plan is stopped. (Underlying engine stop is not exposed by adapter; moving flag is cleared.)

- lib/MotorControl/src/MotorCommandProcessor.cpp
  - Initialized controller with current flag; kept in sync on SET.
  - WAKE: reject with `CTRL:ERR E12 THERMAL_NO_BUDGET_WAKE` when enabled and no budget; when disabled, emit `CTRL:WARN THERMAL_NO_BUDGET_WAKE` then `CTRL:OK`.

Tests
- test/test_MotorControl/test_Thermal.cpp
  - `test_wake_reject_enabled_no_budget`: drains budget, sleeps, then rejects WAKE with E12.
  - `test_wake_warn_disabled_no_budget_then_ok`: disabled mode emits WARN then OK.
  - `test_auto_sleep_overrun_cancels_move_and_awake`: starts long move with limits OFF, re-enables, advances past grace; verifies `awake=0` and `moving=0` for id=0.

Notes
- The hardware adapter interface does not expose an explicit stop; the controller disables outputs and clears overriding flags, and marks the operation complete. This is sufficient for unit tests (stub) and safe behavior on device.
