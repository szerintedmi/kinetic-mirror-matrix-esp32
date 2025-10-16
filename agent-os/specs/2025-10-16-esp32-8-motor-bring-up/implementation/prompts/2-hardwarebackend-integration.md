We're continuing our implementation of ESP32 8‑Motor Bring‑Up (FastAccelStepper + 74HC595) by implementing task group number 2:

## Implement this task and its sub-tasks:

#### Task Group 2: HardwareBackend integration (FastAccelStepper + 74HC595 API)
Assigned implementer: api-engineer
Dependencies: Task Group 1
Standards: `@agent-os/standards/backend/hardware-abstraction.md`, `@agent-os/standards/global/resource-management.md`, `@agent-os/standards/testing/unit-testing.md`

- [ ] 2.0 Implement HardwareBackend behind MotorBackend
  - [ ] 2.1 Write 2-8 focused unit tests (native/sim) for backend sequencing
    - Use a mocked 74HC595 driver to assert: latch-before-start, correct bits per target, and WAKE/SLEEP overrides
    - Verify busy rule on overlapping MOVE; DIR latched once per move
    - Limit to 2-8 tests (target 6)
  - [ ] 2.2 Configure FastAccelStepper for 8 motors (STEP pins)
    - Map STEP pins: `GPIO4,16,17,25,26,27,32,33`
    - Defaults: speed=4000, accel=16000 (override via protocol)
  - [ ] 2.3 Enforce sequencing per move
    - Compute/set DIR & SLEEP bits → driver.set_dir_sleep() → start steppers
  - [ ] 2.4 Map WAKE/SLEEP protocol verbs as overrides
    - Keep FastAccelStepper auto-sleep; WAKE/SLEEP forces SLEEP bit via driver
  - [ ] 2.5 Integrate backend selection
    - Provide compile-time switch to choose `StubBackend` vs `HardwareBackend`
  - [ ] 2.6 Ensure backend unit tests pass
    - Run ONLY tests from 2.1

Acceptance Criteria (independent of TG3):
- Backend unit tests pass with a mocked 74HC595 driver (no hardware required)
- Busy rule and latch-before-start validated via mocks
- For MOVE requests, tests verify the backend issues FastAccelStepper start calls for targeted motors with the correct speed/accel (mocked FAS adapter), and on bench we observe STEP pulses generated on the configured STEP pins (validated in TG3 checklist)

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

