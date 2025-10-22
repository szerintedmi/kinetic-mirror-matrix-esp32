## Shared STEP Housekeeping Tasks

Scope: Integration + Protocol Simplification (Constant Speed) and ESP32 shared-STEP adapter

This checklist captures current findings, decisions, and proposed refactors to keep the spike coherent without overengineering. Preference is for fewer, larger steps that land complete features and avoid double‑refactors.

### 0) Cross‑check: FAS build guards
- Observation: `src/drivers/Esp32/FasAdapterEsp32.cpp` starts with `#if defined(ARDUINO) && !defined(USE_SHARED_STEP)` before including `BuildConfig.h`. This works as intended because command‑line `-DUSE_SHARED_STEP=1` controls the guard. In FAS builds (no `-DUSE_SHARED_STEP`), the file compiles; in Shared builds it’s excluded. Inner blocks use `#if !(USE_SHARED_STEP)` which resolve correctly after including `BuildConfig.h` (macro defaults to 0 if not set).
- Status: OK functionally.
- Improvement (consistency, optional): Use value‑based checks everywhere. Pattern:
  - Include `MotorControl/BuildConfig.h` first, then `#if defined(ARDUINO) && !(USE_SHARED_STEP)` at the top. This avoids mixing `defined()` and value checks and prevents future fragility if headers get rearranged.

### 1) Single owner for DIR/SLEEP (Arduino + Shared STEP)
- Issue: Both `HardwareMotorController` and `SharedStepAdapterEsp32` manipulate 74HC595 `dir_bits_`/`sleep_bits_` and call `setDirSleep()`. This duplicates ownership and risks redundant latches or inconsistent bits.
- Proposed change: For `USE_SHARED_STEP=1` on Arduino builds, make `SharedStepAdapterEsp32` the sole owner of DIR/SLEEP. `HardwareMotorController` should not modify or latch bits in this configuration.
- Files to touch:
  - `lib/MotorControl/src/HardwareMotorController.cpp`: remove `#if (USE_SHARED_STEP)` branches that set `dir_bits_`, `sleep_bits_`, and `latch_()` under Arduino; rely on adapter methods only (`startMoveAbs`, `enableOutputs`, `disableOutputs`).
  - Keep native (host) path unchanged since it uses the stubbed latch for tests.

### 2) Integrate guard windows (RMT migration)
- Context: Guard helpers exist and are unit‑tested (`SharedStepTiming`, `SharedStepGuards`), but the adapter doesn’t schedule DIR flips and SLEEP gating within step gaps. With LEDC, edge‑aligned scheduling isn’t feasible; risk of glitch steps remains.
- Plan: Move the generator to ESP‑IDF RMT and wire an edge hook to schedule:
  - Compute `period_us = step_period_us(speed_sps)`.
  - Use `compute_flip_window(now_us, period_us, win)` to pick the mid‑gap window.
  - Schedule `SLEEP=LOW` → `DIR flip` → `SLEEP=HIGH` inside that window, with a single `setDirSleep()` latch per change.
- Deliverable: RMT‑based `SharedStepRmtGenerator` with an ISR calling a hook after each rising edge (or item boundary), and adapter logic that drives guarded flips.

### 3) LEDC generator caveats and transition
- Current: LEDC `writeTone()` + 50% duty in `SharedStepRmt.cpp` functions for a spike but lacks deterministic edge callbacks and fine duty control.
- Action: Treat LEDC as temporary. Prioritize RMT migration (Task 2) so we don’t refactor twice.

### 4) ‘Awake’ vs hardware SLEEP source of truth
- Issue: `MotorState.awake` (controller view) and hardware SLEEP (adapter latch) can diverge when both layers manipulate SLEEP.
- Plan: After Task 1, the adapter becomes the single source of truth for hardware SLEEP. `HardwareMotorController` should reflect adapter state into `MotorState.awake` (or derive from adapter operations) and avoid direct SLEEP toggling on Arduino.

### 5) Position integration semantics
- Current: Adapter updates position using time integration (`micros()` * sps), gated by `awake`. This approximates pulse counting.
- Note: Acceptable for the spike. With RMT edge hooks, we can switch to per‑pulse accounting later for exact positions. Document this as a known limitation until RMT lands.

### 6) PlatformIO environment names and filters
- Rename envs for clarity:
  - `[env:esp32dev]` → `[env:esp32Fas]`
  - `[env:esp32dev-shared]` → `[env:esp32SharedStep]`
- Fix native build filter case:
  - `-<src/Drivers/Esp32/**>` → `-<src/drivers/Esp32/**>`
- Update any doc references to the old env names.

### 7) Protocol scope note
- Group 3 added `GET/SET SPEED` and (optionally) `GET/SET ACCEL`. The adapter currently ignores accel (as intended for constant‑speed). Keep as‑is; accel will be used when global ramps are implemented.

### 8) Quick bench validation steps (post‑refactor)
- Shared STEP env (`esp32SharedStep`):
  - `GET SPEED` → shows default
  - `SET SPEED=4000` → `CTRL:OK`
  - `MOVE:0,200` then poll `STATUS` until `moving=0`
  - While moving, `SET SPEED=4500` → `CTRL:ERR E04 BUSY`
  - `MOVE:0,200,4000` → `CTRL:ERR E03 BAD_PARAM`
- Electrical checks: STEP pin activity, 74HC595 OE low after first latch, SLEEP high during motion for addressed motors.

### 9) Optional consistency cleanup
- Normalize macro checks to value‑based across the codebase (`#if (USE_SHARED_STEP)`), ensuring `BuildConfig.h` is included where needed. Not urgent but reduces fragility.

---

Owner: api‑engineer
Status: Draft — to be used as the reference list for the next integration iteration.

