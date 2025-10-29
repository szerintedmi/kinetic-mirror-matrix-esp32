# Task Breakdown: MQTT Steel Thread: Presence

## Overview
Total Tasks: 11
Assigned roles: api-engineer, ui-designer, testing-engineer

## Task List

### Firmware MQTT Presence
**Assigned implementer:** api-engineer
**Dependencies:** None

- [ ] 1.0 Implement `MqttPresenceClient` that wraps AsyncMqttClient, connects with broker defaults from `include/secrets.h`, and registers retained birth/LWT payloads on `devices/<mac>/state`.
- [ ] 1.1 Derive lowercase MAC topic IDs and build payloads `state=ready ip=<ipv4> msg_id=<uuid>` by pulling identity data from NetOnboarding; ensure LWT publishes `state=offline`.
- [ ] 1.2 Emit presence updates at ~1 Hz and immediately on motion/power state changes without blocking motor tasks; keep firmware operational when broker unreachable and log a single `CTRL:WARN MQTT_CONNECT_FAILED` event.
- [ ] 1.3 Integrate UUID generation (UUIDv4) using a hardware-seeded entropy source, expose it through CommandExecutionContext, and tag every MQTT publication with `msg_id`.
- [ ] 1.4 After presence publishing is validated, migrate serial command acknowledgments to reuse the new UUIDs instead of monotonic CIDs and update related formatting/helpers.
- [ ] 1.5 Update firmware docs/README notes to describe MQTT presence expectations and capture the follow-up task for runtime SET/GET of broker credentials.
- [ ] 1.6 Write 2-4 focused unit tests (native/host where possible) covering payload formatting, UUID propagation, and the broker-failure log path; run only these new tests.

**Acceptance Criteria:**
- Flash firmware, connect to local Mosquitto with defaults, and confirm retained `state=ready ip=<ipv4> msg_id=<uuid>` appears on `devices/<mac>/state`; power-cycle node to see retained `state=offline` and single `CTRL:WARN MQTT_CONNECT_FAILED` when broker is unreachable.
- Run the 2-4 new unit tests and verify they pass.

### CLI & TUI MQTT Transport
**Assigned implementer:** ui-designer
**Dependencies:** Firmware MQTT Presence

- [ ] 2.0 Extend `mirrorctl`/`serial_cli` argument parsing to support `--transport {serial,mqtt}` (default serial) and share configuration with the interactive TUI launcher.
- [ ] 2.1 Implement an MQTT runtime subscriber (paho-mqtt) that connects with default credentials, subscribes to `devices/+/state`, and feeds parsed presence data into existing TUI renderers with duplicate suppression and MAC/IP annotations.
- [ ] 2.2 When running in MQTT mode, route `status`/TUI tables exclusively from MQTT data and return `error: MQTT transport not implemented yet` for other commands so the behavior is explicit.
- [ ] 2.3 Update CLI/TUI documentation (help text, README snippets) to explain selecting MQTT transport, expected presence outputs, and limitations for non-presence commands.
- [ ] 2.4 Author 2-4 focused host-side tests (or mocked broker harness) validating transport selection, presence parsing, and duplicate suppression; execute only these new tests.

**Acceptance Criteria:**
- Launch `mirrorctl interactive --transport mqtt`, subscribe via local broker, and verify TUI status table updates from MQTT presence messages while other commands return `error: MQTT transport not implemented yet`.
- Run the 2-4 new host-side tests; ensure they pass without running the full suite.

### Validation & Test Coverage
**Assigned implementer:** testing-engineer
**Dependencies:** Firmware MQTT Presence, CLI & TUI MQTT Transport

- [ ] 3.0 Add PlatformIO native/host unit tests covering UUID provider seeding and presence payload formatting (state/ip/msg_id fields), keeping the total new tests in this group ≤10.
- [ ] 3.1 Create a host-level integration test or harness that fakes broker publications to ensure the CLI/TUI MQTT transport updates presence tables within the latency budget and debounces duplicates.
- [ ] 3.2 Document and execute bench validation steps verifying end-to-end latency (<25 ms median, <100 ms p95) and the firmware’s behavior when the broker is unavailable; run only the tests introduced in Tasks 3.0–3.1.

**Acceptance Criteria:**
- Execute Tasks 3.0–3.1 tests to confirm UUID seeding, payload formatting, and CLI presence updates; capture results in validation log.
- Perform bench run measuring presence flip latency (median <25 ms, p95 <100 ms) and document broker-offline behavior per spec.

### Deferred Follow-up
**Assigned implementer:** api-engineer
**Dependencies:** Firmware MQTT Presence

- [ ] 4.0 Capture a follow-up implementation task to add runtime SET/GET commands for broker host/user/pass using Preferences once the steel thread is accepted.
- [ ] 4.1 Draft acceptance criteria and testing expectations for the credential management follow-up so future work honours focused testing limits.
- [ ] 4.2 File the backlog entry (tracker ticket or repo TODO) referencing this spec and the pending credential-management scope.

**Acceptance Criteria:**
- Provide a documented backlog entry with acceptance criteria and test expectations for runtime MQTT credential commands, referencing this spec for traceability.

## Execution Order
1. Firmware MQTT Presence (Task Group 1)
2. CLI & TUI MQTT Transport (Task Group 2)
3. Validation & Test Coverage (Task Group 3)
4. Deferred Follow-up (Task Group 4)
