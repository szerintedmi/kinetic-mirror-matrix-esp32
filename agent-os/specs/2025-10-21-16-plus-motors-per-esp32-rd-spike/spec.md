# Specification: 16+ Motors per ESP32 (Shared STEP Spike)

## Goal
Validate a shared-STEP architecture that scales beyond 8 motors on a single ESP32 by generating one hardware-timed STEP pulse train and using per-motor SLEEP gating and DIR control, while simplifying the command protocol to global speed/accel/decel.

## User Stories
- As a maker, I want multiple motors to move concurrently at a common speed with independent start/stop and direction so I can create larger, synchronized mirror arrays.
- As a developer, I want reliable, jitter-free step generation so later Wi‑Fi features won’t degrade motion quality.

## Core Requirements
### Functional Requirements
- Generate a single shared STEP pulse train using ESP32 hardware (hardware-timed; RMT preferred) up to 10 ksteps/s.
- Control each motor independently via 74HC595-driven DIR and SLEEP; use SLEEP as the per-motor gate.
- Remove per-MOVE and per-HOME speed/accel parameters; introduce global `SPEED`, `ACCEL`, and `DECEL` settings with existing defaults (DECEL defaults to 0 on shared‑STEP; symmetric on FAS).
- Reject changes to global `SPEED`/`ACCEL` while any motor is moving.
- Reject `SET DECEL` while any motor is moving.
- Ensure overlapping moves use the current global speed/accel profile.
- Apply microsecond-scale DIR guard windows (constants) that avoid STEP edges during direction changes.

### Non-Functional Requirements
- Jitter-free STEP generation at 10 ksteps/s target, robust under future Wi‑Fi/ESP‑Now activity.
- Deterministic gating timing relative to STEP edges.
- Unit-test coverage for guard timing and gating window calculations.

## Visual Design
No visual assets provided.

## Reusable Components
### Existing Code to Leverage
- 74HC595 driver: `src/drivers/Esp32/Shift595Vspi.cpp`, `include/drivers/Esp32/Shift595Vspi.h` (DIR/SLEEP control; see `docs/esp32-74hc595-wiring.md`).
- FAS adapter glue (reference for external-pin callbacks): `src/drivers/Esp32/FasAdapterEsp32.*`.
- Motor controller orchestration and state: `lib/MotorControl/src/HardwareMotorController.cpp`, `lib/MotorControl/include/MotorControl/HardwareMotorController.h`.
- Command parsing and responses: `lib/MotorControl/src/MotorCommandProcessor.cpp` (HELP, MOVE, HOME, STATUS, WAKE, SLEEP).
- Motion timing helpers and defaults: `lib/MotorControl/src/MotionKinematics.*`, `lib/MotorControl/include/MotorControl/MotorControlConstants.h`.

### New Components Required
- Shared STEP Generator: minimal in-house driver built on ESP32 hardware (RMT preferred; LEDC/timer fallback) that:
  - Produces continuous pulses at `SPEED` using precise period.
  - Exposes hooks to align gating/DIR transitions to safe edges.
  - Counts delivered pulses for position tracking or delegates counting to controller.
- Gating Coordinator: logic to schedule per-motor SLEEP on/off windows aligned to STEP edges; applies DIR guard constants around flips.

## Technical Approach
- Pulse Generation:
  - Prefer ESP32 RMT for fixed-period pulse train at `SPEED` (steps/s). On constraints, consider LEDC PWM or timer ISR as fallback.
  - Start/stop the generator based on whether any motor is currently moving.
- Gating & Direction:
  - Use 74HC595 outputs for DIR and SLEEP. Before a DIR flip, disable SLEEP for the affected motor for `DIR_GUARD_US` (constant), flip DIR, re-enable before the next STEP edge.
  - Ensure latch timing (RCLK) avoids STEP edges; batch DIR/SLEEP updates, then latch once per update.
- Command Protocol Changes:
  - Remove optional `<speed>,<accel>` from `MOVE` and `HOME` parsing. Add global `SPEED`/`ACCEL` setters/getters and `DECEL` (reject updates while moving).
  - Keep existing responses and thermal-limit behaviors unchanged.
- State & Timing:
  - Continue using existing motor state for positions; shared generator updates positions per delivered pulses while SLEEP is enabled.
  - Maintain estimate calculations for `est_ms` using MotionKinematics with the global speed/accel; for shared‑STEP, use asymmetric estimators with speed‑aware overhead so CTRL:OK and LAST_OP_TIMING agree.
- Testing:
  - Unit tests for DIR guard window calculation and edge-aligned gating.
  - Manual bench tests without logic analyzer: scripted back-and-forth runs at varying distances/rhythms; compare final physical alignment to MCU-tracked position after cycles.

## Out of Scope
- Microstepping modes.
- Wireless features (beyond ensuring step generation remains robust once Wi‑Fi is enabled later).
- PCB layout changes or added buffer hardware; stay with existing 74HC595 approach unless blockers arise.
- New telemetry counters; defer unless needed.

## Success Criteria
- Motors operate reliably up to 10 ksteps/s with clean stop/resume and direction flips using SLEEP gating.
- No perceptible drift in manual bench tests after multi-cycle back-and-forth patterns at target rates.
- Global `SPEED`/`ACCEL` applied consistently; attempts to change while moving are rejected with error.
- Unit tests pass for guard timing and gating logic; build remains stable under PlatformIO.

## Behavior Notes (Shared STEP)
- Global ramp coupling is expected: a single STEP time-base drives all motors. When a motor starts or ends a move (or HOME leg), the global acceleration/deceleration profile adapts to that motor’s constraints and remaining distance. Other motors still stepping will temporarily speed up or slow down accordingly.
- Concurrency trade-off: frequent short moves while a long move is active can increase the long move’s total duration. This is by design in the shared-STEP spike and considered acceptable for this phase.
- Direction unchanged on join: when a motor starts a move and its direction already matches the latched DIR, the adapter does not drop SLEEP or schedule a guard window; it enables immediately without a pause.
- Global MOVE guard: new MOVE commands are rejected while any motor is moving (prevents mid‑run joins). In multicommand batches that begin idle, multiple MOVEs/HOMEs can start together.
- Homing group barrier: homing legs start together within the homing mask; next leg begins only after all targeted motors finish the current leg.
- DECEL trade‑off: DECEL>0 produces smoother shared ramp‑downs near finishes (but slows other concurrent moves); DECEL=0 avoids finish‑induced slowdowns and relies on SLEEP gating for precise stops.
