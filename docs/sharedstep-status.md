# Shared‑STEP (esp32SharedStep) — Current Status: Parked

Purpose: capture context on the custom shared‑STEP generator so we can resume with a clear plan. This supplements the spike docs in `agent-os/specs/2025-10-21-16-plus-motors-per-esp32-rd-spike/implementation`

## Summary

- Backend: Shared‑STEP generator on ESP32 using a single hardware‑timed STEP train (RMT) with per‑motor DIR/SLEEP via 74HC595, implemented in `SharedStepRmt{.h,.cpp}` and `SharedStepAdapterEsp32{.h,.cpp}`.
- PlatformIO env: `esp32SharedStep` (see `platformio.ini`).
- Protocol: MOVE/HOME simplified to use global SPEED/ACCEL/DECEL for shared‑STEP builds.
- Decision: Park this workstream temporarily and continue with Fas (dedicated step) for reliability.

## Observed Issue (Bench)

With shared‑STEP enabled and Fas disabled:

- Even with a single motor moving, occasional missed steps lead to drift over repetitions.
- With multiple motors (MOVE:ALL), motors diverge — different drifts per motor; in some patterns “hardly moving” is observed for small steps.
- Fas baseline shows no noticeable drift under the same patterns.

Repro patterns used (excerpt from bench runner):

- Back/forth ramp sequence (10 reps):
  - `MOVE:ALL,-300`, `MOVE:ALL,0`, `MOVE:ALL,300`, `MOVE:ALL,600`, `MOVE:ALL,900`, `MOVE:ALL,0` → shared‑STEP: total chaos drift; Fas: fine.
- Incremental forward then zero:
  - `MOVE:ALL,100`, `200`, `300`, `400`, `500`, `0` → shared‑STEP: large drift; Fas: fine.
- Small steps:
  - `MOVE:ALL,50`, `100`, `150`, `200`, `250`, `0` → shared‑STEP: large drift; Fas: fine.
  - `MOVE:ALL,30` … `200`, `0` → shared‑STEP: some motors hardly move; Fas: small drift only.

Environment assumptions during testing:

- `SET SPEED=4000; SET ACCEL=16000; SET DECEL=0; HOME:ALL` via `tools/bench/manual_sequence_runner.py`.

## Likely Root Causes (Working Hypotheses)

- Ramp order bias: `updateRamp_()` runs from within per‑motor `updateProgress_()`. Motors later in the polling order integrate with a slightly different (newer) generator speed in the same tick, causing inter‑motor divergence at small distances.
- Edge/anchor mismatch: RMT “edge hook” fires on TX end, not strictly at STEP rising edges. Guard windows for DIR/SLEEP may be scheduled off‑center relative to real edges, occasionally causing missed/extra steps near flips or re‑enables.
- Gating granularity: SLEEP/DIR changes are scheduled from the main loop; if the guard window is narrowly computed and the poll is late, the fallback fast‑forward can re‑enable too early/late.
- Double integration per tick: `isMoving()` and `currentPosition()` both call `updateProgress_()`, possibly skewing software‑tracked positions versus actual pulses for very short moves.
- Pulse shape constraints: At certain speeds, high/low durations may approach peripheral limits; marginal high time could under‑step some DRV8825s.

## Proposed Diagnostic & Fix Plan (When Resuming)

Instrumentation (low‑overhead):

- ISR pulse counters: in the STEP ISR (edge hook), increment `delivered_pulses[i]` for each motor whose SLEEP is HIGH; keep `global_pulse_count`.
- DIAG dump: `GET DIAG:SHAREDSTEP` prints per‑motor: software `pos`, `delivered_pulses`, `awake`, `dir`, `moving`, `target`, `acc_us`, plus global `current_speed_sps_`, `v_cur_sps_`, `period_us_`, and a `ramp_update_count`.
- Guard trace: record timestamps for `t_sleep_low`, `t_dir_flip`, `t_sleep_high` and distance from the last perceived edge anchor; keep a small ring buffer for DIAG.

Validation:

- Single‑motor small steps (e.g., `MOVE:4,50…250…0`): verify `delivered_pulses` equals commanded delta and matches mechanical displacement.
- Multi‑motor small steps (MOVE:ALL): compare `delivered_pulses` across motors; divergence suggests order bias.
- Direction flips: ensure guard timestamps sit away from edges and that pulses around flips are not off‑by‑one.

Fix directions:

- Decouple ramp updates from per‑motor integration: compute ramp once per scheduler tick (or per STEP edge), then integrate all motors using the same `current_speed_sps_` for that dt.
- Correct edge anchoring: align anchor to actual rising edges (or compensate with half‑period) so guard windows sit mid‑gap.
- Avoid duplicate progress updates per loop and ensure constant integration cadence.
- Tighten guard feasibility or temporarily slow during flips at extreme speeds.

## Relevant Sources

- Adapter: `include/drivers/Esp32/SharedStepAdapterEsp32.h`, `src/drivers/Esp32/SharedStepAdapterEsp32.cpp`
- RMT generator: `include/drivers/Esp32/SharedStepRmt.h`, `src/drivers/Esp32/SharedStepRmt.cpp`
- Timing helpers: `include/MotorControl/SharedStepTiming.h`, `include/MotorControl/SharedStepGuards.h`
- Bench runner: `tools/bench/manual_sequence_runner.py`
- PlatformIO env: `platformio.ini` (section `[env:esp32SharedStep]`)
