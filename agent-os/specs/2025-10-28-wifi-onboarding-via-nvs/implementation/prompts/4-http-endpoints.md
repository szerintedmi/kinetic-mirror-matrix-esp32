We're continuing our implementation of Wi‑Fi Onboarding via NVS by implementing task group number 4:

## Implement this task and its sub-tasks:

### Embedded Web UI

#### Task Group 4: HTTP endpoints

**Assigned implementer:** api-engineer
**Dependencies:** Task Group 1

- [ ] 4.0 Serve gzipped static files from LittleFS (`/index.html`, css, js)
- [ ] 4.1 Implement `GET /api/scan` (list SSIDs)
- [ ] 4.2 Implement `GET /api/status` (state/RSSI/IP)
- [ ] 4.3 Implement `POST /api/wifi` (save creds → connect attempt)
- [ ] 4.4 2–6 focused tests (manual with curl) for endpoints

Acceptance

- Endpoints respond as specified; POST triggers connect attempt

Manual test

- `curl http://192.168.4.1/api/scan` etc.; observe JSON and state change

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

Create: `agent-os/specs/2025-10-28-wifi-onboarding-via-nvs/implementation/4-http-endpoints-implementation.md`

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

