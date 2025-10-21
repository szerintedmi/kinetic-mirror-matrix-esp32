We're continuing our implementation of 16+ Motors per ESP32 (Shared STEP Spike) by implementing task group number 1:

## Implement this task and its sub-tasks:

#### Task Group 1: Pulse Generator Spike (RMT)
**Assigned implementer:** api-engineer
**Dependencies:** None

- [ ] 1.0 Prototype hardware-timed STEP generator
  - [ ] 1.1 Write 2-6 focused unit tests for timing helpers (period calc, start/stop gating points)
  - [ ] 1.2 Implement minimal RMT-based pulse generator at global `SPEED` (steps/s)
  - [ ] 1.3 Expose start/stop and edge-aligned callback hooks
  - [ ] 1.4 Add compile flag to select SharedSTEP vs FastAccelStepper paths
  - [ ] 1.5 Ensure tests in 1.1 pass (native/host if possible)

## Understand the context

Read @agent-os/specs/2025-10-21-16-plus-motors-per-esp32-rd-spike/spec.md to understand the context for this spec and where the current task fits into it.

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

Using the task number and task title that's been assigned to you, create a file in the current spec's `implementation` folder called `[task-number]-[task-title]-implementation.md`.

For example, if you've been assigned implement the 3rd task from `tasks.md` and that task's title is "Commenting System", then you must create the file: `agent-os/specs/2025-10-21-16-plus-motors-per-esp32-rd-spike/implementation/3-commenting-system-implementation.md`.

Use the following structure for the content of your implementation documentation:

```markdown
## Summary
- What was implemented in this task group
- Key design decisions and trade-offs

## Files Changed
- path/to/file1: what changed
- path/to/file2: what changed

## Implementation Details
### Component/Feature 1
**Location:** `path/to/file.ext`

[Detailed explanation of this implementation aspect]

**Rationale:** [Why this approach was chosen]

### Component/Feature 2
**Location:** `path/to/file.ext`

[Detailed explanation of this implementation aspect]

**Rationale:** [Why this approach was chosen]

## Testing
### Test Files Created/Updated
- `path/to/test/file` - [What is being tested]

### Manual Testing Performed
- Steps and results

## User Standards & Preferences Compliance
- Reference specific standards below and note compliance
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

