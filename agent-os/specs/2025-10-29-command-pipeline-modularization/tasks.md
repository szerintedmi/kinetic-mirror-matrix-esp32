# Task Breakdown: Command Pipeline Modularization

## Overview
Total Tasks: 3
Assigned roles: api-engineer, testing-engineer

## Task List

### Core Firmware Refactor

#### Task Group 1: Command Pipeline Architecture
**Assigned implementer:** api-engineer
**Dependencies:** None

- [x] 1.0 Establish modular command pipeline
  - [x] 1.1 Write 3-5 focused unit tests covering parser edge cases (quoted CSV, alias handling, batch conflict detection)
  - [x] 1.2 Introduce `CommandExecutionContext` encapsulating controller access, CID allocation, and minimal clock/net helpers
  - [x] 1.3 Extract parsing/validation helpers into a reusable `command_utils` module
  - [x] 1.4 Implement `CommandParser`, `ParsedCommand`, and `BatchExecutor` preserving current batch semantics
  - [x] 1.5 Implement `CommandRouter` and handler classes (`MotorCommandHandler`, `QueryCommandHandler`, `NetCommandHandler`) using the shared context
  - [x] 1.6 Introduce `CommandResult` and response formatter that renders identical serial output (including CID reuse and warnings)
  - [x] 1.7 Replace legacy monolithic flow with new façade (retaining public entry point) and ensure all existing unit tests pass by running ONLY the new tests from 1.1 plus pre-existing suites touching the command processor

**Acceptance Criteria:**
- New abstractions compile and integrate without altering observable command behavior
- New targeted tests from 1.1 pass alongside existing command processor tests
- Serial responses (single and batched) match prior formatting and CID sequencing

### Adapter & Integration Cleanup

#### Task Group 2: Transport Adapter Preparation
**Assigned implementer:** api-engineer
**Dependencies:** Task Group 1

- [x] 2.0 Adapt transports to the modular pipeline
  - [x] 2.1 Write 2-4 focused adapter tests (e.g., ensure serial façade returns identical strings, structured payload round-trips)
  - [x] 2.2 Ensure serial command entry point delegates to the new pipeline without behavioral regressions
  - [x] 2.3 Document integration points for future transports (comments/readme snippets), clarifying expected use of `CommandResult`
  - [x] 2.4 Run ONLY the tests authored in 2.1 to verify adapter behavior

**Acceptance Criteria:**
- Serial adapter uses the modular pipeline and produces unchanged output
- Structured payload documented for future MQTT work without introducing new transport logic
- Tests from 2.1 pass reliably

### Quality Assurance

#### Task Group 3: Test Gap Review & Regression Audit
**Assigned implementer:** testing-engineer
**Dependencies:** Task Groups 1-2

- [x] 3.0 Validate coverage and regression safety
  - [x] 3.1 Review tests from Task Groups 1 and 2 to map covered scenarios
  - [x] 3.2 Identify any remaining high-risk behaviors (e.g., CID reuse in nested batches, NET command fallbacks) lacking targeted tests
  - [x] 3.3 Add up to 6 additional regression-focused tests covering uncovered critical paths (reuse existing harnesses; no new frameworks)
  - [x] 3.4 Run ONLY the combined feature-specific tests (from 1.1, 2.1, and 3.3)

**Acceptance Criteria:**
- Added tests address critical uncovered behaviors without exceeding limits
- All feature-specific tests pass
- Report confirms existing command semantics preserved (formatting, warnings, BUSY handling)

## Execution Order
1. Task Group 1: Command Pipeline Architecture
2. Task Group 2: Transport Adapter Preparation
3. Task Group 3: Test Gap Review & Regression Audit
