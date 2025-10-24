## As‑Built Summary: Integration + Protocol Simplification (Constant Speed)

Scope: Task Group 3 is complete. Firmware integrates a shared‑STEP engine with a simplified constant‑speed protocol and aligned estimates. This document captures the current state concisely (no historical notes).

What’s implemented
- Protocol simplification
  - MOVE/HOME accept only `<id|ALL>,<abs_steps>`; legacy per‑command speed/accel params are rejected with `E03 BAD_PARAM`.
  - Global controls: `GET/SET SPEED` and `GET/SET ACCEL` exist; ACCEL is accepted but ignored for motion in shared‑STEP (reserved for future ramps). `SET` is rejected while any motor is moving.
- Shared‑STEP backend
  - Single STEP pulse train driven by RMT with ISR‑driven TX requeue (no loop mode). Edge hook provides per‑period timing anchor.
  - Per‑motor SLEEP/DIR via 74HC595. `SharedStepAdapterEsp32` is the sole owner of DIR/SLEEP on Arduino builds.
  - Guarded DIR changes using `SharedStepGuards`: SLEEP low → DIR flip → SLEEP high within a safe gap.
  - Position integration via time while SLEEP=HIGH; auto‑sleep when moves complete unless WAKE override is set.
  - Start generator on first active motor; stop when last finishes.
- Estimates alignment
  - For shared‑STEP builds, firmware uses a constant‑speed estimator (ceil steps/speed in ms) for MOVE and multi‑leg HOME; FAS builds continue to use trapezoid/triangular kinematics.
  - On‑device tests compare actual vs. firmware’s estimate with symmetric tolerance.
- Cleanup
  - Removed temporary heartbeat and ADAPTER_* debug commands/APIs.
  - Consolidated on‑device tests under a single Arduino test runner.

Developer surface
- PlatformIO envs
  - `esp32DedicatedStep` (baseline, currently FastAccelStepper‑based)
  - `esp32SharedStep` (shared‑STEP; `-DUSE_SHARED_STEP=1`, `-DSHARED_STEP_GPIO=<pin>`)
- Key sources
  - RMT generator: `src/drivers/Esp32/SharedStepRmt.cpp`, header in `include/drivers/Esp32/SharedStepRmt.h`
  - Adapter: `src/drivers/Esp32/SharedStepAdapterEsp32.cpp`, header in `include/drivers/Esp32/SharedStepAdapterEsp32.h`
  - Protocol: `lib/MotorControl/src/MotorCommandProcessor.cpp`
  - Controller: `lib/MotorControl/src/HardwareMotorController.cpp`

Validation snapshot
- Native tests pass. ESP32 builds link for both envs. Rudimentary on‑device smoke confirms stable motion, correct busy rules, and aligned estimates.

Next
- Bench repetition to validate stability across cycles; then proceed to global ACCEL and ramp scheduling (Task Group 6).
