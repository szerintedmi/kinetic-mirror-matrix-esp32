## Summary (Part 1: Parser + Globals)
- Simplified protocol to constant-speed form and introduced global SPEED/ACCEL controls used by both MOVE and HOME.
- Updated HELP and added GET/SET handlers for SPEED and ACCEL, with busy-reject guards.
- Adjusted all affected tests to the new grammar; added focused tests for GET/SET and HOME using globals.

## Files Changed
- lib/MotorControl/src/MotorCommandProcessor.cpp — HELP output; MOVE/HOME parsing tightened to reject legacy per-command speed/accel; GET/SET SPEED/ACCEL; HOME defaults now use globals.
- lib/MotorControl/include/MotorControl/MotorCommandProcessor.h — New `default_speed_sps_` and `default_accel_sps2_` fields.
- Tests updated:
  - test/test_MotorControl/test_ProtocolBasicsAndMotion.cpp — Grammar + behavior updates.
  - test/test_MotorControl/test_Thermal.cpp — Uses SET SPEED/ACCEL for scenarios.
  - test/test_MotorControl/test_StatusAndBudget.cpp — Grammar update.
  - test/test_MotorControl/test_FeatureFlows.cpp — Grammar update.
  - test/test_MotorControl/test_KinematicsAndStub.cpp — Grammar update; globals set before MOVE.
  - test/test_MotorControl/test_ProtocolSpeedGlobals.cpp — New tests for GET/SET and HOME using globals.
  - test/test_MotorControl/test_main.cpp — Registered new tests.

## Testing
- Host tests (native): ✅ 65/65 passing.
- On-device: Not impacted by parser-only changes; will validate after integration with the shared STEP generator.

## Next (Part 2: Hardware Integration)
- Wire `SharedStepRmtGenerator` into `HardwareMotorController` when `USE_SHARED_STEP=1` (Arduino builds):
  - Start generator when first motor begins moving; stop when last finishes.
  - Count positions per motor only while its SLEEP=1; coordinate DIR flips via guard windows.
  - Enforce overlap at single global speed (already enforced via parser/global defaults).

## Part 2: Hardware Integration — Current Status

### What was implemented (code structure)
- Added a dedicated shared‑STEP backend behind the common `IFasAdapter` interface:
  - Factory selector on ESP32: `createEsp32MotionAdapter()` chooses FastAccelStepper (FAS) or Shared‑STEP by build flag.
  - New adapter: `SharedStepAdapterEsp32` (single STEP pulse train + 74HC595 SLEEP/DIR gating; constant speed for now).
  - New PlatformIO envs:
    - `esp32Fas` (FAS, per‑motor STEP)
    - `esp32SharedStep` (shared STEP backend; `-DUSE_SHARED_STEP=1`, `-DSHARED_STEP_GPIO=<pin>`)

### On‑device findings (shared env)
- Initial tests (before refactor):
  - `M:0,1200` moved motor 0; other motors sometimes updated pos but did not move.
  - `M:ALL,1200` moved only motor 0 fully; others stopped when 0 finished.
- After attempting to consolidate logic in the adapter, user observed regressions:
  - `M:0,1200`/`H:0` woke motor 0 but did not move; pos did not update; stayed awake.
  - Commands to other motors had no effect; `M:ALL` woke all but did not move.

### Probable causes to verify
- SLEEP gating not actually asserted HIGH on move start for each motor → driver never steps.
- DIR/SLEEP latch timing: 74HC595 update may not be latched (RCLK) at the right time.
- OE (Output Enable) pin state: must be LOW to drive outputs; confirm `SHIFT595_OE` wiring and initial state.
- Shared STEP pin mismatch: `SHARED_STEP_GPIO` might not match wiring; confirm actual pin and PlatformIO flag.
- LEDC‑based generator lacks edge callbacks; position integration relies on time, but if SLEEP never enables, motors won’t move and pos will stall.
- Residual FAS behavior interfering (now guarded, but worth confirming the FAS adapter is excluded in shared env).

### Triage and validation plan
1. Electrical sanity:
   - Confirm `SHIFT595_OE` is LOW after init; check `Shift595Esp32::begin()` path.
   - Probe `SHARED_STEP_GPIO` (LED + 330R): blinks/appears solid during a move.
   - Verify 74HC595 SLEEP bit goes HIGH for the addressed motor on move start (quick LED or meter on SLEEP line).
2. Command sanity (shared env):
   - Run `M:0,1200` and poll `STATUS` every ~250 ms; expect `id=0 moving=1 awake=1` and pos increasing.
   - Run `M:ALL,1200`; expect all SLEEP bits HIGH; pos values increasing independently; generator stops after last finishes.
3. Code probes (temporary, remove after):
   - Add debug prints around adapter `startMoveAbs()` to log DIR/SLEEP bitfields per motor and generator start/stop.
4. If SLEEP/DIR are correct but motors still don’t move:
   - Validate STEP pin fan‑out wiring and common ground.
   - Consider switching generator to RMT with deterministic period and optional edge ISR for per‑pulse accounting.

### Next actions (short list)
- In `SharedStepAdapterEsp32`:
  - Ensure SLEEP is forced HIGH at move start (already implemented) and cleared per‑motor on completion.
  - Latch once per batch change (DIR+SLEEP) and avoid redundant latches in the tick.
  - Maintain generator running while any slot is moving; stop only when all stop.
- Add a minimal on‑device smoke loop (optional) to script `M:ALL` and print `STATUS` for quick bench validation.

### Risks / open items
- LEDC PWM is a stand‑in; lacks precise edge hooks and may complicate DIR guard alignment. RMT migration is recommended for stable gating timing and (later) acceleration.
- Mapping of 74HC595 (which chip drives DIR vs SLEEP) must match PCB wiring; mismatched mapping will manifest as correct pos updates but no physical motion.
