# Specification: Homing & Baseline Calibration (Bump‑Stop)

## Goal
Provide a robust, switch‑less homing routine that establishes a repeatable zero baseline per motor using a bump‑stop algorithm, controllable via the existing Serial v1 `HOME` command.

## User Stories
- As an operator, I can issue `HOME:<id|ALL>` to quickly baseline one or all motors without endstop switches.
- As a developer, I can pass optional parameters to tune homing (overshoot, backoff, speed, accel, full_range) and skip some with commas.
- As an operator, I see HOME behave like MOVE regarding auto‑WAKE/SLEEP and concurrency for `ALL` targets.

## Core Requirements
### Functional Requirements
- Implement bump‑stop homing without switches:
  - Default direction: negative.
  - Sequence per motor: move negative by `full_range + overshoot`, back off positive by `backoff` to the negative soft boundary, then move positive by `full_range/2` to reach the soft-range midpoint, and set logical position to zero in FastAccelStepper.
- Concurrency: `HOME ALL` runs concurrently for up to 8 motors, similar to `MOVE ALL`.
- Auto‑WAKE/SLEEP: Same behavior as MOVE and respects WAKE overrides (do not auto‑sleep a motor that is manually WAKEd).
- Parameters and order: `HOME:<id|ALL>[,<overshoot>][,<backoff>][,<speed>][,<accel>][,<full_range>]` with omitted values defaulted; commas can skip fields (e.g., `HOME:2,,50`).
- Relative motion: Perform homing as a series of relative segments (compute absolute targets from current position if adapter only supports absolute).
- No limits enforcement during HOME: HOME ignores normal soft/positional limits; rely on physical bump‑stop and current limiting.
- No persistence: Do not store calibration across reboots; after homing, call FAS `setCurrentPosition(0)` to establish runtime zero.

### Non-Functional Requirements
- Defaults: `overshoot=800`, `backoff=150`, `speed=4000 steps/s`, `accel=16000 steps/s^2`, `full_range = kMaxPos - kMinPos` (derived from current MOVE soft range; currently 2400 with kMinPos = -1200, kMaxPos = 1200).
- Safety: Overdrive at the stop is acceptable given enclosure rigidity and DRV8825 current limiting; no extra clamping needed.
- Performance: Concurrent homing for 8 motors is acceptable (<100 mA peak per motor).

## Visual Design
- None required; control surface is Serial v1.

## Reusable Components
### Existing Code to Leverage
- Parser/handlers: `lib/MotorControl/src/MotorCommandProcessor.cpp` (`handleHOME`, defaults, grammar parsing).
- Controller interface: `lib/MotorControl/include/MotorControl/MotorController.h` (`homeMask`, `moveAbsMask`).
- Hardware controller: `lib/MotorControl/src/HardwareMotorController.cpp` (busy rules, FAS/74HC595 integration, auto‑enable callbacks).
- FAS adapter: `src/drivers/Esp32/FasAdapterEsp32.*` (`startMoveAbs`, `setCurrentPosition`, auto‑enable, external pin handler for DIR/SLEEP).
- Serial console: `src/console/SerialConsole.cpp`.
- Reference prototype (conceptual): `agent-os/specs/2025-10-15-serial-command-protocol-v1/planning/code_examples/StepperMotor.hpp/.cpp` (homing flow and relative moves).

### New Components Required
- Homing routine/state in `HardwareMotorController::homeMask` to orchestrate the three relative segments and final zeroing. Rationale: current `homeMask` is a stub that moves to absolute 0 only.
- Unit tests covering HOME parsing, busy rules, concurrent `ALL` behavior, and integration with FAS adapter stubs. Rationale: ensure parity with MOVE patterns and prevent regressions.

## Technical Approach
- Command parsing: Reuse `MotorCommandProcessor::handleHOME` (already in place). Keep grammar and defaults aligned with MOVE.
- Motion sequence per targeted motor id:
  1) Negative segment: delta1 = `-(full_range + overshoot)`; start abs target = `cur + delta1`.
  2) Backoff: delta2 = `+backoff`; target2 = `target1 + delta2`.
  3) Centering: delta3 = `+(full_range/2)`; target3 = `target2 + delta3`.
  4) On completion, call `setCurrentPosition(id, 0)`.
  - Rationale: `full_range` is the soft range (MOVE’s `kMaxPos - kMinPos`), while `backoff` creates buffer to the physical stops; centering uses the soft range midpoint.
- Adapter constraints: If only absolute moves are available, compute absolute targets from current positions for each segment; otherwise use relative moves.
- Busy rules: Match MOVE — reject HOME if any selected motor is already moving.
- WAKE/SLEEP: Mirror MOVE semantics; honor manual WAKE to keep motors awake.
- No STATUS changes: Homed flags or extra diagnostics are out of scope (defer to roadmap item 4).

## Out of Scope
- Endstop switches, stall/current sensing, persistent calibration, and auto‑retries.
- STATUS extensions (homed flag, diagnostics).

## Success Criteria
- Serial `HELP` lists HOME with the documented grammar and parameters.
- `HOME:<id>` homes a single motor per the sequence and sets runtime zero.
- `HOME:ALL` starts concurrent homing on eight motors without errors; MOVE and WAKE/SLEEP semantics remain consistent.
- After completion, motors auto‑sleep unless forced awake.
- Unit tests pass for parsing, busy rules, and controller sequencing; bench checks confirm expected behavior with DIR/SLEEP and STEP pulses.
