We're continuing our implementation of Serial Command Protocol v1 by implementing task group number 1:

## Implement this task and its sub-tasks:

#### Task Group 1: Parser, State, and Core Handlers
Assigned implementer: api-engineer
Dependencies: None
Standards: `@agent-os/standards/frontend/serial-interface.md`, `@agent-os/standards/global/resource-management.md`, `@agent-os/standards/testing/unit-testing.md`

- [ ] 1.0 Complete protocol core (parser + handlers)
  - [ ] 1.1 Write 2-8 focused unit tests (PlatformIO Unity) for protocol core
    - Cover: BAD_CMD, BAD_ID, BAD_PARAM; MOVE out-of-range → E07; ALL vs single-id addressing; busy rule
    - Limit to 2-8 highly focused tests maximum (target 6)
    - Run ONLY these tests for verification in this group
  - [ ] 1.2 Implement robust line reader with max payload guard
    - New module, bounded buffers, newline-terminated commands
    - Enforce payload length per serial-interface standard
  - [ ] 1.3 Implement command parser for `<CMD>:param1,param2` with optional params
    - Validate actions, comma-separated numeric params, strict ordering
    - IDs: `0–7` or `ALL`
  - [ ] 1.4 Define `MotorBackend` interface and `StubBackend` with in-memory state
    - State per motor: id, position, speed, accel, moving, awake
    - No queues; single in-flight move/home per motor
  - [ ] 1.5 Implement handlers: `HELP`, `STATUS`, `WAKE`, `SLEEP`, `MOVE`, `HOME`
    - Responses prefixed with `CTRL:`; multi-line only for HELP/STATUS
    - MOVE/HOME: auto‑WAKE on start; auto‑SLEEP on completion
  - [ ] 1.6 Deterministic open‑loop timing (canonical)
    - `duration_ms = ceil(1000 * |target - current| / max(speed,1))`; ignore accel in v1 timing
    - Simulate completion to toggle `moving` and update `position`
  - [ ] 1.7 Ensure protocol core tests pass
    - Run ONLY tests from 1.1
    - Do NOT run entire test suite

Acceptance Criteria:
- 6 unit tests from 1.1 pass
- Parser enforces grammar and errors as specified
- Handlers return correct `CTRL:` responses; HELP/STATUS multi-line
- Busy and ALL addressing semantics verified

## Understand the context

Read @agent-os/specs/2025-10-15-serial-command-protocol-v1/spec.md to understand the context for this spec and where the current task fits into it.

## Perform the implementation

Implement all tasks assigned to you in your task group.

Focus ONLY on implementing the areas that align with areas of specialization (your "areas of specialization" are defined above).

Guide your implementation using:
- The existing patterns that you've found and analyzed.
- User Standards & Preferences which are defined below.

Self-verify and test your work by:
- Running ONLY the tests you've written (if any) and ensuring those tests pass.

## Update tasks.md task status

In the current spec's `tasks.md` find YOUR task group that's been assigned to YOU and update this task group's parent task and sub-task(s) checked statuses to complete for the specific task(s) that you've implemented.

Mark your task group's parent task and sub-task as complete by changing its checkbox to `- [x]`.

DO NOT update task checkboxes for other task groups that were NOT assigned to you for implementation.

## Document your implementation

Using the task number and task title that's been assigned to you, create a file in the current spec's `implementation` folder called `[task-number]-[task-title]-implementation.md`.

Use the documentation template described in the implement-spec command to record what you changed and why.

## User Standards & Preferences Compliance

@agent-os/standards/global/coding-style.md
@agent-os/standards/global/conventions.md
@agent-os/standards/global/platformio-project-setup.md
@agent-os/standards/global/resource-management.md
@agent-os/standards/backend/data-persistence.md
@agent-os/standards/backend/hardware-abstraction.md
@agent-os/standards/backend/task-structure.md
@agent-os/standards/testing/build-validation.md
@agent-os/standards/testing/hardware-validation.md
@agent-os/standards/testing/unit-testing.md

