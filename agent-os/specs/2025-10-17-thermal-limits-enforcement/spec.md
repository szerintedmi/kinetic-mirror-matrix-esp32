# Specification: Thermal Limits Enforcement

## Goal

Protect each motor from overheating by enforcing per‑motor, time‑based runtime budgets. Reject MOVE/HOME that would exceed budget and auto‑sleep motors that overrun, while exposing clear diagnostics and a global on/off switch.

## User Stories

- As a maker, I want motors to stop before overheating so my rig can run longer without damage.
- As a developer, I want deterministic rejections with actionable error details so scripts can adapt or wait.
- As a bench tester, I want to toggle enforcement for short lab experiments and see that reflected in tooling

## Core Requirements

### Functional Requirements

- Enforce per‑motor, time‑based thermal budgets using existing metrics: `budget_s` (remaining, can be negative) and `ttfc_s` (time‑to‑full‑cooldown).
- Use compile‑time constants already defined: `MAX_RUNNING_TIME_S`, `FULL_COOL_DOWN_TIME_S`, `REFILL_RATE` (via `REFILL_TENTHS_PER_SEC`).
- Introduce new constant `AUTO_SLEEP_IF_OVER_BUDGET_S` to define grace seconds beyond zero budget before forced sleep.
- MOVE/HOME pre‑check: deterministically estimate required runtime from requested motion (speed, accel, distance) and reject if it would exceed available budget or even the potential max budget for that motor.
- WAKE: allow unless the motor has no budget left; if the motor stays awake long enough to exceed `AUTO_SLEEP_IF_OVER_BUDGET_S` below zero, auto‑sleep it and cancel any active move(s).
- Global enable/disable command: `SET THERMAL_RUNTIME_LIMITING=OFF|ON` (default ON). When OFF, no rejections or auto‑sleep occur. Expose current setting via a dedicated `GET THERMAL_RUNTIME_LIMITING` query; keep STATUS per‑motor only.
- STATUS remains per‑motor lines as it is now

### Non-Functional Requirements

- Maintain motion profiles (no throttling or slowdowns); never alter configured speed/accel to enforce limits.
- Low overhead checks; avoid jitter in control loops and keep budget/timing math integer‑friendly.
- Clear, consistent errors following existing protocol prefixing (`CTRL:ERR ...`).

## Visual Design

No visual assets for this feature.

## Reusable Components

### Existing Code to Leverage

- Budget metrics and STATUS formatting in `MotorCommandProcessor::handleSTATUS()` and `MotorState.budget_tenths` accounting in controllers.
- Compile‑time constants in `lib/MotorControl/include/MotorControl/MotorControlConstants.h`.
- Command parsing and response style in `MotorCommandProcessor` (`HELP`, `STATUS`, `MOVE`, `HOME`, `WAKE`, `SLEEP`).

### New Components Required

- Global runtime‑limits flag (default ON) stored centrally and respected by both command pre‑checks and controller runtime enforcement.
- Deterministic motion time estimator (triangular/trapezoidal) implemented once in a shared helper and used by both pre‑checks and controller scheduling (including the stub).

## Technical Approach

- Constants
  - Add `AUTO_SLEEP_IF_OVER_BUDGET_S` (int seconds) to `MotorControlConstants`.

- Motion Kinematics Helper
  - Add `lib/MotorControl/include/MotorControl/MotionKinematics.h` and corresponding `.cpp`.
  - Provide `estimateMoveTimeMs(distance_steps, speed_sps, accel_sps2)` using triangular/trapezoidal profiles with integer math and ceil for conservative bounds.
  - Use this helper for MOVE/HOME pre‑checks and for `StubMotorController` plan end‑time scheduling.

- Global Flag and Command
  - Add a boolean `thermal_limits_enabled` (default true) maintained by the command processor and visible to controllers.
  - Add parser for `SET THERMAL_RUNTIME_LIMITING=OFF|ON`; update HELP to include this command; return `CTRL:OK` or `CTRL:ERR E03 BAD_PARAM` on invalid values.
  - Add `GET THERMAL_RUNTIME_LIMITING` returning `CTRL:OK THERMAL_RUNTIME_LIMITING=ON|OFF max_budget_s=<N>`; include in HELP. Do not alter STATUS rows.

- Pre‑flight Guardrails
  - MOVE/HOME: before invoking `controller_->moveAbsMask/homeMask`, estimate runtime per addressed motor using requested `speed`/`accel` and distance (for HOME, sum of constituent segments). Compare to:
    - Potential maximum budget (`MAX_RUNNING_TIME_S`) → if runtime exceeds this, reject with code and include `max_budget_s`.
    - Current available budget (`budget_s` when awake or asleep, using controller state after `tick(now_ms)`) → if insufficient, reject with code and include `budget_s`, `ttfc_s`, and `max_budget_s`.
  - When `thermal_limits_enabled == false`, skip all thermal pre‑checks and proceed as today.

- Runtime Enforcement (WAKE and overrun)
  - Controllers continue to decrement budget while awake and refill while asleep.
  - If `thermal_limits_enabled == true` and any motor’s `budget_tenths` drops below `-AUTO_SLEEP_IF_OVER_BUDGET_S * 10`, force that motor asleep and cancel its active move plan(s).
  - WAKE rejection: if a target motor has no budget left at the time of WAKE and limits are enabled, reject WAKE.

- Errors and Messaging
  - Keep protocol prefix: `CTRL:ERR`.
  - New error codes and payloads:
    - `E10 THERMAL_BEYOND_MAX_BUDGET max_budget_s=<N>` (MOVE/HOME estimated runtime exceeds potential maximum budget)
    - `E11 THERMAL_INSUFFICIENT_BUDGET budget_s=<B> ttfc_s=<T> max_budget_s=<N>` (MOVE/HOME would exceed current budget)
    - `E12 THERMAL_NO_BUDGET_WAKE` (WAKE rejected due to zero/negative budget)
  - When `thermal_limits_enabled == false`, still emit non-blocking warnings using the same schema as errors, but prefixed with `CTRL:WARN` instead of `CTRL:ERR`. Commands proceed and end with `CTRL:OK`.
    - `CTRL:WARN E10 THERMAL_BEYOND_MAX_BUDGET max_budget_s=<N>`
    - `CTRL:WARN E11 THERMAL_INSUFFICIENT_BUDGET budget_s=<B> ttfc_s=<T> max_budget_s=<N>`
    - `CTRL:WARN E12 THERMAL_NO_BUDGET_WAKE`
    - Ordering: emit any `CTRL:WARN ...` lines first, then the final `CTRL:OK` so clients can display warnings without breaking success detection.

- STATUS/CLI
  - STATUS: unchanged per‑motor lines only; no trailing meta line.
  - Python CLI: fetch `GET THERMAL_RUNTIME_LIMITING` on startup and after toggles to display the flag (header/footer). Keep existing `budget_s`/`ttfc_s` columns. When `CTRL:WARN ...` appears alongside `CTRL:OK`, surface the warning text in the log/output.

- Testing
  - Unit tests for:
    - `HELP` lists both `SET THERMAL_RUNTIME_LIMITING=OFF|ON` and `GET THERMAL_RUNTIME_LIMITING`; `GET` returns `CTRL:OK THERMAL_RUNTIME_LIMITING=ON|OFF max_budget_s=<N>`.
    - MOVE rejection with `E10` when estimated runtime > `MAX_RUNNING_TIME_S`.
    - MOVE rejection with `E11` when current `budget_s` is insufficient (include fields).
    - WAKE rejected with `E12` when no budget left and limits enabled.
    - Auto‑sleep after `AUTO_SLEEP_IF_OVER_BUDGET_S` overrun cancels active move and sets `awake=0`.
    - Disabling limits via `SET THERMAL_RUNTIME_LIMITING=OFF` emits the corresponding `CTRL:WARN E10/E11/E12 ...` followed by a final `CTRL:OK` (verify WARN lines precede OK and payload fields match error schema).
    - MotionKinematics: estimator returns conservative time; used by both pre‑checks and stub scheduling so planned end times are consistent with estimates (within expected ceil rounding).
    - CLI: on startup, issues `GET THERMAL_RUNTIME_LIMITING` and renders the flag; when a command returns `CTRL:WARN ...` and `CTRL:OK`, the Python CLI surfaces the warning text without treating the command as failure.

## Out of Scope

- Runtime tuning of thresholds (remain compile‑time constants).
- Persistence of settings across reboots.
- Per‑motor overrides or exemptions.

## Success Criteria

- Per‑motor lines in STATUS continue to show `budget_s` and `ttfc_s`; no meta summary lines introduced.
- `GET THERMAL_RUNTIME_LIMITING` returns the current flag and `max_budget_s`; `SET ...` toggles it.
- MOVE/HOME are rejected deterministically with `CTRL:ERR` codes `E10`/`E11` and documented fields; WAKE rejected with `E12` when appropriate.
- When the motor overruns by more than `AUTO_SLEEP_IF_OVER_BUDGET_S`, it is force‑slept and any active move is cancelled.
- CLI displays the flag using GET; no changes to STATUS parsing required.
