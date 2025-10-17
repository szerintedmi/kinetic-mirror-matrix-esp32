# Spec Requirements: Homing & Baseline Calibration (Bump‑Stop)

## Initial Description
3. [ ] Homing & Baseline Calibration (Bump‑Stop) — Implement bump‑stop homing without switches: drive toward an end for `full_range + overshoot` steps, back off `backoff` steps, then move `full_range/2` to midpoint and set zero. Parameters have safe defaults and can be overridden via `HOME` (order: overshoot, backoff, speed, accel, full_range). Enforce slow, conservative speed during HOME (defaults may be overridden). Depends on: 2. `M`

## Requirements Discussion

### First Round Questions

**Q1:** Direction: I’m assuming the default homing drive is toward the negative direction until the mechanical end is reached. Is that correct, or should we default to the positive direction?
**Answer:** the hard limits are mechanically identical in practice on the actuator. So any arbitary directions is fine, let's use negative direction

**Q2:** Defaults: For safety, I’m thinking conservative defaults like overshoot=50 steps, backoff=25 steps, speed=200 steps/s, accel=800 steps/s^2, full_range=2000 steps. Should we use these, or do you prefer different baseline values?
**Answer:** we need higher overshoot, use Defaults: `overshoot=800`, `backoff=150` . The motor is tiny and fast, the enclusore is solid, DRV8825 current limited so overdriving for ca. max 1s in worts case doesn't do harm 

**Q3:** Concurrency: For `HOME ALL`, I’m assuming we run motors sequentially (or in small batches) to reduce current spikes and heat. Is that correct, or should we home all motors simultaneously?
**Answer:** for now home all runs concurrently, same as MOVE ALL. 8 motors are perfectly fine (each <100mA peak) . We will introduce current budgeting/batching in later roadmap items when we introduce more motors

**Q4:** Post‑home state: I’m assuming `HOME` auto‑WAKEs as needed and then auto‑SLEEPs each motor after homing completes (consistent with MOVE). Is that the desired behavior, or should motors remain awake after homing?
**Answer:** yes, home exact same behaviour as MOVE, autowakes, autosleeps but  also respects WAKE status just like MOVE

**Q5:** Persistence: Should the computed zero midpoint be persisted across reboots (e.g., LittleFS) or remain in‑memory only until power cycle/reset?
**Answer:** no persistance. to be clear: nothing computed. we just move the motor to a 0 position with homing and reset the current position to 0 with fas

**Q6:** Parameter handling: I’m assuming the `HOME` parameter order is `[overshoot, backoff, speed, accel, full_range]` and that omitted parameters use defaults per motor. Is that correct?
**Answer:** correct. eg. `HOME:2,,50` only overrides backoff and uses the default overshoot, speed accel etc.

**Q7:** Safety clamp: To guard against misconfigured ranges, I propose clamping total travel to `full_range + overshoot` and returning an error if commanded beyond that during HOME. Proceed with this guard?
**Answer:** this question doesn't make sense. how could I command higher more than full_range + overshoot? ?? or you expect a fas moveTo command travels more than expected? if so then we don't need to prepare for that

**Q8:** Homed flag and STATUS: Do you want a per‑motor “homed” flag now (internally), with `STATUS` extension deferred to roadmap item 4, or should we include a minimal homed indicator in STATUS immediately?
**Answer:** defer homed flag/status to next roadmap item

**Q9:** Per‑motor variation: Should all motors share the same default `full_range` and parameters, or do you want per‑motor overrides configurable via the `HOME` command?
**Answer:** motors are identical same default for all of them for now. We might introduce some calibration settings later but don't worry about it for now.

**Q10:** Out of scope: Are there any scenarios explicitly out of scope for this iteration (e.g., endstop switches, stall/current sensing, persistent calibration files, or auto‑retries)?
**Answer:** out of scope for this roadmap item -  beyond the above discussed-  no endstop, no stall sensing, no persisting calibration and no auto-retries

### Existing Code to Reference
User indicated prototype homing logic using FastAccelStepper:
- Path: `agent-os/specs/2025-10-15-serial-command-protocol-v1/planning/code_examples/StepperMotor.hpp`
- Path: `agent-os/specs/2025-10-15-serial-command-protocol-v1/planning/code_examples/StepperMotor.cpp`

Notes: Prototype differs in structure (no 74HC595 integration, auto‑enable not via FAS). Main learning to adopt is the homing logic approach. Do not directly reuse code; use as conceptual reference.

### Follow-up Questions
No additional follow‑ups required at this time.

## Visual Assets

No visual assets provided.

## Requirements Summary

### Functional Requirements
- Provide `HOME` command implementing bump‑stop homing without switches.
- Direction: default homing travel toward negative limit.
 - Sequence per motor: drive negative for `full_range + overshoot`, back off `backoff`, then move `full_range/2` to reach the midpoint, set logical position to zero in FastAccelStepper.
 - Rationale: `backoff` is the safety buffer between physical stops and our soft limits. After anchoring at the hard stop and backing off to the negative soft boundary, moving `full_range/2` lands at the center of the soft range; subsequent MOVE commands constrained to ±`full_range/2` cannot hit physical limits.
- Concurrency: `HOME ALL` runs concurrently for up to 8 motors (similar to MOVE ALL).
- Auto‑WAKE/SLEEP behavior mirrors MOVE and respects manual WAKE status (i.e., do not auto‑SLEEP if a motor is explicitly WAKEd).
- Parameters (order): `overshoot, backoff, speed, accel, full_range`; omitted parameters use defaults.
- Comma placeholders to skip parameters are acceptable (per example `HOME:2,,50`). Serial grammar remains aligned with v1.
 - Use FastAccelStepper relative motion for HOME segments (e.g., `move(...)`), not absolute `moveTo(...)`.
 - Do not enforce normal motion limits/soft limits during HOME; treat HOME as calibration that may exceed nominal bounds.

### Non-Functional Requirements
- Safety: High overshoot permitted given physical constraints and current limiting; no extra guard required beyond algorithm.
- No persistence: Calibration/zero is not stored across reboots; only runtime state is updated.
- Performance: Concurrent homing across 8 motors acceptable with current draw <100 mA per motor.
 - Limits enforcement: Normal soft/positional limits are not applied during HOME; rely on physical bump-stop and DRV8825 current limiting.

### Reusability Opportunities
- Reference the prototype homing logic in `StepperMotor.hpp/.cpp` for algorithmic flow, adapting to the current architecture with 74HC595 and FAS auto‑enable.

### Scope Boundaries
**In Scope:**
- Bump‑stop homing algorithm and CLI command parameters as described.
- Concurrent homing for 8 motors.
- Auto‑WAKE/SLEEP semantics consistent with MOVE.

**Out of Scope:**
- Endstop switch support.
- Stall/current sensing.
- Persistent calibration files or storing zero across reboots.
- Auto‑retries or fault‑recovery logic.
- STATUS surface changes (homed flag deferred to roadmap item 4).

### Technical Considerations
 - Defaults:
  - overshoot = 800 steps
  - backoff = 150 steps
  - speed = 4000 steps/s (same as MOVE default)
  - accel = 16000 steps/s^2 (same as MOVE default)
  - full_range = current MOVE soft range = `kMaxPos - kMinPos` (currently 2400 steps from kMinPos=-1200, kMaxPos=1200)
- Implementation should integrate with FastAccelStepper and existing 74HC595 control for DIR/SLEEP, using the same external‑pin callback strategy as MOVE.
- Ensure `HOME` respects existing task structure and command parser patterns from Serial Command Protocol v1.
 - Use FAS relative API (`move`) for homing phases; avoid `moveTo` to prevent unintended absolute target interactions.
 - Do not apply internal clamps for HOME relative moves; computation still uses `full_range` and `overshoot` as algorithm parameters only.
