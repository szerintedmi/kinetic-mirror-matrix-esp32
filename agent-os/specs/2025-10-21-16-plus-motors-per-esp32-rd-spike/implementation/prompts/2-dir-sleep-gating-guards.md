We're continuing our implementation of 16+ Motors per ESP32 (Shared STEP Spike) by implementing task group number 2:

## Implement this task and its sub-tasks:

#### Task Group 2: DIR/SLEEP Gating & Guards
**Assigned implementer:** api-engineer
**Dependencies:** Task Group 1

- [ ] 2.0 Implement per-motor SLEEP gating + DIR guard windows
  - [ ] 2.1 Write 2-6 unit tests for guard timing logic and “no-STEP during DIR flip” rules
  - [ ] 2.2 Define constants `DIR_GUARD_US_[PRE,POST]` (microseconds) per spec
  - [ ] 2.3 Integrate 74HC595 latch updates so SLEEP/DIR changes avoid STEP edges
  - [ ] 2.4 Align re-enable timing to next safe STEP gap
  - [ ] 2.5 Ensure tests in 2.1 pass

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

## Update tasks.md task status

In the current spec's `tasks.md` find YOUR task group that's been assigned to YOU and update this task group's parent task and sub-task(s) checked statuses to complete for the specific task(s) that you've implemented.

Mark your task group's parent task and sub-task as complete by changing its checkbox to `- [x]`.

DO NOT update task checkboxes for other task groups that were NOT assigned to you for implementation.

## Document your implementation

Create: `agent-os/specs/2025-10-21-16-plus-motors-per-esp32-rd-spike/implementation/2-dir-sleep-gating-guards-implementation.md`

Structure:

```markdown
## Summary
## Files Changed
## Implementation Details
## Testing
## User Standards & Preferences Compliance
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

