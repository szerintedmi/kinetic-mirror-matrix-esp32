We're continuing our implementation of Status & Diagnostics by implementing task group number 4:

## Implement this task and its sub-tasks:

### Testing

#### Task Group 4: Test Review & Gap Analysis
Assigned implementer: testing-engineer
Dependencies: Task Groups 1–3
Standards: `@agent-os/standards/testing/unit-testing.md`, `@agent-os/standards/testing/hardware-validation.md`, `@agent-os/standards/global/coding-style.md`

- [ ] 4.0 Review feature tests and fill critical gaps only
  - [ ] 4.1 Review tests from Task Groups 1–3
    - Unity tests from 1.1 and formatting tests from 3.1
    - Python CLI tests from 2.1
  - [ ] 4.2 Analyze coverage gaps specific to STATUS extensions and TUI
    - Focus on boundary conditions: negative budget_s visibility (no clamp at 0), ttfc_s calc, homed + steps_since_home reset behavior
  - [ ] 4.3 Write up to 8 additional strategic tests max
    - Prioritize end‑to‑end flows through parser/handlers and CLI parsing/rendering helpers
  - [ ] 4.4 Run feature‑specific tests only
    - Run ONLY tests from 1.1, 2.1, 3.1, and 4.3 (expected total ≤ 20)

## Understand the context

Read @agent-os/specs/2025-10-17-status-diagnostics/spec.md to understand the context for this spec and where the current task fits into it.

## Perform the implementation

Implement all tasks assigned to you in your task group.

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

## User Standards & Preferences Compliance

IMPORTANT: Ensure that your implementation work is ALIGNED and DOES NOT CONFLICT with the user's preferences and standards as detailed in the following files:

@agent-os/standards/testing/unit-testing.md
@agent-os/standards/testing/hardware-validation.md
@agent-os/standards/global/coding-style.md
@agent-os/standards/global/conventions.md
