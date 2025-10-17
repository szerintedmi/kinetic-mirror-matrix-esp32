## Summary
- Extended per-motor state with homing and thermal budget metrics.
- Implemented budget spend/refill accounting and steps-since-home tracking.
- Updated STATUS formatting to include: `homed`, `steps_since_home`, `budget_s`, `ttfc_s`.
- Added Unity tests covering new keys and basic invariants.
 - Introduced centralized constants header and aligned tests to constants.

## Changes
### MotorState extensions
Location: `lib/MotorControl/include/MotorControl/MotorController.h`
- Added fields: `homed`, `steps_since_home`, `budget_tenths`, `last_update_ms`.

### Stub controller logic
Location: `lib/MotorControl/src/StubMotorController.{h,cpp}`
- Added `MovePlan::is_home` and `start_pos` to differentiate HOME vs MOVE.
- Initialize new state fields; set budget to 90.0s (900 tenths).
- Tick: event-driven budget bookkeeping; complete moves; update `steps_since_home` and `homed` on HOME completion.

### Hardware controller logic
Location: `lib/MotorControl/src/HardwareMotorController.cpp`
- Initialize new state fields (same defaults as stub).
- Tick: budget bookkeeping (tenths, applied in whole seconds); accumulate `steps_since_home` using absolute delta only when `homed=1`; set `homed=1` and reset steps on HOME completion.

### STATUS formatting
Location: `lib/MotorControl/src/MotorCommandProcessor.cpp`
- STATUS renders: `id pos moving awake homed steps_since_home budget_s ttfc_s speed accel`.
- `budget_s`: clamped to `[0..MAX_RUNNING_TIME_S]`, displayed with 1 decimal.
- `ttfc_s`: computed from true deficit with a hard runtime cap — while ON, the internal budget floor ensures the maximum cooldown will not exceed `MAX_COOL_DOWN_TIME_S`. Display shows one decimal and also caps to `MAX_COOL_DOWN_TIME_S`. Tick is invoked before formatting to ensure up-to-date values.

### Centralized constants and style
Locations:
- `lib/MotorControl/include/MotorControl/MotorControlConstants.h`
- Replaced magic numbers (limits, defaults, budget params) with constexpr constants in controllers and processor.
- Removed `using namespace` in production and used explicit `MotorControlConstants::` qualifiers.

## Testing
New files:
- `test/test_MotorControl/test_StatusBudget.cpp`

Tests validate:
- New STATUS keys are present.
- Budget spending while awake (~1.0 s/s) and refilling while asleep (1.5 s/s), clamped to 90s.
- `ttfc_s` non-negative and zeros out when at max.
- `homed` flag and `steps_since_home` reset on HOME; increments with MOVE.
- Tests reference shared constants (`MAX_RUNNING_TIME_S`, position limits, default speed/accel) to avoid magic numbers.

How to run only these tests (native):
- `pio test -e native -v --filter test_MotorControl`

## Standards Compliance
- No floats used; fixed-point tenths for budget metrics per `@agent-os/standards/global/resource-management.md`.
- Kept STATUS single-line-per-motor printable format per `@agent-os/standards/frontend/serial-interface.md`.
- Added concise, role-focused unit tests per `@agent-os/standards/testing/unit-testing.md`.
 - Centralized parameters and avoided magic numbers, added brief newcomer-oriented comments.
