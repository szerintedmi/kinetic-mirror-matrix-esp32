We're continuing our implementation of Serial Command Protocol v1 by implementing task group number 2:

## Implement this task and its sub-tasks:

#### Task Group 2: Python Serial CLI
Assigned implementer: ui-designer
Dependencies: Task Group 1
Standards: `@agent-os/standards/frontend/serial-interface.md`, `@agent-os/standards/testing/build-validation.md`

- [ ] 2.0 Complete host CLI
  - [ ] 2.1 Write 2-8 focused tests for CLI argument→command mapping
    - Use a stub serial transport to avoid hardware dependency
    - Cover subcommands: help, status, wake, sleep, move (defaults), home (param order)
    - Limit to 2-8 tests (target 4)
    - Run ONLY these tests for this group
  - [ ] 2.2 Implement `tools/serial_cli.py`
    - Subcommands: `help`, `status`, `wake`, `sleep`, `move`, `home`
    - Map to exact grammar; omit optional params by default
  - [ ] 2.3 Provide example E2E script
    - `tools/e2e_demo.py` invoking typical flows; print responses
  - [ ] 2.4 Ensure CLI tests pass
    - Run ONLY tests from 2.1

Acceptance Criteria:
- 4 CLI mapping tests pass
- CLI prints responses and handles multi-line outputs cleanly
- Examples run against firmware serial port

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
@agent-os/standards/frontend/embedded-web-ui.md
@agent-os/standards/frontend/serial-interface.md
@agent-os/standards/testing/build-validation.md
@agent-os/standards/testing/unit-testing.md

