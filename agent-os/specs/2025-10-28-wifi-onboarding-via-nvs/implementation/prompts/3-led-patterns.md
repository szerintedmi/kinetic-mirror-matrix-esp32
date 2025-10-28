We're continuing our implementation of Wi‑Fi Onboarding via NVS by implementing task group number 3:

## Implement this task and its sub-tasks:

### Feedback & Hardware Signals

#### Task Group 3: LED patterns

**Assigned implementer:** api-engineer
**Dependencies:** Task Group 1

- [ ] 3.0 Define LED pin/polarity in `include/boards/Esp32Dev.hpp` (GPIO 2, active‑low)
- [ ] 3.1 Implement non‑blocking blinker (fast=AP, slow=connecting, solid=connected)
- [ ] 3.2 2–4 focused tests (timing windows) or visual confirmation script

Acceptance

- Patterns match states; does not block main loop

Manual test

- Observe LED through each state; verify blink rates

## Understand the context

Read @agent-os/specs/2025-10-28-wifi-onboarding-via-nvs/spec.md to understand the context for this spec and where the current task fits into it.

## Perform the implementation

Implement all tasks assigned to you in your task group.

Focus ONLY on implementing the areas that align with areas of specialization.

Guide your implementation using:
- The existing patterns that you've found and analyzed.
- User Standards & Preferences which are defined below.

Self-verify and test your work by:
- Running ONLY the tests you've written (if any) and ensuring those tests pass.

## Update tasks.md task status

In the current spec's `tasks.md` find YOUR task group and update completion checkboxes for the tasks you implement.

## Document your implementation

Create: `agent-os/specs/2025-10-28-wifi-onboarding-via-nvs/implementation/3-led-patterns-implementation.md`

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

