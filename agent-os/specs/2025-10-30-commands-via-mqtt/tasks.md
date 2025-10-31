# Task Breakdown: Commands via MQTT

## Overview
Total Tasks: 10
Assigned roles: api-engineer, ui-designer, testing-engineer

## Task List

### Firmware & Transport Layer

#### Task Group 1: MQTT Command Parity Core
**Assigned implementer:** api-engineer
**Dependencies:** None

- [x] 1.0 Establish shared command schema
  - [x] 1.1 Author `docs/mqtt-command-schema.md` outlining envelope, status enums, error catalog, params mapping (reuse serial taxonomy)
  - [x] 1.2 Relocate `net_onboarding::NextMsgId` into new shared `transport/message_id` utility with unit coverage (2-4 focused tests)
  - [x] 1.3 Update serial formatter to consume shared error/status catalog without altering line format
- [x] 1.4 Implement `mqtt::MqttCommandServer`
  - [x] 1.4.1 Subscribe to `devices/<id>/cmd` (QoS1), validate JSON payloads, enforce single-action requests
  - [x] 1.4.2 Integrate `MotorCommandProcessor::execute` and map `CommandResult` into normalized result structs
  - [x] 1.4.3 Publish ACK and completion responses on `devices/<id>/cmd/resp` (QoS1) with shared schema
  - [x] 1.4.4 Guard duplicates by tracking last N `cmd_id`s and logging rate-limited `CTRL:INFO MQTT_DUPLICATE cmd_id=...`
- [x] 1.5 Extend firmware tests (2-8 focused PlatformIO native cases) covering:
- [x] JSON validation failure (`status:"error"`)
  - [x] Successful MOVE command response mapping
  - [x] Duplicate `cmd_id` suppression
  - [x] BUSY rejection parity
- [x] 1.6 Run only new/modified native tests and target component build (`pio test -e native --filter mqtt_command*` + `pio run`)

**Acceptance Criteria:**
- Shared message ID helper live outside onboarding and passes unit tests
- Firmware publishes ACK & completion payloads matching documented schema
- Serial output reflects consolidated error/status codes without behavior regressions
- Duplicate commands skip execution and log `CTRL:INFO MQTT_DUPLICATE`
- Focused test suite (2-8 cases) passes locally

#### Task Group 2: MQTT Config Commands (Post-Parity)
**Assigned implementer:** api-engineer
**Dependencies:** Task Group 1

- [ ] 2.0 Implement configuration actions
  - [ ] 2.1 Define `MQTT:GET_CONFIG` / `MQTT:SET_CONFIG` spec additions in schema doc (append)
  - [ ] 2.2 Persist broker host/port/user/pass in Preferences namespace `mqtt` with version guard
  - [ ] 2.3 Expose commands over serial and MQTT (same envelope) with validation + defaults fallback
  - [ ] 2.4 Add 2-4 focused tests exercising set/get round trip and reboot persistence (native + if needed embedded mock)
- [ ] 2.5 Update developer docs (`docs/mqtt-command-schema.md`, README/Troubleshooting) with usage examples
- [ ] 2.6 Run only added tests from this group

**Acceptance Criteria:**
- Config commands return defaults when unset and persist overrides across reboot
- Serial and MQTT transports share identical payloads for config actions
- Documentation reflects new commands and preference storage flow
- Focused tests (≤4) pass locally

### Host Tooling & UX

#### Task Group 3: CLI/TUI MQTT Command Support
**Assigned implementer:** ui-designer
**Dependencies:** Task Group 1

- [ ] 3.0 Update MQTT worker
  - [ ] 3.1 Add 2-6 focused tests around new command client helpers (mock MQTT)
  - [ ] 3.2 Implement command publish helper that emits single-action messages (expand `;`-delimited input into sequential publishes)
  - [ ] 3.3 Track pending commands, surface ACK latency and BUSY/ERROR states in logs
  - [ ] 3.4 Respect duplicate notice (`CTRL:INFO MQTT_DUPLICATE`) without retrying
- [ ] 3.5 Integrate with TUI/CLI interfaces
  - [ ] 3.5.1 Wire command queue UI to display MQTT status updates (no auto serial fallback)
  - [ ] 3.5.2 Ensure telemetry polling cadence (1 Hz / 5 Hz) remains unaffected
- [ ] 3.6 Update CLI help text / documentation referencing new JSON schema and config commands
- [ ] 3.7 Run only the new focused Python tests (`pytest tools/serial_cli/tests/test_mqtt_commands.py -k new_cases`)

**Acceptance Criteria:**
- MQTT mode sends single-action commands and logs ACK/completion states analogous to serial mode
- UI surfaces command progress without blocking telemetry updates
- Tests (≤6) covering command publish + response handling pass
- Updated help/docs available to operators

### Validation

#### Task Group 4: Integrated Test & Bench Verification
**Assigned implementer:** testing-engineer
**Dependencies:** Task Groups 1-3

- [ ] 4.0 Review feature-specific tests from groups 1-3 (expect 6-16 total)
- [ ] 4.1 Identify remaining critical workflow gaps (e.g., MOVE over MQTT during active motion, config persistence after reboot)
- [ ] 4.2 Add up to 6 supplementary tests (host integration script or PlatformIO native) covering:
  - End-to-end MOVE/MOVE overlap resulting in BUSY
  - MQTT command vs. STATUS parity check
  - Config command persistence validation
- [ ] 4.3 Execute feature-focused suite only: union of group tests plus additions (document exact commands run)
- [ ] 4.4 Produce bench validation checklist referencing `@agent-os/standards/testing/hardware-validation.md` (e.g., single MOVE success, BUSY rejection, config change reboot)

**Acceptance Criteria:**
- Added tests (≤6) pass and cover remaining critical flows
- Bench checklist completed and attached under `specs/.../verification/`
- No full-suite runs; only feature-specific tests executed

## Execution Order
1. Task Group 1: MQTT Command Parity Core
2. Task Group 2: MQTT Config Commands (Post-Parity)
3. Task Group 3: CLI/TUI MQTT Command Support
4. Task Group 4: Integrated Test & Bench Verification
