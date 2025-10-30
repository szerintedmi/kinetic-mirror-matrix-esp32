# Task Breakdown: Telemetry via MQTT (STATUS Parity)

## Overview
Total Tasks: 9
Assigned roles: api-engineer, testing-engineer

## Task List

### Firmware Telemetry Publisher

#### Task Group 1: Aggregate MQTT Snapshot
**Assigned implementer:** api-engineer
**Dependencies:** MQTT presence client ready (spec item 9)

- [ ] 1.0 Ship aggregate MQTT status publisher
  - [ ] 1.1 Extend `mqtt::AsyncMqttPresenceClient` (or introduce `AsyncMqttNodeClient`) to expose a shared publish queue for additional topics without spawning another client instance.
  - [ ] 1.2 Implement `mqtt::MqttStatusPublisher` that builds the aggregate `{node_state, ip, motors:{...}}` snapshot, enforces the 1 Hz / 5 Hz cadence, and suppresses unchanged payloads via hashing or checksum.
  - [ ] 1.3 Configure the MQTT Last Will on `devices/<mac>/status` with `{"node_state":"offline","motors":{}}` and wire the publisher into the main loop (`src/console/SerialConsole.cpp`) so telemetry rides alongside presence without blocking motor control.
  - [ ] 1.4 Bench-verify idle and motion cadences on hardware (≥2 motors moving) while watching broker traffic to confirm QoS0/non-retained semantics and latency targets.
  - [ ] 1.5 Remove the legacy retained `devices/<mac>/state` publisher and associated payload builders so the aggregate topic becomes the sole presence/telemetry stream.
  - [ ] 1.6 Strip the `msg_id` field from all MQTT payload builders while keeping the shared UUID generator utility alive for future MQTT command correlation work.

**Acceptance Criteria:**
- Aggregate snapshots publish at 1 Hz idle / 5 Hz motion with `node_state="ready"` and full motor payloads.
- Offline transitions broadcast the Last Will payload when power is removed or Wi-Fi drops.
- No heap growth or missed motor updates observed over a 10-minute soak test.
- UUID generator APIs remain available for roadmap item 11 even though telemetry/presence payloads no longer include `msg_id`.

### Host Tooling Updates

#### Task Group 2: MQTT CLI/TUI Parity
**Assigned implementer:** api-engineer
**Dependencies:** Task Group 1

- [ ] 2.0 Update host tooling for aggregate snapshots
  - [ ] 2.1 Adjust `tools/serial_cli/mqtt_runtime.py` to subscribe to `devices/+/status`, parse the aggregate payload, and surface per-motor rows aligned with serial `STATUS` columns.
  - [ ] 2.2 Refresh the interactive TUI status table to show stale-age highlighting based on the aggregate snapshot timestamp while keeping command dispatch disabled in MQTT mode.
  - [ ] 2.3 Ensure `mirrorctl status --transport mqtt` renders the same table as serial mode once a snapshot is received, including `actual_ms` when present, and remove any remaining references to the legacy `devices/<mac>/state` topic in CLI codepaths.
  - [ ] 2.4 Update CLI documentation (`docs/mqtt-local-broker-setup.md` or README excerpts) to explain the new topic and payload structure, explicitly noting that `devices/<mac>/status` replaces the previous `devices/<mac>/state` presence message.

**Acceptance Criteria:**
- CLI command and TUI both display motor telemetry identical to serial mode using the aggregate payload.
- Stale data (>2 s) is indicated without freezing the UI.
- Developer documentation references the new topic and JSON schema.

### Validation & Test

#### Task Group 3: Targeted Test Coverage
**Assigned implementer:** testing-engineer
**Dependencies:** Task Groups 1-2

- [ ] 3.0 Validate telemetry pipeline
  - [ ] 3.1 Add up to 6 native (host) tests covering payload serialization, cadence transitions, and change-suppression behavior for `MqttStatusPublisher`.
  - [ ] 3.2 Extend host-side tests to inject sample aggregate payloads into `MqttWorker.ingest_message` and assert the rendered table matches expected motor rows.
  - [ ] 3.3 Run only the new tests plus existing related suites; capture bench measurements demonstrating 1 Hz / 5 Hz cadence and latency budget (<100 ms p95) on the local Mosquitto broker.

**Acceptance Criteria:**
- New tests pass locally and document the aggregate payload contract.
- MQTT cadence benchmarks, test outputs, and any captured broker logs are stored under `agent-os/specs/2025-10-30-telemetry-via-mqtt-status-parity/verification`.
- No additional regressions observed in unrelated test suites.
