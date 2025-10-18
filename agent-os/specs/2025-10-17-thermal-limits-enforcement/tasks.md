# Task Breakdown: Thermal Limits Enforcement

## Overview
Total Tasks: 22
Assigned roles: api-engineer, ui-designer, testing-engineer

## Task List

### Firmware: Protocol & Status

#### Task Group 1: Global flag, HELP, GET/SET
**Assigned implementer:** api-engineer
**Dependencies:** None

- [x] 1.0 Add global `thermal_limits_enabled` (default ON)
- [x] 1.1 Parse `SET THERMAL_LIMITING=OFF|ON` in `MotorCommandProcessor`
- [x] 1.2 Update `HELP` to include both `SET THERMAL_LIMITING=OFF|ON` and `GET THERMAL_LIMITING`
- [x] 1.3 Implement `GET THERMAL_LIMITING` → `CTRL:OK THERMAL_LIMITING=ON|OFF max_budget_s=<N>`
- [x] 1.4 Keep existing per-motor STATUS output unchanged (no meta lines)
- [x] 1.5 Unit tests: HELP lists GET/SET; GET returns expected payload

**Acceptance Criteria:**
- `HELP` lists `SET THERMAL_LIMITING=OFF|ON` and `GET THERMAL_LIMITING`
- STATUS per-motor lines unchanged; no trailing meta lines
- `GET THERMAL_LIMITING` returns `ON` by default and correct `max_budget_s`

### Firmware: Motion Kinematics

#### Task Group 2: Shared estimator helper and integration
**Assigned implementer:** api-engineer
**Dependencies:** Task Group 1

- [x] 2.0 Add `lib/MotorControl/include/MotorControl/MotionKinematics.h` and `.cpp`
- [x] 2.1 Implement `estimateMoveTimeMs(distance_steps, speed_sps, accel_sps2)` with triangular/trapezoidal profile; integer math with ceil
- [x] 2.2 Replace stub duration calc with estimator in `StubMotorController`
- [x] 2.3 Provide HOME leg-by-leg estimate utility for pre-checks
- [x] 2.4 Unit tests: estimator conservative vs simple bounds; stub schedule uses estimator (durations match within ceil rounding)

**Acceptance Criteria:**
- Estimator compiles and returns conservative time for both short (triangular) and long (trapezoidal) moves
- Stub plans’ end times align with estimator outputs

### Firmware: Pre-flight Guardrails

#### Task Group 3: MOVE/HOME budget pre-checks and messaging
**Assigned implementer:** api-engineer
**Dependencies:** Task Groups 1–2

- [x] 3.0 In `MotorCommandProcessor`, before invoking controller, compute per‑motor required runtime using estimator
- [x] 3.1 Compare to max budget (`MAX_RUNNING_TIME_S`) → reject with `CTRL:ERR E10 ...` (or `CTRL:WARN` when limits disabled)
- [x] 3.2 Compare to current budget (`budget_s`, `ttfc_s`) after `tick(now_ms)` → reject with `CTRL:ERR E11 ...` (or WARN)
- [x] 3.3 Ensure WARN behavior when limits disabled: emit `CTRL:WARN ...` lines then `CTRL:OK`
- [x] 3.4 Unit tests for E10/E11 errors and WARN variants

**Acceptance Criteria:**
- Deterministic rejections for MOVE/HOME with proper payload fields
- When disabled, warnings are emitted and command succeeds with `CTRL:OK`

### Firmware: Runtime Enforcement

#### Task Group 4: Auto-sleep overrun and WAKE handling
**Assigned implementer:** api-engineer
**Dependencies:** Task Group 1

- [x] 4.0 Add `AUTO_SLEEP_IF_OVER_BUDGET_S` constant to `MotorControlConstants`
- [x] 4.1 In controllers’ `tick()`, when enabled and `budget_tenths < -AUTO_SLEEP_IF_OVER_BUDGET_S*10`, force SLEEP and cancel any active move (stub and hardware)
- [x] 4.2 Reject `WAKE` with `CTRL:ERR E12 THERMAL_NO_BUDGET_WAKE` when no budget (or WARN when disabled)
- [x] 4.3 Ensure move cancellation is safe and idempotent
- [x] 4.4 Unit tests: auto-sleep after overrun cancels active plan (awake=0, moving=0); WAKE rejection/warn paths

**Acceptance Criteria:**
- Motors are force-slept after grace overrun; active moves stop
- WAKE rejected only when enabled and no budget; WARN observed when disabled

### Host CLI/TUI

#### Task Group 5: CLI display and warning surfacing
**Assigned implementer:** ui-designer
**Dependencies:** Task Group 1

- [x] 5.0 Show `thermal_limits` global flag (header/footer) using `GET THERMAL_LIMITING`
- [x] 5.1 Surface any `CTRL:WARN ...` lines alongside `CTRL:OK` in logs/output
- [x] 5.2 Keep existing STATUS table columns unchanged
- [x] 5.3 On startup and after toggles, issue `GET THERMAL_LIMITING` and render flag
- [x] 5.4 Tests: verify WARN lines are captured and rendered without treating as failure

**Acceptance Criteria:**
- CLI shows thermal flag state
- WARN lines appear in output; commands remain successful

### Testing

#### Task Group 6: Test Review & Gap Analysis
**Assigned implementer:** testing-engineer
**Dependencies:** Task Groups 1–5

- [ ] 6.0 Review unit tests from Task Groups 1–5
- [ ] 6.1 Identify any critical gaps (e.g., corner kinematics, WARN/ERR ordering)
- [ ] 6.2 Add up to 10 integration/feature tests to cover end-to-end flows (STATUS → SET OFF → MOVE beyond budget → WARN + OK; SET ON → same → ERR)
- [ ] 6.3 Run only feature-specific tests

**Acceptance Criteria:**
- All feature-specific tests pass
- WARN/ERR ordering validated (WARN precedes OK)
- Estimator consistency validated via behaviors, not numerical internals

## Execution Order

Recommended implementation sequence:
1. Firmware: Protocol & Status (Task Group 1)
2. Firmware: Motion Kinematics (Task Group 2)
3. Firmware: Pre-flight Guardrails (Task Group 3)
4. Firmware: Runtime Enforcement (Task Group 4)
5. Host CLI/TUI (Task Group 5)
6. Test Review & Gap Analysis (Task Group 6)
