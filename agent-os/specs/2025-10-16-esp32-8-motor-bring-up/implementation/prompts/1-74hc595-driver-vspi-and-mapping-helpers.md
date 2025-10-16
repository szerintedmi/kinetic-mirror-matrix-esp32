We're continuing our implementation of ESP32 8‑Motor Bring‑Up (FastAccelStepper + 74HC595) by implementing task group number 1:

## Implement this task and its sub-tasks:

#### Task Group 1: 74HC595 driver (VSPI) + mapping helpers
Assigned implementer: api-engineer
Dependencies: None
Standards: `@agent-os/standards/backend/hardware-abstraction.md`, `@agent-os/standards/testing/unit-testing.md`

- [ ] 1.0 Complete SPI driver & helpers (no FastAccelStepper dependency)
  - [ ] 1.1 Write 2-8 focused unit tests (native/sim)
    - Verify bit packing: DIR in byte0, SLEEP in byte1; SLEEP polarity: 1=awake/high
    - Verify latch sequence: shift → latch; no extra toggles
    - Limit to 2-8 tests (target 4)
  - [ ] 1.2 Implement low-level VSPI driver
    - Init VSPI (`SCK=GPIO18`, `MOSI=GPIO23`, `MISO=GPIO19` unused), `RCLK=GPIO5`
    - Provide `set_dir_sleep(dir_bits, sleep_bits)` API
  - [ ] 1.3 Provide mapping helpers
    - `compute_dir_bits(target_targets)` and `compute_sleep_bits(target_targets, awake)`
  - [ ] 1.4 Ensure tests pass

Acceptance Criteria (independent):
- Given target selections and WAKE/SLEEP overrides, unit tests verify `compute_dir_bits` and `compute_sleep_bits` produce expected bitmasks for motors 0–7
- With a stubbed SPI interface, tests capture and assert that the exact two bytes shifted/latch sequence matches the computed DIR (byte0) and SLEEP (byte1) bits (i.e., 74HC595 outputs reflect intended command effects)
- API is usable standalone (no dependency on motion backend)

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
@agent-os/standards/backend/data-persistence.md
@agent-os/standards/backend/hardware-abstraction.md
@agent-os/standards/backend/task-structure.md
@agent-os/standards/testing/build-validation.md
@agent-os/standards/testing/hardware-validation.md
@agent-os/standards/testing/unit-testing.md

