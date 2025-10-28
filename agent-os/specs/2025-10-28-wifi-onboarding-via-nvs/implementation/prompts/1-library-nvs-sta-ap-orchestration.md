We're continuing our implementation of Wi‑Fi Onboarding via NVS by implementing task group number 1:

## Implement this task and its sub-tasks:

### Connectivity Core (merged)

#### Task Group 1: Library + NVS + STA/AP orchestration

**Assigned implementer:** api-engineer
**Dependencies:** None

- [ ] 1.0 Create library skeleton `lib/net_onboarding/`
  - Public header: `include/net_onboarding/NetOnboarding.h`
  - Source: `src/NetOnboarding.cpp`
  - API: `begin()`, `loop()`, `setCredentials(ssid,pass)`, `resetCredentials()`, `status()`; enums for `AP_ACTIVE|CONNECTING|CONNECTED`
- [ ] 1.1 NVS persistence using `Preferences`
  - Namespace `net`; keys `ssid`, `psk`
  - Functions: `saveCredentials`, `loadCredentials`, `clearCredentials`
- [ ] 1.2 STA connect with configurable timeout; fallback/hand‑over to SoftAP
  - On timeout → start AP; on success → stop AP
  - `status()` exposes state, RSSI, IP (when connected)
- [ ] 1.3 SoftAP identity & password
  - SSID `SOFT_AP_SSID_PREFIX + last-3-bytes(MAC)`; PSK from `SOFT_AP_PASS`
  - Verify DHCP defaults
- [ ] 1.4 Tests (focused)
  - 2–4 native tests for basic state transitions and status enum
  - 2–4 on‑device checks for timeout → AP and happy‑path connect
- [ ] 1.5 Motor timing isolation checks
  - Script a simple motion loop; compare baseline vs AP active vs CONNECTED steady‑state
  - Document acceptable jitter during CONNECTING

Acceptance

- Cold boot with saved creds connects within timeout; else AP starts and portal IP reachable
- NVS save/load/clear works across reboot
- No observable motor stutter in AP/CONNECTED steady‑state vs baseline

Manual tests

- AP path: Ensure no creds in NVS → boot → verify AP SSID format and joinable; reach `192.168.4.1`
- STA path (before `NET:SET` exists): Pre‑seed NVS credentials using one of:
  1) Temporary one‑off "seed" sketch that calls `saveCredentials(TEST_STA_SSID, TEST_STA_PASS)` from `include/secrets.h`, then reflash normal firmware
  2) A small temporary seeding function behind `#ifdef SEED_NVS` compiled once
  3) A standalone minimal tool that writes `ssid`/`psk` via `Preferences` (Arduino sketch) and reboots
- After seeding, reboot → verify CONNECTED state, RSSI, and IP reported by `status()`; run motor loop and observe isolation

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

In the current spec's `tasks.md` find YOUR task group that's been assigned to YOU and update this task group's parent task and sub-task(s) checked statuses to complete for the specific task(s) that you've implemented.

## Document your implementation

Using the task number and task title that's been assigned to you, create a file in the current spec's `implementation` folder called `1-library-nvs-sta-ap-orchestration-implementation.md`.

Use the structure provided in implement-spec to document code locations, rationale, tests, and compliance.

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

