We're continuing our implementation of ESP32 8‑Motor Bring‑Up (FastAccelStepper + 74HC595) by implementing task group number 3:

## Implement this task and its sub-tasks:

#### Task Group 3: Build validation & bench checklist
Assigned implementer: testing-engineer
Dependencies: Task Groups 1–2
Standards: `@agent-os/standards/testing/build-validation.md`, `@agent-os/standards/testing/hardware-validation.md`

- [ ] 3.0 Validate builds and run feature-specific tests only
  - [ ] 3.1 Build for `esp32dev` via PlatformIO
  - [ ] 3.2 Run ONLY unit tests from Task Groups 1–2
  - [ ] 3.3 Create bench checklist (manual)
    - Verify DIR/SLEEP via logic probe/LEDs while issuing MOVE/WAKE/SLEEP
    - Confirm concurrent MOVE on 2+ motors; ensure latch occurs before motion
    - Check auto-sleep after completion and STATUS awake=0

Acceptance Criteria:
- `pio run -e esp32dev` succeeds
- Unit tests from 1–2 pass; bench checklist documented and followed during bring-up
- Bench verifies: (a) 74HC595 outputs (DIR/SLEEP) reflect commands immediately after latch; (b) FastAccelStepper produces STEP pulses for MOVE on selected motors and auto-sleeps after completion

## Understand the context

Read @agent-os/specs/2025-10-16-esp32-8-motor-bring-up/spec.md to understand the context for this spec and where the current task fits into it.

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
@agent-os/standards/testing/build-validation.md
@agent-os/standards/testing/hardware-validation.md
@agent-os/standards/testing/unit-testing.md

