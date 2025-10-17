We're continuing our implementation of Homing & Baseline Calibration (Bump‑Stop) by implementing task group number 2:

## Implement this task and its sub-tasks:

#### Task Group 2: Build validation and homing bench checks
Assigned implementer: testing-engineer
Dependencies: Task Group 1
Standards: `@agent-os/standards/testing/build-validation.md`, `@agent-os/standards/testing/hardware-validation.md`

- [ ] 2.0 Validate build and execute homing bench checklist
  - [ ] 2.1 Build firmware for `esp32dev` via PlatformIO
  - [ ] 2.2 Bench checklist (manual)
    - Issue `HOME:<id>`: observe negative run to hard stop, backoff to negative soft boundary, center move; final logical position reads 0 in STATUS
    - Issue `HOME:ALL`: confirm concurrent motion across 8 motors without brownouts; STEP pulses and DIR/SLEEP latching look stable
    - Verify auto‑WAKE before motion and auto‑SLEEP after completion; respect manual `WAKE` (stay awake) and `SLEEP` (error if moving)
    - Confirm ignoring soft limits during HOME does not affect MOVE’s soft limits after homing

Acceptance Criteria
- `pio run -e esp32dev` completes successfully
- Bench checklist executed and documented; observed behavior matches spec (sequence, concurrency, WAKE/SLEEP parity, final zero)

## Execution Order
1. Firmware Implementation (Task Group 1)
2. Build & Bench Validation (Task Group 2)

## Understand the context

Read @agent-os/specs/2025-10-17-homing-baseline-calibration-bump-stop/spec.md to understand the context for this spec and where the current task fits into it.

## Perform the implementation

Implement all tasks assigned to you in your task group.

Focus ONLY on implementing the areas that align with areas of specialization (your "areas of specialization" are defined above).

Guide your implementation using:
- The existing patterns that you've found and analyzed.
- User Standards & Preferences which are defined below.

Self-verify and test your work by:
- Running ONLY the tests you've written (if any) and ensuring those tests pass.
- Execute the manual bench checklist as described.

## Update tasks.md task status

In the current spec's `tasks.md` find YOUR task group that's been assigned to YOU and update this task group's parent task and sub-task(s) checked statuses to complete for the specific task(s) that you've implemented.

Mark your task group's parent task and sub-task as complete by changing its checkbox to `- [x]`.

DO NOT update task checkboxes for other task groups that were NOT assigned to you for implementation.

## Document your implementation

Using the task number and task title that's been assigned to you, create a file in the current spec's `implementation` folder called `[task-number]-[task-title]-implementation.md`.

Use the following structure for the content of your implementation documentation:

```
## Summary
[Short summary of what you validated]

## Build Outputs
- PlatformIO envs built, versions

## Bench Checklist Results
- Observations for each checklist bullet
- Any anomalies

## Testing
- Feature-specific tests run
- Results summary

## User Standards & Preferences Compliance
[List specific standards followed and how]

## Known Issues & Limitations
[List any]
```

## User Standards & Preferences Compliance

@agent-os/standards/global/coding-style.md
@agent-os/standards/global/conventions.md
@agent-os/standards/global/platformio-project-setup.md
@agent-os/standards/global/resource-management.md
@agent-os/standards/testing/build-validation.md
@agent-os/standards/testing/hardware-validation.md
@agent-os/standards/testing/unit-testing.md
