We're continuing our implementation of Thermal Limits Enforcement by implementing task group number 3:

## Implement this task and its sub-tasks:

#### Task Group 3: MOVE/HOME budget pre-checks and messaging
**Assigned implementer:** api-engineer
**Dependencies:** Task Groups 1–2

- [ ] 3.0 In `MotorCommandProcessor`, before invoking controller, compute per‑motor required runtime using estimator
- [ ] 3.1 Compare to max budget (`MAX_RUNNING_TIME_S`) → reject with `CTRL:ERR E10 ...` (or `CTRL:WARN` when limits disabled)
- [ ] 3.2 Compare to current budget (`budget_s`, `ttfc_s`) after `tick(now_ms)` → reject with `CTRL:ERR E11 ...` (or WARN)
- [ ] 3.3 Ensure WARN behavior when limits disabled: emit `CTRL:WARN ...` lines then `CTRL:OK`
- [ ] 3.4 Unit tests for E10/E11 errors and WARN variants

**Acceptance Criteria:**
- Deterministic rejections for MOVE/HOME with proper payload fields
- When disabled, warnings are emitted and command succeeds with `CTRL:OK`

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

Update this task group’s checkboxes in tasks.md for what you implemented.

## Document your implementation

Create `agent-os/specs/2025-10-17-thermal-limits-enforcement/implementation/3-move-home-budget-pre-checks-and-messaging-implementation.md` with details of code and tests.

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

