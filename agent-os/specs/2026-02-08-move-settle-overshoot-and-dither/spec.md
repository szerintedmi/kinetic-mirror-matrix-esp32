# Specification: Move Settle — Overshoot & Dither

## Goal

Add two complementary settling mechanisms to the MOVE command — directional overshoot (anti-backlash) and decaying-amplitude dither (vibration settling) — to improve positional repeatability of 3D-printed ball-joint kinematic mirror mounts driven by small stepper linear actuators.

## User Stories

- As an operator, I can issue `MOVE` with an overshoot parameter so the motor always approaches the target from a consistent direction, taking up mechanical backlash.
- As an operator, I can enable dither so the motor oscillates around the target after arrival, shaking the ball joints into a repeatable resting position.
- As a developer, I can configure default overshoot and dither values via `SET` and override them per-command, following the same pattern as speed/accel.
- As a developer, I can disable overshoot (`0`) or dither (`amplitude=0`) independently to isolate which technique helps on a given build.

## Core Requirements

### Functional Requirements

#### 1. Move Overshoot (Anti-Backlash)

- After a MOVE, instead of stopping at the target, the motor first moves to an overshoot position, then approaches the target from a fixed direction controlled by the sign of the overshoot value.
- Sequence for a move from `current` to `target` with `overshoot != 0`:
  1. Move directly to `target - overshoot` (the overshoot position)
  2. Move to `target` (approach from the fixed direction)
- The sign of `overshoot` controls the approach direction: positive overshoot means the final approach is in the positive direction (overshoot position is below target). Negative overshoot means the final approach is in the negative direction.
- This is a 2-move sequence (directly to overshoot position, then to target), not 3-move. The motor does not stop at the target first.
- If `overshoot = 0`, skip the overshoot phase entirely (legacy behavior).
- Firmware config setting `MOVE_OVERSHOOT`, default `300`, overridable per-command. Negative values are allowed.

#### 2. Dither (Vibration Settling)

- After the motor reaches the target (post-overshoot if enabled), it oscillates back and forth with linearly decaying amplitude over N cycles.
- Sequence for dither with `amplitude > 0` and `cycles > 0`:
  - For cycle `i` from 1 to `cycles`:
    - Compute `amp_i = amplitude * (cycles - i + 1) / cycles` (linear decay).
    - If `amp_i < dither_min_amplitude`, stop early (remaining oscillation too small to move mechanics).
    - Move to `target + amp_i`.
    - Move to `target - amp_i`.
  - Final move to `target` (ensure exact final position).
- If `amplitude = 0`, skip dither entirely.
- Dither positions are not clamped to soft limits (same rationale as HOME — the mechanics define the real boundary, and dither amplitudes are small relative to the range).
- Firmware config settings:
  - `DITHER_AMPLITUDE`: initial oscillation size in full steps, default `0` (disabled).
  - `DITHER_CYCLES`: number of full oscillation cycles, default `3`.
  - `DITHER_MIN_AMPLITUDE`: minimum amplitude threshold below which cycles are skipped, default `20`. Config-only (not overridable per-command).
- `DITHER_AMPLITUDE` and `DITHER_CYCLES` are overridable per-command. `DITHER_MIN_AMPLITUDE` is config-only.

#### 3. Scope: MOVE Command Only

- Overshoot and dither apply exclusively to the `MOVE` command.
- `HOME` already has its own overshoot/backoff mechanism and is not affected.
- `WAKE`, `SLEEP`, `STATUS`, `HELP` are unaffected.

#### 4. Combined Execution Order

For a MOVE with both overshoot and dither enabled, the full sequence is:

```
1. OVERSHOOT:  Move directly to target - overshoot (2-move: no stop at target first)
2. APPROACH:   Move to target
3. DITHER:     For each cycle i:
                 Move to target + amp_i
                 Move to target - amp_i
4. RETURN:     Move to target (final)
```

The entire sequence is a single logical operation: the motor reports `moving=true` throughout, rejects new MOVE/HOME with `E04 BUSY`, and the completion tracker fires only after the final phase.

#### 5. Protocol Extension

```
MOVE:<id>,<target>[,speed][,accel][,overshoot][,dither_amplitude][,dither_cycles]
```

- Positions 5-7 are new optional parameters.
- Empty string = use firmware default (same pattern as speed/accel).
- Examples:
  - `MOVE:0,800` — all defaults
  - `MOVE:0,800,,,,0` — default speed/accel/overshoot, dither disabled
  - `MOVE:0,800,,,300,100,5` — default speed/accel, 300-step overshoot, 100-step dither amplitude, 5 cycles
  - `MOVE:0,800,,,0` — explicitly no overshoot, default dither

#### 6. GET/SET Extensions

New settings accessible via `GET` and `SET`:

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `MOVE_OVERSHOOT` | int | `300` | Overshoot distance in full steps (sign = approach direction, 0 = disabled) |
| `DITHER_AMPLITUDE` | int >= 0 | `0` | Initial dither amplitude in full steps (0 = disabled) |
| `DITHER_CYCLES` | int >= 0 | `3` | Number of dither oscillation cycles |
| `DITHER_MIN_AMPLITUDE` | int >= 0 | `20` | Minimum amplitude threshold; cycles with amplitude below this are skipped |

All settings are session-only (not persisted to NVS), require motors to be idle to change (consistent with SPEED/ACCEL), and apply to subsequent MOVE commands. This matches the existing pattern — only Wi-Fi and MQTT config use NVS persistence.

#### 7. Thermal Budget

- Before starting the multi-phase move, estimate total motion time for all phases (primary move + overshoot legs + all dither legs + final return) using existing `MotionKinematics::estimateMoveTimeMs()`.
- Reject with existing thermal budget error if total estimated time exceeds available budget, before any motion begins.

#### 8. Shared-Step Mode

- Overshoot and dither parameters are not supported in `USE_SHARED_STEP` builds (same restriction as per-command speed/accel overrides).
- If overshoot/dither parameters are provided in shared-step mode, reject with `E03 BAD_PARAM`.
- Firmware config defaults for overshoot/dither are ignored in shared-step mode.

#### 9. Busy Semantics During Settle

- The motor reports `moving=true` throughout all settle phases (overshoot, dither, return).
- `SLEEP` on a moving motor already returns `E04 BUSY` (existing behavior). No special cancellation logic is needed for this iteration.
- A new `MOVE` or `HOME` to a motor mid-settle also receives `E04 BUSY`.
- Force-stop / cancellation during settle phases is deferred to a future iteration if needed.

### Non-Functional Requirements

- Settling time is not a concern for this iteration; optimize for repeatability.
- The multi-phase state machine must be non-blocking — driven by `tick()`, not by busy-wait loops.
- All dither/overshoot values are in full-step units (scaled by `microstep_multiplier` before hardware calls, matching existing behavior). Microstep-level dither may be explored in a future iteration if full-step dither proves effective.
- Position limits (`MIN_POS_STEPS` / `MAX_POS_STEPS`) apply to the user-specified target. Overshoot reverses direction if it would exceed limits. Dither positions may temporarily exceed soft limits (small amplitude relative to range).

## Reusable Components

### Existing Code to Leverage

- `handleMove()` in `CommandHandlers.cpp` — extend parameter parsing (positions 5-7), follows established empty-param pattern.
- `handleGet()` / `handleSet()` in `CommandHandlers.cpp` — add new keys following SPEED/ACCEL pattern.
- `CommandExecutionContext` — add references for new default settings.
- `MotorCommandProcessor` — add member variables for new defaults, pass to context.
- `MotorControlConstants` — add new default constants.
- `HardwareMotorController::tick()` — already polls motor completion; extend to drive phase transitions.
- `MotionKinematics::estimateMoveTimeMs()` — reuse for thermal budget estimation of each phase leg.
- `CompletionTracker` — already tracks operation completion; needs awareness that the operation spans multiple phases.

### New Components Required

- Multi-phase move state in `MotorState` (phase enum, remaining cycles, amplitude, center position). Rationale: `tick()` needs per-motor state to know which phase to transition to next.
- Phase state machine logic in `HardwareMotorController::tick()`. Rationale: non-blocking execution requires state-driven transitions rather than a blocking loop.
- Total motion time estimator for multi-phase moves. Rationale: thermal budget pre-check needs to account for all phases before starting.

## Technical Approach

- **State machine**: Add a `MovePhase` enum to `MotorState`: `{NONE, PRIMARY, OVERSHOOT, APPROACH, DITHER_POS, DITHER_NEG, DITHER_RETURN}`. Add fields: `settle_center` (target position), `settle_overshoot`, `settle_dither_amplitude`, `settle_dither_cycles`, `settle_dither_min_amplitude`, `settle_remaining_cycles`, `settle_current_amplitude`.
- **moveAbsMask() changes**: When overshoot or dither are active, set the initial phase to `OVERSHOOT` (if overshoot != 0) or `PRIMARY` (dither only) and populate the settle fields. When overshoot is active, the first physical move goes directly to `target - overshoot` (not to the target). No `settle_dir` field is needed — the sign of `settle_overshoot` encodes the approach direction.
- **tick() extensions**: When a motor in a settle phase stops moving (FAS reports idle), transition to the next phase and issue the next `startMoveAbs()`. When the final phase completes, clear the phase to `NONE` and mark `moving = false`.
- **Completion tracker**: `moving` stays `true` until the final phase completes, so the tracker naturally fires at the right time.
- **Command parsing**: Extend `handleMove()` to parse positions 5-7 from the comma-split args, with empty = default from context.
- **Overshoot boundary**: Compute overshoot target in the direction of travel. If out of bounds, reverse the overshoot direction. If reversed target is also out of bounds, clamp; skip if clamped position equals the target.
- **Microstep scaling**: Apply `microstep_multiplier` to overshoot and dither amplitude values (same as target, speed, accel).
- **HELP text**: Update MOVE help line to show new optional parameters.
- **Python CLI/TUI**: Add `--overshoot`, `--dither-amplitude`, `--dither-cycles` arguments to the `move` command. Update TUI motor control panels for both serial and MQTT transports.

## Out of Scope

- Persisting overshoot/dither settings across reboots (NVS).
- Per-motor overshoot/dither configuration (all motors share the same defaults).
- Dither frequency as an independent parameter (governed by motor speed/accel).
- Asymmetric dither patterns or non-linear decay curves.
- Force-stop / cancellation of settle phases mid-sequence (SLEEP returns BUSY during motion, consistent with existing behavior).
- Microstep-level dither granularity (revisit if full-step dither proves effective).

## Success Criteria

### Firmware
- `SET MOVE_OVERSHOOT=300` followed by `MOVE:0,800` executes: move to 800-300=500 (overshoot position), then approach 800 from below (positive direction).
- `SET MOVE_OVERSHOOT=-300` followed by `MOVE:0,800` executes: move to 800-(-300)=1100, then approach 800 from above (negative direction).
- Overshoot within bounds: `MOVE:0,800` with `MOVE_OVERSHOOT=300` moves to 500, then returns to 800.
- `SET DITHER_AMPLITUDE=100` and `SET DITHER_CYCLES=3` followed by `MOVE:0,800` executes: move to 800, then oscillate +100/-100, +67/-67, +33/-33 around 800, then return to 800.
- Combined overshoot + dither executes the full 4-phase sequence in order.
- `MOVE:0,800,,,0,0` disables both overshoot and dither regardless of firmware defaults.
- `STATUS` shows `moving=1` throughout the entire multi-phase sequence.
- `SLEEP:0` mid-settle returns `E04 BUSY` (consistent with existing moving-motor behavior).
- Thermal budget pre-check accounts for total estimated time of all phases.
- `GET MOVE_OVERSHOOT`, `GET DITHER_AMPLITUDE`, `GET DITHER_CYCLES`, `GET DITHER_MIN_AMPLITUDE` return current values.
- Shared-step builds reject overshoot/dither parameters with `E03 BAD_PARAM`.
- `HELP` output documents the new MOVE parameters.

### Backward Compatibility
- All existing MOVE tests continue to pass (backward compatible when overshoot=0 and dither_amplitude=0).
- Default `DITHER_AMPLITUDE=0` means existing behavior is unchanged out of the box.

### Tests
- New unit tests cover: multi-phase sequencing, overshoot boundary reversal, early dither termination at min amplitude, thermal budget estimation for multi-phase moves, GET/SET for new settings.

### Documentation & Tooling
- Serial `HELP` text updated with new MOVE parameter grammar.
- Python CLI `move` subcommand accepts `--overshoot`, `--dither-amplitude`, `--dither-cycles` optional arguments.
- TUI motor control updated to expose overshoot/dither for both serial and MQTT transports.
- `GET`/`SET` commands for new settings work via both serial and MQTT.
- `README.md` Protocol Reference updated with new MOVE grammar and GET/SET resources.
- `docs/mqtt-command-schema.md` updated: MOVE params, GET resources, SET params, serial mapping.
- `docs/mqtt-payload-examples.md` updated: MOVE with settle params example, GET ALL response.
- `docs/mqtt-config-schema.md` updated: new config fields for settle settings.
