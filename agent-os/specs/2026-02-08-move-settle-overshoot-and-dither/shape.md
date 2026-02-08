# Move Settle: Overshoot & Dither — Shaping Notes

## Scope

Add two complementary settling mechanisms to the MOVE command:

1. **Directional overshoot** (anti-backlash): motor overshoots past target in the direction of travel, then returns to exact target. Ensures consistent approach direction to take up mechanical backlash in 3D-printed ball-joint mounts.

2. **Decaying-amplitude dither** (vibration settling): motor oscillates around the target with linearly decaying amplitude over N cycles, shaking ball joints into a repeatable resting position.

Both mechanisms are independently configurable via `SET` defaults and per-command overrides. Default `DITHER_AMPLITUDE=0` means dither is disabled out of the box (backward compatible). Default `MOVE_OVERSHOOT=300` provides overshoot by default.

## Decisions

- **Non-blocking state machine in `tick()`**: Multi-phase move is driven by a `MovePhase` enum in `MotorState`. When `tick()` detects motor idle, it transitions to the next phase and issues the next `startMoveAbs()`. No busy-wait loops.

- **Settle fields in `MotorState`**: Per-motor settle state (phase, center, amplitudes, remaining cycles) stored in the existing `MotorState` struct. This is the natural home — `tick()` already iterates over motors and has private access to `motors_[]`.

- **Extend `moveAbsMask()` virtual interface**: Add settle params with default values of 0 for backward compatibility. Both `HardwareMotorController` and `StubMotorController` override. This is cleaner than having command handlers reach into private motor state.

- **Thermal pre-check sums all phase legs**: Before any motion begins, estimate total time for primary move + overshoot legs + all dither legs + final return. Reject with E10/E11 if total exceeds budget. Uses existing `MotionKinematics::estimateMoveTimeMs()` for each leg.

- **Stub controller simulates multi-phase as single timed block**: No phase state machine in `StubMotorController`. Total duration = sum of all phase leg estimates. Single `plans_[i].end_ms` covers the entire sequence. Simplifies testing while preserving timing accuracy.

- **Dither positions not clamped to soft limits**: Same rationale as HOME — mechanics define the real boundary, and dither amplitudes are small relative to travel range.

- **Overshoot reverses direction at boundaries**: If overshoot target exceeds `[MIN_POS_STEPS, MAX_POS_STEPS]` (in hw-space), reverse the overshoot direction. If reversed also OOB, clamp; skip overshoot if clamped equals target.

- **All values in full-step units**: Overshoot and dither amplitude specified in user-space full steps, scaled by `microstep_multiplier` at command entry, consistent with target/speed/accel handling.

- **Session-only settings**: Not persisted to NVS, matching SPEED/ACCEL/DECEL pattern. Require motors idle to change.

- **Shared-step mode rejects settle params**: Same restriction as per-command speed/accel overrides. Returns E03 BAD_PARAM.

## Context

- **Visuals**: None needed — this is a firmware/protocol feature
- **References**: See `references.md`
- **Product alignment**: N/A (no product folder)
- **Standards applied**: See `standards.md`
