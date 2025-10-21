We're continuing our implementation of 16+ Motors per ESP32 (Shared STEP Spike) by implementing task group number 5:

## Implement this task and its sub-tasks:

#### Task Group 5: Host CLI/TUI Adjustments
**Assigned implementer:** ui-designer
**Dependencies:** Task Group 3

- [ ] 5.0 Update Python CLI/TUI to match protocol
  - [ ] 5.1 Write 2-4 focused tests for CLI command builders/parsing (if present)
  - [ ] 5.2 Remove speed/accel inputs from `MOVE`/`HOME` paths
  - [ ] 5.3 Update HELP and interactive hints to the new grammar
  - [ ] 5.4 Keep STATUS polling unchanged

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
- Use `python -m serial_cli` interactively to validate HELP text and command builders.

## Update tasks.md task status

Update this task group's checkboxes in `tasks.md` upon completion of sub-tasks you implemented.

## Document your implementation

Create: `agent-os/specs/2025-10-21-16-plus-motors-per-esp32-rd-spike/implementation/5-host-cli-tui-adjustments-implementation.md`

Structure: see template in implement-spec guide.

## User Standards & Preferences Compliance

@agent-os/standards/global/coding-style.md
@agent-os/standards/global/conventions.md
@agent-os/standards/global/platformio-project-setup.md
@agent-os/standards/global/resource-management.md
@agent-os/standards/frontend/embedded-web-ui.md
@agent-os/standards/frontend/serial-interface.md
@agent-os/standards/testing/unit-testing.md
@agent-os/standards/testing/hardware-validation.md
@agent-os/standards/testing/build-validation.md

