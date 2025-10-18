We're continuing our implementation of Thermal Limits Enforcement by implementing task group number 5:

## Implement this task and its sub-tasks:

#### Task Group 5: CLI display and warning surfacing
**Assigned implementer:** ui-designer
**Dependencies:** Task Group 1

- [ ] 5.0 Show `thermal_limits` global flag (header/footer) using `GET THERMAL_RUNTIME_LIMITING`
- [ ] 5.1 Surface any `CTRL:WARN ...` lines alongside `CTRL:OK` in logs/output
- [ ] 5.2 Keep existing STATUS table columns unchanged
- [ ] 5.3 On startup and after toggles, issue `GET THERMAL_RUNTIME_LIMITING` and render flag
- [ ] 5.4 Tests: verify WARN lines are captured and rendered without treating as failure

**Acceptance Criteria:**
- CLI shows thermal flag state
- WARN lines appear in output; commands remain successful

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
- If you have a TUI, verify flag display and WARN surfacing interactively.

## Update tasks.md task status

Update this task group’s checkboxes in tasks.md for what you implemented.

## Document your implementation

Create `agent-os/specs/2025-10-17-thermal-limits-enforcement/implementation/5-cli-display-and-warning-surfacing-implementation.md` with code notes and tests.

## User Standards & Preferences Compliance

@agent-os/standards/global/coding-style.md
@agent-os/standards/global/conventions.md
@agent-os/standards/global/platformio-project-setup.md
@agent-os/standards/global/resource-management.md
@agent-os/standards/frontend/serial-interface.md
@agent-os/standards/frontend/embedded-web-ui.md
@agent-os/standards/testing/unit-testing.md
@agent-os/standards/testing/hardware-validation.md
@agent-os/standards/testing/build-validation.md

