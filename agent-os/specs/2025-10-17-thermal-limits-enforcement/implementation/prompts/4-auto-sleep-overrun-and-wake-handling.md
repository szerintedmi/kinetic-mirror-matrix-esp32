We're continuing our implementation of Thermal Limits Enforcement by implementing task group number 4:

## Implement this task and its sub-tasks:

#### Task Group 4: Auto-sleep overrun and WAKE handling
**Assigned implementer:** api-engineer
**Dependencies:** Task Group 1

- [ ] 4.0 Add `AUTO_SLEEP_IF_OVER_BUDGET_S` constant to `MotorControlConstants`
- [ ] 4.1 In controllers’ `tick()`, when enabled and `budget_tenths < -AUTO_SLEEP_IF_OVER_BUDGET_S*10`, force SLEEP and cancel any active move (stub and hardware)
- [ ] 4.2 Reject `WAKE` with `CTRL:ERR E12 THERMAL_NO_BUDGET_WAKE` when no budget (or WARN when disabled)
- [ ] 4.3 Ensure move cancellation is safe and idempotent
- [ ] 4.4 Unit tests: auto-sleep after overrun cancels active plan (awake=0, moving=0); WAKE rejection/warn paths

**Acceptance Criteria:**
- Motors are force-slept after grace overrun; active moves stop
- WAKE rejected only when enabled and no budget; WARN observed when disabled

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

## Update tasks.md task status

Update this task group’s checkboxes in tasks.md for what you implemented.

## Document your implementation

Create `agent-os/specs/2025-10-17-thermal-limits-enforcement/implementation/4-auto-sleep-overrun-and-wake-handling-implementation.md` with code notes and tests.

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

