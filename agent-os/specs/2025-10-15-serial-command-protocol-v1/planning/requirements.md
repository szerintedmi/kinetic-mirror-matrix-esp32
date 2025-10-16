# Spec Requirements: Serial Command Protocol v1

## Initial Description

let's create the spec for the first roadmap item

## Requirements Discussion

### First Round Questions

**Q1:** I’m assuming commands use single-line, space-delimited, uppercase verbs with newline termination, like `MOVE 3 1200 500 800` and `HELP`. Is that correct, or should we support commas/JSON payloads?
**Answer:** 1.  <CMD>:param1,param2 is easier to parse and read. Also it allows to ommit optional parameters to keep those at default

**Q2:** For responses, I’m proposing `CTRL:OK` and `CTRL:ERR <code> <message>` (per serial-interface standards), one line per request. Is that acceptable, or do you prefer plain `OK`/`ERR <code>` without the `CTRL:` prefix?
**Answer:** 2. CTRL: prefix is fine

**Q3:** Motor addressing: I assume IDs are 1–8 plus `ALL` (e.g., `MOVE ALL ...`). Is that correct, or do you prefer 0–7? Should we accept zero-padded IDs?
**Answer:** 3. I would keep 0-7 to map directly array indexes and avoid +1 confusion all the time . WDYT?

**Q4:** Defaults: I propose default MOVE `speed` in steps/s and `accel` in steps/s^2 pulled from firmware constants when omitted. Is that correct, or should the CLI always specify speed/accel explicitly?
**Answer:** 4. defaults can come from code constants for now. use 4000 steps/s for speed, 16000 steps/s^2 for accel, -1200 and +1200 motion limit (2400 total full steps) , homing overshoot 800 full steps, 150 backoff

**Q5:** Auto power: The roadmap says auto‑WAKE before motion and auto‑SLEEP after completion. Should we also add an optional idle timeout to auto‑SLEEP after X seconds of inactivity, or keep only post‑motion sleep for v1?
**Answer:** 5. autosleep only after MOVE and HOME commands

**Q6:** HOME parameters/order: `HOME [id|ALL] [overshoot] [backoff] [speed] [accel] [full_range]`, with conservative defaults and clamped limits. Is that the exact order and behavior you want for v1?
**Answer:** 6. no clamped limits for HOME  params but

**Q7:** Error taxonomy: I propose a small, fixed set: `E01 BAD_CMD`, `E02 BAD_ID`, `E03 BAD_PARAM`, `E04 BUSY`, `E05 NOT_HOMED`, `E06 INTERNAL`. Should we use these codes and include short help via `HELP`?
**Answer:** 7. I'm ok with these codes. though no need to surface them in help. also no need for E05 NOT_HOMED for now - we can later add auto homing at power on later, for now we must be sure we home before we start any move

**Q8:** CLI deliverable: Plan to provide a Python 3 CLI (pyserial) at `tools/serial_cli.py` supporting `help`, `move`, `home`, `status`, `wake`, `sleep`, plus a basic test script for E2E. Good starting place, or prefer another layout/entrypoint?
**Answer:** 8. ok

**Q9:** Anything explicitly out of scope for v1 (e.g., relative moves, queues, multi‑line responses, checksums, or persistent config)?
**Answer:** 9. no relative moves (apart from HOME which is only relative), no queuing (new pos or homing  command while previous one is not finished throws error) . multi line response: help and status will require multi line, no checksums, no persistent config

### Existing Code to Reference

Provided code examples from Proof-of-concept:

- `agent-os/specs/2025-10-15-serial-command-protocol-v1/planning/code_examples/StepperMotor.hpp`
- `agent-os/specs/2025-10-15-serial-command-protocol-v1/planning/code_examples/StepperMotor.cpp`

Note: Use as references only; do not constrain new code to these structures.

### Follow-up Clarifications (Confirmed)

- HOME: No clamping or limits on parameters. Purposefully allow overrides for diagnostics/tests. Use roadmap order `overshoot, backoff, speed, accel, full_range`. Format: `HOME:<id|ALL>,<overshoot>[,<backoff>][,<speed>][,<accel>][,<full_range>]`.
- MOVE out-of-range: Reject targets outside [-1200,+1200] with explicit position-out-of-limits error code (see Error Codes below).
- STATUS: Multi-line, one line per motor including id, position, speed, accel, and moving/awake flags.

## Visual Assets

### Files Provided

No visual assets provided.

### Visual Insights

None.

## Requirements Summary

### Functional Requirements

- Command grammar uses `<CMD>:param1,param2` with optional parameters omitted to use defaults.
- Supported commands in v1: `HELP`, `MOVE`, `HOME`, `STATUS`, `WAKE`, `SLEEP`.
- Responses prefixed with `CTRL:`; single-line for most commands, multi-line for `HELP` and `STATUS`.
- Motor addressing: IDs `0–7` and `ALL`.
- MOVE: absolute position steps, format `MOVE:<id|ALL>,<abs_steps>[,<speed>][,<accel>]`; defaults speed `4000` steps/s and accel `16000` steps/s^2 when omitted; motion limits target range `[-1200, +1200]` full steps (total range 2400).
- HOME: relative homing with parameters (overshoot, backoff, optional speed/accel/full_range). No clamped limits (confirmed). Defaults: overshoot `800` steps, backoff `150` steps. Parameter order: `overshoot, backoff, speed, accel, full_range`. All parameters are optional; `HOME:<id|ALL>` is valid and uses defaults.
- WAKE/SLEEP: `WAKE:<id|ALL>` and `SLEEP:<id|ALL>` with immediate effect; no extra params.
- Auto-sleep only after completion of `MOVE` and `HOME` (no idle-timeout autosleep in v1). In later hardware integration, this will align with FastAccelStepper's per-motor auto-enable/auto-sleep; manual wake/sleep control is not required for normal operation.
- Busy behavior: while a motion/homing is active, further `MOVE`/`HOME` return `CTRL:ERR E04 BUSY`.
- Out-of-range MOVE: targets beyond [-1200,+1200] rejected with explicit error code.
- Error codes used: `E01 BAD_CMD`, `E02 BAD_ID`, `E03 BAD_PARAM`, `E04 BUSY`, `E06 INTERNAL`, `E07 POS_OUT_OF_RANGE`. Omit `E05 NOT_HOMED` for now. Do not list codes in `HELP`.
- Host CLI deliverable at `tools/serial_cli.py` implementing end-to-end tests for all commands.

### Non-Functional Requirements

- Serial: 115200 baud; follow guard parsing and payload length limits per `@agent-os/standards/frontend/serial-interface.md`.
- Usability: `HELP` must list verbs and parameter grammar succinctly; readable multi-line formatting.
- Robustness: Reject malformed payloads and out-of-range parameters with `CTRL:ERR` and appropriate codes.
- Concurrency: No queuing; enforce single in-flight motion per motor set; commands are processed atomically.
- Deterministic open-loop timing: Motion/homing completion uses a deterministic duration based on target delta and speed. This timing model is canonical (not a temporary stub) and remains valid for open-loop control; later phases may add a small start delay to mirror FastAccelStepper behavior.

### Reusability Opportunities

- Leverage concepts from PoC `StepperMotor` (awake state management, auto-sleep timer hooks, active move tracking) where practical.
- Adhere to `serial-interface` conventions (`CTRL:` prefix, help formatting, guard parsing) to ensure consistency.
- Introduce a `MotorBackend` abstraction: `StubBackend` in this item; `HardwareBackend` with FastAccelStepper and 74HC595 in roadmap item 2.

### Scope Boundaries

**In Scope:**

- Command parsing and validation for `HELP/MOVE/HOME/STATUS/WAKE/SLEEP`.
- Single-board, single-node USB serial control; 0–7 addressing and ALL broadcast.
- Python CLI for manual/E2E testing over USB.

**Out of Scope:**

- Relative moves (beyond internal HOME behavior).
- Command queuing or pipelines.
- Checksums, persistent config, or idle autosleep timers.
- Extended diagnostics beyond minimal `STATUS` (detailed version planned in later roadmap items).

### Technical Considerations

- Parameter grammar: comma-separated, strictly ordered; optional suffix params default if omitted.
- Addressing: zero-based IDs map directly to internal arrays; accept `ALL` broadcast.
- Limits: motion target range `[-1200,+1200]`; out-of-range MOVE rejected with `CTRL:ERR E07 POS_OUT_OF_RANGE`.
- Response format: `CTRL:OK` and `CTRL:ERR <code> <message>`; multi-line blocks for `HELP` and `STATUS`.
- Host CLI: Python 3 + `pyserial`; simple cross-platform usage and tiny E2E checks.
- Motion duration calculation (canonical): `duration_ms = ceil(1000 * |target - current| / max(speed,1))`. Acceleration is ignored in v1 timing; a small fixed `start_delay_ms` may be added in later items to align with FastAccelStepper startup behavior.
- WAKE/SLEEP mapping for hardware (future): With FastAccelStepper's auto-enable, `WAKE`/`SLEEP` may act as diagnostics/overrides rather than required flow controls. Protocol keeps these verbs for consistency.
