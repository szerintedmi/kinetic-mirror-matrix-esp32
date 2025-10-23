## Implementation Report — Acceleration (Global ACCEL + Trapezoid)

Summary
- Shared‑STEP now supports global acceleration with a single time‑base ramp that all active motors share. We added a separate DECEL control, homing group barriers, a global MOVE‑while‑moving guard (with multicommand carve‑out), direction‑match optimization, and unified, accurate estimates (asymmetric model + speed‑aware overhead). Device timing tolerances were tightened accordingly.

Outcomes
- Global ACCEL wired: `GET ACCEL` / `SET ACCEL=<a>` already existed and now actively drive shared‑STEP ramps. `SET ACCEL` rejected while moving.
- Added DECEL: `GET DECEL` / `SET DECEL=<d>`; shared‑STEP uses DECEL for ramp‑down (DECEL=0 disables finish‑induced slowdowns), FastAccelStepper path stores but ignores DECEL (symmetric ramps), documented.
- Trapezoidal ramp in shared‑STEP adapter: generator period updates follow a global trajectory based on current speed, `v_max` (global SPEED), and `accel`. Deceleration triggers using `d_stop = ceil(v^2/(2a))` against the minimum remaining distance across active motors.
- Estimates unified: firmware uses `MotionKinematics` on all builds; for shared‑STEP, asymmetric variants with speed‑aware overhead are used for both CTRL:OK and LAST_OP_TIMING to keep them consistent.
- Tests added: ramp helper coverage (stopping distance, edge cases).

Key changes
- Adapter ramping — src/drivers/Esp32/SharedStepAdapterEsp32.cpp, include/drivers/Esp32/SharedStepAdapterEsp32.h
  - Added global ramp state (`v_cur`, `v_max`, `a`, `d`, accumulators). New `updateRamp_()` adjusts RMT generator speed based on accel/coast/decel decisions; re‑anchors guard timing when speed changes. When DECEL=0, ramp‑down is disabled (finish‑induced slowdowns removed; stops via SLEEP gating only).
  - Position integration now uses the current global generator speed rather than per‑slot speed.
  - Start behavior:
    - If generator idle: perform immediate safe DIR/SLEEP flip (no edge scheduling needed), then start generator at a safe minimum speed and ramp up.
    - If generator running: schedule guarded flip in the next safe gap using current `period_us_` and `phase_anchor_us_`.
  - Volatile bitmasks for ISR visibility (`dir_bits_`, `sleep_bits_`).
- Timing helpers — include/MotorControl/SharedStepTiming.h
  - Added `stop_distance_steps(v, a)` used by ramp logic and tests.
- Estimates — lib/MotorControl/src/MotorCommandProcessor.cpp, lib/MotorControl/src/HardwareMotorController.cpp
  - Replaced shared‑STEP constant‑speed branches with asymmetric `MotionKinematics` for MOVE/HOME estimates; centralized speed‑aware overhead via shared‑STEP wrappers.
- Protocol — lib/MotorControl/src/MotorCommandProcessor.*
  - Added `GET/SET DECEL` with busy‑while‑moving guards; HELP updated. Added multi‑command batch context so multiple MOVEs can start together while enforcing a global busy rule for shared‑STEP in single‑command flow.
- Controller — lib/MotorControl/src/HardwareMotorController.*
  - Exposed `setDeceleration()` to forward DECEL to the adapter. Homing legs now use a group barrier: next leg starts only after all motors in the current leg finish; legs start together for synchronized ramp‑up.
- Tests — test/test_MotorControl/test_SharedStepRamp.cpp
  - `stop_distance_steps` correctness at typical values and edge cases.

Notes on guard alignment
- On each speed change, the adapter updates `period_us_` and resets `phase_anchor_us_` to `micros()` so future guard windows align to the current period. Start‑of‑move immediate flips avoid edge hazards when the generator is idle; guarded windows are used only when the generator is already running.

Expected behavior with multiple concurrent moves
- Because a single STEP time‑base is shared, the global ramp responds to whichever motor needs to accelerate or decelerate next (based on minimum remaining distance and `ACCEL`). This can momentarily slow or speed other motors that are still moving.
- This coupling is an intentional trade‑off in the shared‑STEP spike and is acceptable for this phase. We do not implement “join‑at‑safe‑speed” scheduling at this time to avoid added complexity; we may revisit if it becomes a problem.
- Optimization: when a joining motor’s desired direction matches the current latched DIR, we skip SLEEP‑low and the guard window to avoid a tiny pause; the motor enables immediately.
- Global busy rule (shared‑STEP): New MOVE commands are rejected while any motor is moving, except when initiated together in a multi‑command batch. This avoids mid‑run joins; HOME progression respects group barriers so legs start together.

Edge cases and trade‑offs
- Parallel start (multicommand): Multiple MOVEs/HOMEs in the same input line begin together. A single global ramp is shared: if DECEL>0, a shorter move approaching its stop causes the train to decelerate for everyone; with DECEL=0, the shorter motor is SLEEP‑gated off and others keep cruising.
- Staggered start (single command after motion started): Shared‑STEP rejects new MOVEs while any motor is moving (batch carve‑out only). This avoids mid‑run joins and the complexity of “join‑at‑safe‑speed”.
- Finishes at different times: With DECEL>0, early finishes slow others near their stop (higher coupling, smoother landings). With DECEL=0, early finishes do not slow others; stops depend on SLEEP gating (faster overall, but more abrupt for the finisher).
- Mixed HOME + MOVE: If started together in one batch, DECEL>0 will produce decel/accel cycles as the shorter path finishes first; DECEL=0 minimizes this coupling. Homing legs themselves are synchronized by group barriers within the homing mask.

Estimates and tolerances
- Shared‑STEP uses asymmetric estimators:
  - MOVE: `estimateMoveTimeMsSharedStep(d, v, accel, decel)`
  - HOME: `estimateHomeTimeMsWithFullRangeSharedStep(overshoot, backoff, full_range, v, accel, decel)`
- Overheads are speed‑aware (smaller at higher v) and applied uniformly for CTRL:OK and LAST_OP_TIMING.
- On‑device timing tests tightened to ±max(1%, 60 ms) for MOVE and ±max(1%, 80 ms) for HOME.
 - Low‑DECEL note: When `DECEL` is very small, triangular moves can be slightly over‑estimated because the analytical model assumes braking to zero at the target. Runtime SLEEP‑gating stops exactly at target without spending the full brake‑to‑zero tail, so actual can be tens to a few hundred ms lower at extreme low `DECEL`. This is acceptable for now; if needed we can switch triangular estimates to a fast discrete simulation that mirrors adapter rules to remove this bias.

Build/Run
- Host tests: `pio test -e native`
- ESP32 (shared‑STEP): `pio run -e esp32SharedStep`

Manual smoke (expected)
- `SET SPEED=4000`, `SET ACCEL=16000`
- `MOVE:0,800` → `CTRL:OK est_ms=...`; compare with `GET LAST_OP_TIMING:0` after completion.
- While moving, `SET ACCEL=8000` → `CTRL:ERR E04 BUSY`.

Follow‑ups
- Optional: add a host‑side ramp progression simulation test to compare against `MotionKinematics` time within a tolerance for triangular and trapezoidal cases.
