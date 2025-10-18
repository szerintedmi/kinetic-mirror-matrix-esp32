We're continuing our implementation of Thermal Limits Enforcement by implementing task group number 2:

## Implement this task and its sub-tasks:

#### Task Group 2: Shared estimator helper and integration
**Assigned implementer:** api-engineer
**Dependencies:** Task Group 1

- [ ] 2.0 Add `lib/MotorControl/include/MotorControl/MotionKinematics.h` and `.cpp`
- [ ] 2.1 Implement `estimateMoveTimeMs(distance_steps, speed_sps, accel_sps2)` with triangular/trapezoidal profile; integer math with ceil
- [ ] 2.2 Replace stub duration calc with estimator in `StubMotorController`
- [ ] 2.3 Provide HOME leg-by-leg estimate utility for pre-checks
- [ ] 2.4 Unit tests: estimator conservative vs simple bounds; stub schedule uses estimator (durations match within ceil rounding)

**Acceptance Criteria:**
- Estimator compiles and returns conservative time for both short (triangular) and long (trapezoidal) moves
- Stub plans’ end times align with estimator outputs

## Understand the context

Read @agent-os/specs/2025-10-17-thermal-limits-enforcement/spec.md to understand the context for this spec and where the current task fits into it.

## Perform the implementation

Implement all tasks assigned to you in your task group.

Focus ONLY on implementing the areas that align with **areas of specialization** (your "areas of specialization" are defined above).

Guide your implementation using:
- **The existing patterns** that you've found and analyzed.
- **User Standards & Preferences** which are defined below.

Self-verify and test your work by:
- Running ONLY the tests you've written (if any) and ensuring those tests pass.
- IF your task involves user-facing UI, and IF you have access to browser testing tools, open a browser and use the feature you've implemented as if you are a user to ensure a user can use the feature in the intended way.


## Update tasks.md task status

In the current spec's `tasks.md` find YOUR task group that's been assigned to YOU and update this task group's parent task and sub-task(s) checked statuses to complete for the specific task(s) that you've implemented.

Mark your task group's parent task and sub-task as complete by changing its checkbox to `- [x]`.

DO NOT update task checkboxes for other task groups that were NOT assigned to you for implementation.


## Document your implementation

Create `agent-os/specs/2025-10-17-thermal-limits-enforcement/implementation/2-shared-estimator-helper-and-integration-implementation.md` and document what you changed, tests added, and rationale.

## User Standards & Preferences Compliance

@agent-os/standards/global/coding-style.md
@agent-os/standards/global/conventions.md
@agent-os/standards/global/platformio-project-setup.md
@agent-os/standards/global/resource-management.md
@agent-os/standards/backend/data-persistence.md
@agent-os/standards/backend/hardware-abstraction.md
@agent-os/standards/backend/task-structure.md
@agent-os/standards/testing/unit-testing.md
@agent-os/standards/testing/hardware-validation.md
@agent-os/standards/testing/build-validation.md

