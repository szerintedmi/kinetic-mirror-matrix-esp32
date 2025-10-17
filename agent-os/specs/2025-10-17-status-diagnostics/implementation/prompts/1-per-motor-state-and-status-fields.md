We're continuing our implementation of Status & Diagnostics by implementing task group number 1:

## Implement this task and its sub-tasks:

### Firmware Telemetry & STATUS Extensions

#### Task Group 1: Per‑Motor State + STATUS fields
Assigned implementer: api-engineer
Dependencies: None
Standards: `@agent-os/standards/frontend/serial-interface.md`, `@agent-os/standards/backend/task-structure.md`, `@agent-os/standards/global/resource-management.md`, `@agent-os/standards/testing/unit-testing.md`

- [ ] 1.0 Complete firmware telemetry additions and STATUS output
  - [ ] 1.1 Write 2–8 focused unit tests (Unity)
    - Validate STATUS includes new keys: `homed`, `steps_since_home`, `budget_s`, `ttfc_s`
    - Validate basic invariants under stubbed time: budget starts at 90, decrements while ON, clamps at 0, and ttfc_s ≥ 0
    - Target 6 tests; run ONLY these tests for this group
  - [ ] 1.2 Extend MotorState and controllers
    - Add: `homed` (bool/bit), `steps_since_home` (int32), `budget_balance_s` (fixed‑point or int), `last_update_ms` (uint32)
    - Initialize sane defaults on boot; reset on reboot
  - [ ] 1.3 Event‑driven budget bookkeeping
    - Update budget on MOVE/HOME/WAKE/SLEEP and before responding to STATUS
    - Spend 1.0 s/s while ON; refill 1.5 s/s while OFF up to max 90 s
    - Avoid floats; implement fixed‑point tenths (Q9.1) or integer math
  - [ ] 1.4 Steps‑since‑home tracking
    - Reset on successful HOME completion
    - Accumulate absolute step deltas as position changes
  - [ ] 1.5 Homed flag behavior
    - Set to 1 on successful HOME; cleared on reboot; MOVE does not change this flag
  - [ ] 1.6 STATUS formatting
    - Reorder keys for readability if needed; include `homed`, `steps_since_home`, `budget_s` (1 decimal), `ttfc_s` (1 decimal)
    - Use integer/fixed‑point formatting; no heavy allocations in loop
  - [ ] 1.7 Ensure firmware unit tests pass and build succeeds
    - Run ONLY tests from 1.1
    - Validate `pio run` for `esp32dev`

## Understand the context

Read @agent-os/specs/2025-10-17-status-diagnostics/spec.md to understand the context for this spec and where the current task fits into it.

## Perform the implementation

Implement all tasks assigned to you in your task group.

Focus ONLY on implementing the areas that align with areas of specialization (your "areas of specialization" are defined above).

Guide your implementation using:
- The existing patterns that you've found and analyzed.
- User Standards & Preferences which are defined below.

Self-verify and test your work by:
- Running ONLY the tests you've written (if any) and ensuring those tests pass.
- IF your task involves user-facing UI, and IF you have access to browser testing tools, open a browser and use the feature you've implemented as if you are a user to ensure a user can use the feature in the intended way.

## Update tasks.md task status

In the current spec's `tasks.md` find YOUR task group that's been assigned to YOU and update this task group's parent task and sub-task(s) checked statuses to complete for the specific task(s) that you've implemented.

Mark your task group's parent task and sub-task as complete by changing its checkbox to `- [x]`.

DO NOT update task checkboxes for other task groups that were NOT assigned to you for implementation.

## Document your implementation

Using the task number and task title that's been assigned to you, create a file in the current spec's `implementation` folder called `[task-number]-[task-title]-implementation.md`.

For example, if you've been assigned implement the 3rd task from `tasks.md` and that task's title is "Commenting System", then you must create the file: `agent-os/specs/2025-10-17-status-diagnostics/implementation/3-commenting-system-implementation.md`.

Use the following structure for the content of your implementation documentation:

```markdown
## Summary
- What you implemented
- Where in the codebase

## Changes
### [Component/Feature 1]
**Location:** `path/to/file.ext`

[Detailed explanation]

**Rationale:** [Why]

### [Component/Feature 2]
**Location:** `path/to/file.ext`

[Detailed explanation]

**Rationale:** [Why]

## Testing
- New/updated test files
- What they validate
- How to run just these tests

## Standards Compliance
- Reference standards and how you adhered to them
```

## User Standards & Preferences Compliance

IMPORTANT: Ensure that your implementation work is ALIGNED and DOES NOT CONFLICT with the user's preferences and standards as detailed in the following files:

@agent-os/standards/global/coding-style.md
@agent-os/standards/global/conventions.md
@agent-os/standards/global/platformio-project-setup.md
@agent-os/standards/global/resource-management.md
@agent-os/standards/backend/data-persistence.md
@agent-os/standards/backend/hardware-abstraction.md
@agent-os/standards/backend/task-structure.md
@agent-os/standards/testing/unit-testing.md

