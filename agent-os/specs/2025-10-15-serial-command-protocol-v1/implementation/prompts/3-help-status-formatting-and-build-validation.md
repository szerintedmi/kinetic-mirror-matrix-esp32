We're continuing our implementation of Serial Command Protocol v1 by implementing task group number 3:

## Implement this task and its sub-tasks:

#### Task Group 3: HELP/STATUS formatting and Build Validation
Assigned implementer: api-engineer
Dependencies: Task Group 1
Standards: `@agent-os/standards/testing/build-validation.md`, `@agent-os/standards/global/platformio-project-setup.md`, `@agent-os/standards/frontend/serial-interface.md`

- [ ] 3.0 Finalize HELP and STATUS outputs and validate build
  - [ ] 3.1 Write 2-8 focused unit tests (Unity)
    - Validate HELP contains all verbs with correct param grammar (no error codes listed)
    - Validate STATUS has one line per motor including id, pos, speed, accel, moving, awake
    - Limit to 2-8 tests (target 4)
  - [ ] 3.2 Implement HELP generator and STATUS reporter
    - Terse, skimmable formatting per spec; strictly ordered params
  - [ ] 3.3 Build validation
    - Compile firmware for `esp32dev` via PlatformIO
    - No additional targets needed at this phase
  - [ ] 3.4 Ensure tests pass and build succeeds
    - Run ONLY tests from 3.1

Acceptance Criteria:
- 4 HELP/STATUS tests pass
- `pio run` succeeds for `esp32dev`
- Output formats match spec exactly

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
@agent-os/standards/frontend/embedded-web-ui.md
@agent-os/standards/frontend/serial-interface.md
@agent-os/standards/testing/build-validation.md
@agent-os/standards/testing/hardware-validation.md
@agent-os/standards/testing/unit-testing.md

