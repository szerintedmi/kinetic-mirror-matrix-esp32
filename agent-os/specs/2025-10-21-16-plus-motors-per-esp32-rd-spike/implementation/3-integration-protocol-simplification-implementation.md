## Implementation Report — Integration + Protocol Simplification (Constant Speed)

This report captures the current as‑built state for Task Group 3.

Outcomes
- Protocol simplified to constant‑speed. `MOVE:<id|ALL>,<abs_steps>` and `HOME:<id|ALL>[,<overshoot>][,<backoff>][,<full_range>]` only. Legacy per‑command speed/accel params are rejected.
- Global controls: `GET/SET SPEED` and `GET/SET ACCEL` (ACCEL accepted but ignored in shared‑STEP until ramps land). `SET` rejected while any motor is moving.
- New shared‑STEP build: `env:esp32SharedStep` uses an RMT/ISR generator with a single shared STEP pin (no FAS dependency). Baseline `env:esp32Fas` remains.
- Shared‑STEP adapter owns DIR/SLEEP on Arduino; guard windows enforce SLEEP low → DIR flip → SLEEP high within safe gaps.
- Estimates aligned: shared‑STEP builds use constant‑speed estimates; FAS builds keep MotionKinematics. On‑device tests compare actual vs firmware estimate with symmetric tolerance.
- Temporary debug and per‑adapter diagnostics removed; on‑device tests consolidated under a single runner.

Builds and environments
- `esp32SharedStep` (shared‑STEP):
  - Flags: `-DUSE_SHARED_STEP=1 -DSHARED_STEP_GPIO=<pin>`
  - RMT with 1 µs ticks, ISR‑driven TX requeue, edge hook per period.
  - No FastAccelStepper dependency.
- `esp32Fas` (baseline FAS):
  - Per‑motor STEP via FastAccelStepper; unchanged.

Key modules and changes
- Protocol and estimates — lib/MotorControl/src/MotorCommandProcessor.cpp
  - Simplified MOVE/HOME grammar; help text updated; GET/SET SPEED/ACCEL.
  - Estimates: when `USE_SHARED_STEP`, compute constant‑speed durations (ceil steps/speed in ms) for MOVE and multi‑leg HOME; otherwise use MotionKinematics.
- Controller — lib/MotorControl/src/HardwareMotorController.cpp
  - For Arduino+`USE_SHARED_STEP`, record constant‑speed `last_op_est_ms` and set moving/awake intent; adapter handles gating.
  - For native/FAS paths, behavior unchanged.
- RMT generator — src/drivers/Esp32/SharedStepRmt.cpp, include/drivers/Esp32/SharedStepRmt.h
  - ISR‑driven requeue replaces loop mode; idempotent start/stop; optional edge hook invoked every period.
- Shared‑STEP adapter — src/drivers/Esp32/SharedStepAdapterEsp32.cpp, include/drivers/Esp32/SharedStepAdapterEsp32.h
  - Sole owner of DIR/SLEEP; computes guard windows, batches latches, and time‑integrates positions while SLEEP=HIGH.
  - Starts generator on first active motor, stops on last completion; auto‑sleep unless WAKE override is set.
- Cleanup
  - Removed heartbeat and adapter debug commands/APIs; removed trace buffers and GET ADAPTER_* handlers.
  - On‑device tests: single test runner at `test/test_OnDevice/test_main.cpp`.

Validation
- Native tests: pass.
- ESP32 builds: `esp32SharedStep` and `esp32Fas` compile and link.
- On‑device timing tests now assert actual within ±max(10%, 150 ms) of firmware’s own estimate.
- Manual smoke: GET/SET SPEED; MOVE/HOME cycles; busy rejections during motion.

How to build/run
- Shared‑STEP: `pio run -e esp32SharedStep`
- FAS baseline: `pio run -e esp32Fas`
- Host tests: `pio test -e native`
- On‑device tests (compile only): `pio test -e esp32Fas -f test_OnDevice --without-uploading` (adjust env as needed)

Next steps
- Bench repetition across >10 MOVE/HOME cycles at varied speeds; verify stability and auto‑sleep.
- Implement global ACCEL with trapezoidal period updates, keeping guard scheduling aligned; expand tests to cover ramp sequencing.
