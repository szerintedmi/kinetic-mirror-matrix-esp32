We're continuing our implementation of Thermal Limits Enforcement by implementing task group number 1:

## Implement this task and its sub-tasks:

#### Task Group 1: Global flag, HELP, GET/SET
**Assigned implementer:** api-engineer
**Dependencies:** None

- [ ] 1.0 Add global `thermal_limits_enabled` (default ON)
- [ ] 1.1 Parse `SET THERMAL_RUNTIME_LIMITING=OFF|ON` in `MotorCommandProcessor`
- [ ] 1.2 Update `HELP` to include both `SET THERMAL_RUNTIME_LIMITING=OFF|ON` and `GET THERMAL_RUNTIME_LIMITING`
- [ ] 1.3 Implement `GET THERMAL_RUNTIME_LIMITING` → `CTRL:OK THERMAL_RUNTIME_LIMITING=ON|OFF max_budget_s=<N>`
- [ ] 1.4 Keep existing per-motor STATUS output unchanged (no meta lines)
- [ ] 1.5 Unit tests: HELP lists GET/SET; GET returns expected payload

**Acceptance Criteria:**
- `HELP` lists `SET THERMAL_RUNTIME_LIMITING=OFF|ON` and `GET THERMAL_RUNTIME_LIMITING`
- STATUS per-motor lines unchanged; no trailing meta lines
- `GET THERMAL_RUNTIME_LIMITING` returns `ON` by default and correct `max_budget_s`

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

Using the task number and task title that's been assigned to you, create a file in the current spec's `implementation` folder called `1-global-flag-help-get-set-implementation.md`.

Use the following structure for the content of your implementation documentation:

```markdown
## Implementation Summary

### Components Changed
**Location:** [list files]

[Details, rationale]

## Testing
[What tests were added/updated and results]

## Standards Compliance
[Brief notes mapping to standards below]
```

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

