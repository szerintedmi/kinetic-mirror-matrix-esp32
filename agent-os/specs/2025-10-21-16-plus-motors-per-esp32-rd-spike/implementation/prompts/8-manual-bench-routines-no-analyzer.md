We're continuing our implementation of 16+ Motors per ESP32 (Shared STEP Spike) by implementing task group number 8:

## Implement this task and its sub-tasks:

#### Task Group 8: Manual Bench Routines (No Analyzer)
**Assigned implementer:** testing-engineer
**Dependencies:** Task Groups 3, 6

- [ ] 7.0 Create deterministic bench scripts and device routines
  - [ ] 7.1 Add a Python CLI script to run patterned back-and-forth sequences (varying distances/rhythms)
  - [ ] 7.2 Add a device-side routine to report final MCU-tracked positions after N cycles
  - [ ] 7.3 Lab checklist for comparing physical vs tracked positions

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
- Execute the CLI script and compare MCU-tracked vs physical positions per the checklist.

## Update tasks.md task status

Update this task group's checkboxes in `tasks.md` upon completion of sub-tasks you implemented.

## Document your implementation

Create: `agent-os/specs/2025-10-21-16-plus-motors-per-esp32-rd-spike/implementation/8-bench-routines-implementation.md`

Structure: see template in implement-spec guide.

## User Standards & Preferences Compliance

@agent-os/standards/global/coding-style.md
@agent-os/standards/global/conventions.md
@agent-os/standards/global/platformio-project-setup.md
@agent-os/standards/global/resource-management.md
@agent-os/standards/testing/unit-testing.md
@agent-os/standards/testing/hardware-validation.md
@agent-os/standards/testing/build-validation.md

