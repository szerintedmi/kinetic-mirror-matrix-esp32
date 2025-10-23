## Implementation Report — Acceleration (Global ACCEL + Trapezoid)

Outcomes
- Global ACCEL wired: `GET ACCEL` / `SET ACCEL=<a>` already existed and now actively drive shared‑STEP ramps. `SET ACCEL` rejected while moving.
- Trapezoidal ramp in shared‑STEP adapter: generator period updates follow a global trajectory based on current speed, `v_max` (global SPEED), and `accel`. Deceleration triggers using `d_stop = ceil(v^2/(2a))` against the minimum remaining distance across active motors.
- Estimates unified: firmware uses `MotionKinematics` for MOVE and HOME on all builds (including `USE_SHARED_STEP`).
- Tests added: ramp helper coverage (stopping distance, edge cases).

Key changes
- Adapter ramping — src/drivers/Esp32/SharedStepAdapterEsp32.cpp, include/drivers/Esp32/SharedStepAdapterEsp32.h
  - Added global ramp state (`v_cur`, `v_max`, `a`, accumulators). New `updateRamp_()` adjusts RMT generator speed based on accel/coast/decel decisions; re‑anchors guard timing when speed changes.
  - Position integration now uses the current global generator speed rather than per‑slot speed.
  - Start behavior:
    - If generator idle: perform immediate safe DIR/SLEEP flip (no edge scheduling needed), then start generator at a safe minimum speed and ramp up.
    - If generator running: schedule guarded flip in the next safe gap using current `period_us_` and `phase_anchor_us_`.
  - Volatile bitmasks for ISR visibility (`dir_bits_`, `sleep_bits_`).
- Timing helpers — include/MotorControl/SharedStepTiming.h
  - Added `stop_distance_steps(v, a)` used by ramp logic and tests.
- Estimates — lib/MotorControl/src/MotorCommandProcessor.cpp, lib/MotorControl/src/HardwareMotorController.cpp
  - Replaced shared‑STEP constant‑speed branches with `MotionKinematics` for MOVE/HOME estimates.
- Tests — test/test_MotorControl/test_SharedStepRamp.cpp
  - `stop_distance_steps` correctness at typical values and edge cases.

Notes on guard alignment
- On each speed change, the adapter updates `period_us_` and resets `phase_anchor_us_` to `micros()` so future guard windows align to the current period. Start‑of‑move immediate flips avoid edge hazards when the generator is idle; guarded windows are used only when the generator is already running.

Build/Run
- Host tests: `pio test -e native`
- ESP32 (shared‑STEP): `pio run -e esp32SharedStep`

Manual smoke (expected)
- `SET SPEED=4000`, `SET ACCEL=16000`
- `MOVE:0,800` → `CTRL:OK est_ms=...`; compare with `GET LAST_OP_TIMING:0` after completion.
- While moving, `SET ACCEL=8000` → `CTRL:ERR E04 BUSY`.

Follow‑ups
- Optional: add a host‑side ramp progression simulation test to compare against `MotionKinematics` time within a tolerance for triangular and trapezoidal cases.
