# Move Settle: Overshoot & Dither — Implementation Plan

## Context

3D-printed ball-joint kinematic mirror mounts have positional repeatability issues due to mechanical backlash and friction. Two complementary settling mechanisms — directional overshoot (anti-backlash) and decaying-amplitude dither (vibration settling) — will be added to the MOVE command. Full spec: `spec.md` in this folder.

---

## Task 1: Constants, MovePhase Enum, MotorState Fields

### `lib/MotorControl/include/MotorControl/MotorControlConstants.h`
Add default constants:
```cpp
constexpr int DEFAULT_MOVE_OVERSHOOT = 300;
constexpr int DEFAULT_DITHER_AMPLITUDE = 0;   // disabled by default
constexpr int DEFAULT_DITHER_CYCLES = 3;
constexpr int DEFAULT_DITHER_MIN_AMPLITUDE = 20;
```

### `lib/MotorControl/include/MotorControl/MotorController.h`
Add `MovePhase` enum before `MotorState`:
```cpp
enum class MovePhase : uint8_t {
  NONE, PRIMARY, OVERSHOOT, APPROACH, DITHER_POS, DITHER_NEG, DITHER_RETURN
};
```

Add fields to `MotorState` (at end, with default member initializers):
```cpp
MovePhase move_phase = MovePhase::NONE;
long settle_center = 0;
int settle_overshoot = 0;
int settle_dither_amplitude = 0;
int settle_dither_cycles = 0;
int settle_dither_min_amplitude = 0;
int settle_remaining_cycles = 0;
int settle_current_amplitude = 0;
int settle_speed = 0;
int settle_accel = 0;
```

### Extend `moveAbsMask()` virtual interface
```cpp
virtual bool moveAbsMask(uint32_t mask, long target, int speed, int accel,
                         uint32_t now_ms,
                         int overshoot = 0, int dither_amplitude = 0,
                         int dither_cycles = 0, int dither_min_amplitude = 0) = 0;
```
Default params ensure backward compatibility. Both `HardwareMotorController` and `StubMotorController` update their overrides.

---

## Task 2: Extend CommandExecutionContext & MotorCommandProcessor

### `CommandExecutionContext.h`
Add reference members and accessors:
- `int& default_move_overshoot_` → `int& defaultMoveOvershoot()`
- `int& default_dither_amplitude_` → `int& defaultDitherAmplitude()`
- `int& default_dither_cycles_` → `int& defaultDitherCycles()`
- `int& default_dither_min_amplitude_` → `int& defaultDitherMinAmplitude()`

### `MotorCommandProcessor.h`
Add private members initialized from `MotorControlConstants` defaults. Add read-only config accessors for MQTT config publisher.

### `MotorCommandProcessor.cpp`
Initialize from constants in constructor. Pass to `makeContext()` by reference.

---

## Task 3: Extend MOVE Command Parsing

### `CommandHandlers.cpp` — `handleMove()`

1. **Remove** existing "reject non-empty parts > 4" guard (lines 450-456)
2. **Shared-step guard**: extend to also reject parts[4..6] with E03
3. **Parse positions 5-7** (non-shared-step):
   - `parts[4]` → overshoot (int >= 0, empty = default)
   - `parts[5]` → dither_amplitude (int >= 0, empty = default)
   - `parts[6]` → dither_cycles (int >= 0, empty = default)
   - Reject > 7 parts with non-empty trailing values
4. **Microstep scale**: apply `ms_mult` to overshoot, dither_amplitude, dither_min_amplitude
5. **Pass to moveAbsMask()**: include all settle params

---

## Task 4: Multi-Phase Thermal Budget Pre-Check

### `CommandHandlers.cpp` — `handleMove()`

Replace single-leg thermal estimate with total multi-phase estimate:
```
total = estimateMoveTimeMs(primary_dist)
if overshoot > 0 && delta != 0:
  total += estimateMoveTimeMs(overshoot)     // overshoot leg
  total += estimateMoveTimeMs(overshoot)     // approach leg
if dither_amplitude > 0 && dither_cycles > 0:
  for i in 1..cycles:
    amp_i = amplitude * (cycles - i + 1) / cycles
    if amp_i < min_amplitude: break
    total += estimateMoveTimeMs(amp_i)       // center to center+amp
    total += estimateMoveTimeMs(2 * amp_i)   // center+amp to center-amp
  total += estimateMoveTimeMs(last_amp)      // return to center
```

Reuse existing `MotionKinematics::estimateMoveTimeMs()` for each leg.

---

## Task 5: Multi-Phase State Machine

### `HardwareMotorController.cpp`

#### `moveAbsMask()` changes:
- Accept settle params, populate `motors_[i].settle_*` fields
- If `overshoot > 0` and `delta != 0`:
  - Compute `overshoot_target = target + sign(delta) * overshoot`
  - Boundary handling: if OOB, reverse direction; if still OOB, clamp; skip if clamped == target
  - Set `move_phase = OVERSHOOT`, start move to overshoot_target
- Else if dither active: set `move_phase = PRIMARY`, start move to target
- Else: `move_phase = NONE` (legacy)

#### `tick()` extensions:
When motor stops and `move_phase != NONE`:
```
OVERSHOOT  → startMoveAbs to settle_center, phase = APPROACH
PRIMARY    → (fallthrough to APPROACH logic)
APPROACH   → if dither: start first dither cycle, phase = DITHER_POS
             else: phase = NONE, moving = false, mark complete
DITHER_POS → startMoveAbs to center - amplitude, phase = DITHER_NEG
DITHER_NEG → decrement cycles, compute next amplitude
             if more cycles && amp >= min: start next +amp, phase = DITHER_POS
             else: startMoveAbs to center, phase = DITHER_RETURN
DITHER_RETURN → phase = NONE, moving = false, mark complete
```

- Pre-latch DIR bits before each sub-move
- Keep `moving = true` and `last_op_ongoing = true` throughout all phases
- CompletionTracker fires naturally when final phase completes

### `StubMotorController.cpp`
- Accept settle params, compute total duration as sum of all leg estimates
- Single `plans_[i].end_ms = now_ms + total_duration`

---

## Task 6: GET/SET for New Settings

### `CommandHandlers.cpp`

#### `handleGet()`:
- Add to GET ALL: `MOVE_OVERSHOOT`, `DITHER_AMPLITUDE`, `DITHER_CYCLES`, `DITHER_MIN_AMPLITUDE`
- Add individual key handlers (same pattern as SPEED)

#### `handleSet()`:
- `SET MOVE_OVERSHOOT=<val>` — int >= 0, check all motors idle
- `SET DITHER_AMPLITUDE=<val>` — int >= 0, check all motors idle
- `SET DITHER_CYCLES=<val>` — int >= 0, check all motors idle
- `SET DITHER_MIN_AMPLITUDE=<val>` — int >= 0, check all motors idle

---

## Task 7: Update HELP Text

### `HelpText.cpp`
Update MOVE line (FAS mode):
```
MOVE:<id|ALL>,<abs_steps>[,<speed>][,<accel>][,<overshoot>][,<dither_amp>][,<dither_cycles>]
```

---

## Task 8: Unit Tests

### `test/test_MotorControl/test_MoveSettle.cpp` (new file)

1. `test_move_overshoot_basic` — verify overshoot timing/position
2. `test_move_dither_basic` — verify dither oscillation
3. `test_move_overshoot_and_dither_combined` — full 4-phase sequence
4. `test_overshoot_boundary_reversal` — near MAX_POS
5. `test_overshoot_skip_when_no_delta` — target == current
6. `test_overshoot_disabled_when_zero`
7. `test_dither_disabled_when_zero_amplitude`
8. `test_dither_early_termination_min_amplitude`
9. `test_get_set_move_overshoot`
10. `test_get_set_dither_amplitude`
11. `test_get_set_dither_cycles`
12. `test_get_set_dither_min_amplitude`
13. `test_set_settle_busy_reject`
14. `test_per_command_override` — `MOVE:0,800,,,300,100,5`
15. `test_per_command_disable` — `MOVE:0,800,,,0,0`
16. `test_thermal_budget_multiphase`
17. `test_status_moving_throughout_settle`
18. `test_shared_step_rejects_settle_params`
19. `test_help_includes_settle_params`
20. `test_get_all_includes_settle_settings`

### `test/test_MotorControl/test_main.cpp`
Register all new test functions.

---

## Task 9: Python CLI & MQTT

### `tools/mirror_cli/cli.py`
- Add `--overshoot`, `--dither-amplitude`, `--dither-cycles` to `move` command
- Create `_join_move_with_placeholders()` (same pattern as HOME)

### `tools/mirror_cli/command_builder.py`
Parse `args[4..6]` as `overshoot`, `dither_amplitude`, `dither_cycles` in MOVE.

### MQTT Command Handler (firmware)
Extend `MqttCommandHandler.cpp` MOVE action to extract new JSON params.

---

## Task 10: Python Tests

### `tools/mirror_cli/tests/`
- Test CLI argument parsing for new move options
- Test `_join_move_with_placeholders()` output
- Test MQTT command builder for new params

---

## Task 11: Documentation Updates

### `README.md` — Protocol Reference (line ~142)
- Update MOVE line to include new optional params:
  ```
  MOVE:<id|ALL>,<abs_steps>[,<speed>][,<accel>][,<overshoot>][,<dither_amp>][,<dither_cycles>]
  ```
- Add new GET/SET resources to the protocol reference:
  ```
  GET MOVE_OVERSHOOT
  GET DITHER_AMPLITUDE
  GET DITHER_CYCLES
  GET DITHER_MIN_AMPLITUDE
  SET MOVE_OVERSHOOT=<int>
  SET DITHER_AMPLITUDE=<int>
  SET DITHER_CYCLES=<int>
  SET DITHER_MIN_AMPLITUDE=<int>
  ```

### `docs/mqtt-command-schema.md`
- **MOVE section** (line ~69): Add `overshoot`, `dither_amplitude`, `dither_cycles` to MOVE params table and request example
- **GET section** (line ~247): Add new resources to GET resource table (`MOVE_OVERSHOOT`, `DITHER_AMPLITUDE`, `DITHER_CYCLES`, `DITHER_MIN_AMPLITUDE`)
- **GET ALL completion** (line ~257): Add new fields to example response
- **SET section** (line ~291): Add new SET parameters to table with valid values (int >= 0)
- **Serial Command Mapping** (line ~698): Update MOVE row to show new optional params
- **Serial Protocol Quick Reference** (line ~679): Update MOVE example

### `docs/mqtt-payload-examples.md`
- **MOVE section** (line ~224): Add example with overshoot and dither params:
  ```json
  {
    "action": "MOVE",
    "params": {
      "target_ids": 0,
      "position_steps": 800,
      "overshoot": 300,
      "dither_amplitude": 100,
      "dither_cycles": 5
    }
  }
  ```
- **GET ALL section** (line ~421): Add new settings to example response
- **Broadcasted config** (line ~9): Add new config fields to example

### `docs/mqtt-config-schema.md`
- **Payload Structure** (line ~7): Add `move_overshoot`, `dither_amplitude`, `dither_cycles`, `dither_min_amplitude` to example JSON
- **Fields table** (line ~25): Add rows for new config fields
- **Publish Triggers** (line ~41): Note that SET of overshoot/dither settings triggers config republish

---

## Task 12: Build Validation

```bash
pio run -e esp32DedicatedStep    # ESP32 FAS build
pio run -e esp32SharedStep       # Shared-step build
pio run -e native                # Native/stub build
pio test -e native               # All tests (C++ + Python)
```

---

## Verification

1. **Build all targets**: `pio run -e esp32DedicatedStep && pio run -e esp32SharedStep && pio run -e native`
2. **Run all tests**: `pio test -e native`
3. **Manual serial test** (if hardware available):
   - `SET MOVE_OVERSHOOT=300` → `GET MOVE_OVERSHOOT` → verify 300
   - `MOVE:0,800` → observe overshoot to ~1100, return to 800, STATUS shows moving=1 throughout
   - `SET DITHER_AMPLITUDE=100` + `SET DITHER_CYCLES=3` → `MOVE:0,500` → observe dither
   - `MOVE:0,800,,,0,0` → no overshoot/dither despite defaults
   - Boundary: `MOVE:0,1100` with overshoot=300 → reversed overshoot direction

---

## Key Files

| File | Changes |
|------|---------|
| `lib/MotorControl/include/MotorControl/MotorControlConstants.h` | New default constants |
| `lib/MotorControl/include/MotorControl/MotorController.h` | MovePhase enum, MotorState fields, moveAbsMask() |
| `lib/MotorControl/include/MotorControl/command/CommandExecutionContext.h` | New reference members |
| `lib/MotorControl/src/command/CommandExecutionContext.cpp` | Constructor update |
| `lib/MotorControl/include/MotorControl/MotorCommandProcessor.h` | New members + accessors |
| `lib/MotorControl/src/MotorCommandProcessor.cpp` | Init defaults, makeContext() |
| `lib/MotorControl/src/command/CommandHandlers.cpp` | MOVE parsing, GET/SET, thermal |
| `lib/MotorControl/src/command/HelpText.cpp` | Updated HELP text |
| `lib/MotorControl/src/HardwareMotorController.cpp` | moveAbsMask() + tick() |
| `lib/MotorControl/src/StubMotorController.h/.cpp` | moveAbsMask() override |
| `test/test_MotorControl/test_MoveSettle.cpp` | New test file |
| `test/test_MotorControl/test_main.cpp` | Register tests |
| `tools/mirror_cli/cli.py` | CLI args |
| `tools/mirror_cli/command_builder.py` | MQTT command parsing |
| `README.md` | Protocol Reference section |
| `docs/mqtt-command-schema.md` | MOVE params, GET/SET resources |
| `docs/mqtt-payload-examples.md` | New MOVE examples, GET ALL response |
| `docs/mqtt-config-schema.md` | New config fields |
