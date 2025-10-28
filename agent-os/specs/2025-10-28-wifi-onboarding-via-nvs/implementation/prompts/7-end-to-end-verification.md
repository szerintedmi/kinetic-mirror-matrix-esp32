We're continuing our implementation of Wi‑Fi Onboarding via NVS by implementing task group number 7:

## Implement this task and its sub-tasks:

### Steel Thread / E2E

#### Task Group 7: End‑to‑end verification

**Assigned implementer:** testing-engineer
**Dependencies:** Task Groups 1–6

- [ ] 7.0 Cold boot (no creds) → AP; portal sets creds → CONNECTED
- [ ] 7.1 `NET:STATUS` reports CONNECTED with IP/RSSI
- [ ] 7.2 Long‑press reset restores AP flow
- [ ] 7.3 Update README with onboarding guide (SoftAP and Serial alternatives) and LED patterns

Acceptance

- All steps succeed without code changes; logs/LEDs match spec; README updated

Manual test

- Follow 7.0–7.2; record outcomes under `planning/verification/`

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

Create: `agent-os/specs/2025-10-28-wifi-onboarding-via-nvs/implementation/7-end-to-end-verification-implementation.md`

## User Standards & Preferences Compliance

@agent-os/standards/global/coding-style.md
@agent-os/standards/global/conventions.md
@agent-os/standards/global/platformio-project-setup.md
@agent-os/standards/global/resource-management.md
@agent-os/standards/testing/build-validation.md
@agent-os/standards/testing/hardware-validation.md
@agent-os/standards/testing/unit-testing.md

