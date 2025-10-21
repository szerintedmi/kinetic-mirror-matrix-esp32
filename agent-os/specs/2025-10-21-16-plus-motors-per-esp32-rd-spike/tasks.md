# Task Breakdown: 16+ Motors per ESP32 (Shared STEP Spike)

## Overview
Total Tasks: 20
Assigned roles: api-engineer, ui-designer, testing-engineer

## Task List

### Firmware: Shared STEP Engine

#### Task Group 1: Pulse Generator Spike (RMT)
**Assigned implementer:** api-engineer
**Dependencies:** None

- [x] 1.0 Prototype hardware-timed STEP generator
  - [x] 1.1 Write 2-6 focused unit tests for timing helpers (period calc, start/stop gating points)
  - [x] 1.2 Implement minimal RMT-based pulse generator at global `SPEED` (steps/s)
  - [x] 1.3 Expose start/stop and edge-aligned callback hooks
  - [x] 1.4 Add compile flag to select SharedSTEP vs FastAccelStepper paths
  - [x] 1.5 Ensure tests in 1.1 pass (native/host if possible)

**Acceptance Criteria:**
- Unit tests for timing helpers pass
- RMT generator produces expected periods by calculation and simulated counters
- Build compiles with a feature flag selecting the new path

#### Task Group 2: DIR/SLEEP Gating & Guards
**Assigned implementer:** api-engineer
**Dependencies:** Task Group 1

- [x] 2.0 Implement per-motor SLEEP gating + DIR guard windows
  - [x] 2.1 Write 2-6 unit tests for guard timing logic and “no-STEP during DIR flip” rules
  - [x] 2.2 Define constants `DIR_GUARD_US_[PRE,POST]` (microseconds) per spec
  - [x] 2.3 Integrate 74HC595 latch updates so SLEEP/DIR changes avoid STEP edges
  - [x] 2.4 Align re-enable timing to next safe STEP gap
  - [x] 2.5 Ensure tests in 2.1 pass

**Acceptance Criteria:**
- Guard constants used; logic prevents STEP coinciding with DIR transitions in tests
- 74HC595 updates are batched and latched once per change cycle

#### Task Group 3: Integration + Protocol Simplification (Constant Speed)
**Assigned implementer:** api-engineer
**Dependencies:** Task Groups 1-2

- [ ] 3.0 Integrate shared STEP into `HardwareMotorController` (constant speed only)
  - [ ] 3.1 Remove per-move/home `<speed>,<accel>` from parser and HELP output
  - [ ] 3.2 Add `GET SPEED` / `SET SPEED=<v>` (global); reject `SET SPEED` while any motor is moving
  - [ ] 3.3 Wire `MOVE`/`HOME` to shared STEP generator using global `SPEED`; ignore acceleration for now
  - [ ] 3.4 Maintain positions by counting pulses only while SLEEP is enabled per motor; start generator on first active motor, stop on last finish
  - [ ] 3.5 Enforce overlap rule: concurrent moves are allowed only at the single global `SPEED`
  - [ ] 3.6 Write 2-6 parser tests for simplified grammar and erroring on legacy params
  - [ ] 3.7 CLI smoke (manual): via `python -m serial_cli`
        - `GET SPEED` → defaults
        - `SET SPEED=4000` → `CTRL:OK`
        - `MOVE:0,200` then `STATUS` until `moving=0`
        - While moving, `SET SPEED=4500` → error (reject while moving)
        - `MOVE:0,200,4000` (legacy param) → `CTRL:ERR E03 BAD_PARAM`

**Acceptance Criteria:**
- Parser accepts simplified grammar; HELP shows no per-move/home speed/accel
- `SPEED` global applies; rejected while moving; CLI smoke passes
- Controller path updates position consistently; thermal checks preserved

### Protocol & Host Tools

#### Task Group 5: Host CLI/TUI Adjustments
**Assigned implementer:** ui-designer
**Dependencies:** Task Group 3

- [ ] 5.0 Update Python CLI/TUI to match protocol
  - [ ] 5.1 Write 2-4 focused tests for CLI command builders/parsing (if present)
  - [ ] 5.2 Remove speed/accel inputs from `MOVE`/`HOME` paths
  - [ ] 5.3 Update HELP and interactive hints to the new grammar
  - [ ] 5.4 Keep STATUS polling unchanged

**Acceptance Criteria:**
- CLI/TUI sends valid simplified commands and surfaces HELP correctly

#### Task Group 6: Acceleration (Global ACCEL + Trapezoid)
**Assigned implementer:** api-engineer
**Dependencies:** Task Group 3 (constant speed integrated)

- [ ] 6.0 Implement global acceleration support
  - [ ] 6.1 Add `GET ACCEL` / `SET ACCEL=<a>`; reject `SET ACCEL` while moving
  - [ ] 6.2 Add trapezoidal ramp scheduling in the shared STEP generator (period updates)
  - [ ] 6.3 Update estimates to use MotionKinematics with global `SPEED`/`ACCEL`
  - [ ] 6.4 Write 2-6 unit tests for ramp period scheduling and corner cases (short moves, accel-limited)
  - [ ] 6.5 CLI smoke (manual):
        - `SET SPEED=4000`, `SET ACCEL=16000`
        - `MOVE:0,800` → `CTRL:OK est_ms=...` then `GET LAST_OP_TIMING:0` to compare
        - While moving, `SET ACCEL=8000` → error (reject while moving)

**Acceptance Criteria:**
- Ramp logic produces expected period sequences in tests
- CLI smoke confirms commands and rejects during motion; estimates align with timing report

### Validation & Bench

#### Task Group 7: Unit Tests for Timing and Guards
**Assigned implementer:** testing-engineer
**Dependencies:** Task Groups 1-3, 6

- [ ] 6.0 Consolidate timing/guard tests
  - [ ] 6.1 Review tests from 1.1 and 2.1; add up to 6 more for edge cases
  - [ ] 6.2 Ensure host-native tests run under PlatformIO `env:native` if available

**Acceptance Criteria:**
- All timing/guard tests pass on host

#### Task Group 8: Manual Bench Routines (No Analyzer)
**Assigned implementer:** testing-engineer
**Dependencies:** Task Groups 3, 6

- [ ] 7.0 Create deterministic bench scripts and device routines
  - [ ] 7.1 Add a Python CLI script to run patterned back-and-forth sequences (varying distances/rhythms)
  - [ ] 7.2 Add a device-side routine to report final MCU-tracked positions after N cycles
  - [ ] 7.3 Lab checklist for comparing physical vs tracked positions

**Acceptance Criteria:**
- Scripted routine runs without code edits; produces repeatable sequences
- Operators can compare physical alignment vs. MCU-reported positions

## Execution Order
1. Task Group 1: Pulse Generator Spike (RMT)
2. Task Group 2: DIR/SLEEP Gating & Guards
3. Task Group 3: Integration + Protocol Simplification (Constant Speed)
4. Task Group 5: Host CLI/TUI Adjustments
5. Task Group 6: Acceleration (Global ACCEL + Trapezoid)
6. Task Group 7: Unit Tests for Timing and Guards
7. Task Group 8: Manual Bench Routines (No Analyzer)
