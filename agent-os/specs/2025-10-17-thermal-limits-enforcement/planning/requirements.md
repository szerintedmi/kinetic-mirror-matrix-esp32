# Spec Requirements: Thermal Limits Enforcement

## Initial Description
yes, thermal limits enforcement

## Requirements Discussion

### First Round Questions

**Q1:** Budget basis: per-motor, time-based limits using STATUS-tracked motion time (sliding window vs max consecutive run)?
**Answer:** Per motor, time-based. Metrics exist from previous item: `budget_s` and `ttfc_s` (per `agent-os/specs/2025-10-17-status-diagnostics/spec.md`). Actuals are already calculated and shown via STATUS.

**Q2:** Defaults proposal (e.g., window 60s, max run 20s, cooldown 30s)?
**Answer:** Defaults already set in code: `MAX_RUNNING_TIME_S`, `FULL_COOL_DOWN_TIME_S`, `REFILL_RATE` in `lib/MotorControl/include/MotorControl/MotorControlConstants.h`.

**Q3:** Over-limit behavior (soft-stop vs pre-reject)? WAKE handling?
**Answer:** Do not slow down. Reject MOVE and HOME in advance if they would exceed limit based on deterministic budget calc from speed/accel. WAKE cannot be pre-guarded (unknown sleep time); implement auto-shutdown if runtime overruns budget by configured amount, also cancelling active moves as guardrail. Introduce new constant: `AUTO_SLEEP_IF_OVER_BUDGET_S`.

**Q4:** Config surface (commands to tune thresholds) and global enable?
**Answer:** No runtime tuning; use code constants. Add global ON/OFF: `SET THERMAL_RUNTIME_LIMITING=OFF|ON` (default ON). Expose via `GET THERMAL_RUNTIME_LIMITING`; keep STATUS unchanged.

**Q5:** Error semantics for rejections and WAKE?
**Answer:** Multiple reasons: (a) Command beyond potential max budget → include max budget in error. (b) Command within range but not enough current budget → include `budget_s`, `ttfc_s`, and max budget. (c) WAKE can only be rejected when no budget left.

**Q6:** Overrides scope?
**Answer:** Global override only (see Q4).

**Q7:** Persistence?
**Answer:** None (no persistence).

**Q8:** CLI/TUI changes?
**Answer:** Metrics already implemented; only add limit override status (global ON/OFF) to views.

**Q9:** Exclusions?
**Answer:** None (no excluded scenarios).

### Existing Code to Reference
- Metrics: `budget_s`, `ttfc_s` per `agent-os/specs/2025-10-17-status-diagnostics/spec.md` and existing STATUS implementation.
- Constants: `MAX_RUNNING_TIME_S`, `FULL_COOL_DOWN_TIME_S`, `REFILL_RATE` in `lib/MotorControl/include/MotorControl/MotorControlConstants.h`.
- New constant to add: `AUTO_SLEEP_IF_OVER_BUDGET_S` (seconds beyond budget before forced SLEEP + cancel moves).
- Command processor: extend to handle `SET THERMAL_RUNTIME_LIMITING=OFF|ON` and `GET THERMAL_RUNTIME_LIMITING`; do not change STATUS format.

## Visual Assets

No visual assets provided.

## Requirements Summary

### Functional Requirements
- Enforce per-motor, time-based thermal runtime budgets using existing `budget_s` and `ttfc_s` metrics.
- Use compile-time constants: `MAX_RUNNING_TIME_S`, `FULL_COOL_DOWN_TIME_S`, `REFILL_RATE`; introduce `AUTO_SLEEP_IF_OVER_BUDGET_S`.
- Pre-flight guardrails for MOVE and HOME: deterministically compute required budget from planned motion (speed, accel, steps) and reject if it would exceed budget.
- WAKE handling: allow unless no budget remains; if runtime exceeds budget by `AUTO_SLEEP_IF_OVER_BUDGET_S`, auto-SLEEP the motor and cancel active moves.
- Global enable/disable via `SET THERMAL_RUNTIME_LIMITING=OFF|ON` (default ON). Expose via `GET THERMAL_RUNTIME_LIMITING` and show in CLI header/footer; do not alter STATUS rows.
- Error responses:
  - Beyond potential max budget: include `max_budget_s`.
  - Insufficient current budget: include `budget_s`, `ttfc_s`, and `max_budget_s`.
  - WAKE rejection only when `budget_s` is 0.

- MOVE/HOME success responses include device-side `est_ms` so host tools do not re-compute:
  - `CTRL:OK est_ms=<ms>`
  - When limits are disabled and a pre-check would have warned: `CTRL:WARN ...` then the final `CTRL:OK est_ms=...`.

### Non-Functional Requirements
- No motion speed reductions; maintain configured speed profiles (avoid underpowering/step loss).
- Low runtime overhead; integrate with existing MotorControl timing without jitter.
- Clear, consistent error codes/messages to support automation and CLI UX.

### Reusability Opportunities
- Reuse STATUS metrics infrastructure; add a lightweight GET endpoint for the global flag; CLI displays this separately.
- Reuse command parsing patterns from existing SET/CONFIG style handlers (if any) in MotorCommandProcessor.

### Scope Boundaries
**In Scope:**
- Firmware-side enforcement, new constant, new global command flag, STATUS/CLI exposure, deterministic pre-checks, WAKE auto-shutdown.

**Out of Scope:**
- Runtime editing of thresholds (remain compile-time constants).
- Persistence of settings across reboots.
- Per-motor overrides or exemptions.

### Technical Considerations
- Implement deterministic budget estimation for planned MOVE/HOME using current speed/accel and step counts.
- Add runtime monitor to detect overrun and trigger auto-SLEEP + move cancellation.
- Update `MotorCommandProcessor` to parse `SET THERMAL_RUNTIME_LIMITING=OFF|ON` and handle `GET THERMAL_RUNTIME_LIMITING`; leave STATUS formatting unchanged.
- Update Python CLI to show global limit status; existing metrics remain unchanged.
- For MOVE/HOME tooling, host must consume `est_ms` returned by device (no host-side re-estimation).
 - Provide `GET LAST_OP_TIMING[:<id|ALL>]` for device-side timing:
   - With `<id>`: `CTRL:OK LAST_OP_TIMING ongoing=0|1 id=<id> est_ms=<N> started_ms=<ms> [actual_ms=<N>]`
   - With no param or `ALL`: multi-line block with header `LAST_OP_TIMING` then per-motor lines: `id=<n> ongoing=0|1 est_ms=<N> started_ms=<ms> [actual_ms=<N>]`
   - Host `check-move`/`check-home` poll until completion and use `actual_ms` for ±100 ms validation.

### Testing
- Unit tests assert `CTRL:OK est_ms=...` for MOVE/HOME and that WARN paths still end with `CTRL:OK est_ms=...`.
- CLI tests validate HOME placeholder serialization and that device-provided `est_ms` is parsed by check-move/check-home commands.
