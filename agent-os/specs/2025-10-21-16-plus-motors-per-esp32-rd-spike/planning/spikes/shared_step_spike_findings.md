# Shared STEP Line Spike — Approach, Findings, and Learnings

This write‑up summarizes the quick technical spike we ran to evaluate driving multiple stepper drivers from a single shared STEP line. It intentionally avoids implementation details (we used disposable hacks for the spike and have since reverted them). The goal here is to capture the approach, constraints, observations, and recommendations for a proper design.

## Context and Goal

- Explore whether multiple motors can share a single STEP signal while maintaining independent direction (DIR) and enable/sleep (EN) control.
- Validate practical limits: start/stop accuracy, direction changes, homing, and position tracking when multiple motors move concurrently.
- Keep changes minimal and temporary; focus on bench‑level feasibility, not production quality.

## Test Setup and Assumptions

- Physical wiring: multiple drivers’ STEP inputs tied to one MCU STEP pin (the “source”).
- Per‑motor control: DIR and EN/SLEEP are independently driven per motor (e.g., via shift registers), so each driver starts/stops independently even with a shared STEP pulse train.
- Motion engine: a per‑motor motion planner (e.g., FastAccelStepper instances) retains each motor’s target, speed, accel, and position tracking.
- Speeds: tests used moderate speeds (e.g., 2–4 kHz) to observe behavior across direction changes and homing.

## Approach (bench‑only, disposable)

- Nominate one MCU STEP pin as the shared “source.” While any non‑source motor is moving, keep that source pulsing at the move’s speed.
- Use DIR and EN/SLEEP as gates per motor: a motor only steps while its driver is enabled and DIR is asserted appropriately.
- Consistent, single default speed instead per motor / command speed settings so the shared pulse train matches all motors’ expectations.
- For testing trigger near‑simultaneous actions by sending multiple commands in one device line (semicolon separated). This reduces host/serial latency skew for testing
- Prefer small or no added pause around DIR flips. Millisecond‑scale guards at high STEP frequency can drop many pulses and cause under‑stepping.

## What Worked

- Eight motors shared one STEP line reliably when overlapping moves used the same speed/accel profile.
- EN/SLEEP gating allowed independent start/stop while the shared STEP continued for others still moving.
- Homing sequences worked if direction flips and enable windows were coordinated so that no STEP edges occurred during DIR transitions (or only very short microsecond guards were used).
- Using a global default speed for omitted parameters helped harmonize overlapping moves.

## Constraints and Failure Modes We Saw

- Same‑speed requirement for overlaps: a single pulse train cannot satisfy different target speeds and acceleration concurrently. Divergent speeds must not be allowed
- Changing the global default speed or accel must be rejected with error code if any move is in progress.
- Direction change hazards:
  - Long (millisecond‑scale) “settle” pauses at high STEP rates cause significant pulse loss, leading to under‑stepping and position drift.
  - Too little guard can sample DIR too close to STEP on some drivers. A microsecond‑level (not millisecond) guard is more appropriate.
- Position vs. physical reality drift:
  - If pulses are missed (e.g., due to pausing the source), the planner’s position can claim success while the physical axis is short.

## Recommendations for a Production‑Ready Design

- Architect a proper shared‑STEP scheduler:
  - A single pulse generator (e.g., a hardware timer, RMT, or PWM) drives the shared STEP. Decide if there is still any need for FastAccelStepper - ie. we already estimate move times, if we generate pulses we could count steps and get rid of FastAccelStepper. Potential approach would be to keep fastAccelstepper interface and switch over to our implentation of a minimalistic driver: generating pulses, callbacks for enable/disable and position tracking as per current FastAccelStepper interface.
  - Per‑motor “gate windows” enable/disable EN with precise timing aligned to pulse edges.
  - Coordinate DIR changes in gaps between pulses or with short, deterministic guards.
- Unify speed while overlapping:
  - constrain overlapping moves to a single common speed profile .
- Add observability:
  - Telemetry for “pulses delivered vs. planned” per motor; latch last trigger timestamps for start/stop and DIR changes.
- Robust configuration:
  - Explicit source selection; clear diagnostics when the selected source doesn’t match expected wiring.
- Testing strategy:
  - Unit tests for gating windows around pulse edges.
  - HIL tests that exercise direction flips and staggered finishes at various speeds/accels.
  - Logic analyzer captures to verify that EN/DIR transitions avoid STEP edges.

## Suggested Next Steps

- Draft a spec for the shared‑STEP scheduler and gating model.
- Choose the hardware mechanism for pulse generation (e.g., ESP32 RMT) and DIR/EN update timing.
- Prototype a production‑intended path (separate from the spike) with:
  - Microsecond‑scale DIR guards
  - Deterministic start/stop aligned to pulse edges
  - Explicit overlapping‑speed rules
- Build minimal instrumentation to confirm “no missed pulses” in overlapping cases and under direction flips.
