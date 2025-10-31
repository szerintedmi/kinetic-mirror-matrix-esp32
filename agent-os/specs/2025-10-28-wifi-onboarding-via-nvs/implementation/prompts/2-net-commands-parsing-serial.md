We're continuing our implementation of Wi‑Fi Onboarding via NVS by implementing task group number 2:

## Implement this task and its sub-tasks:

### Serial Surface

#### Task Group 2: NET commands parsing (Serial)

**Assigned implementer:** api-engineer
**Dependencies:** Task Group 1

- [ ] 2.0 Extend `MotorCommandProcessor` HELP with NET actions
- [ ] 2.1 Implement `NET:RESET` (clear creds + reboot into AP)
- [ ] 2.2 Implement `NET:STATUS` (state/RSSI/IP)
- [ ] 2.3 Implement `NET:SET,<ssid>,<pass>` with quoted fields and validation
- [ ] 2.4 Suspend `NET:*` while connecting → `CTRL:ERR NET_BUSY_CONNECTING`
- [ ] 2.5 2–6 focused tests (native) for parsing/formatting and busy path

Acceptance

- HELP output lists NET actions (`NET:RESET`, `NET:STATUS`, `NET:SET`) with payload syntax (including quoting rules) and documents busy error `NET_BUSY_CONNECTING`.
- Manual serial session exercises all NET actions; responses match spec

Manual test

- Use `tools/serial_cli` to send HELP/NET:* and verify responses

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

Create: `agent-os/specs/2025-10-28-wifi-onboarding-via-nvs/implementation/2-net-commands-parsing-serial-implementation.md`

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

