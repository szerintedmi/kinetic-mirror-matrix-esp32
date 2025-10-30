# Specification: Telemetry via MQTT (STATUS Parity)

## Goal
Deliver an aggregate MQTT telemetry snapshot that mirrors the existing serial `STATUS` feed so host tools can rely on MQTT for real-time status while keeping the serial pathway as a compatible fallback.

## User Stories
- As a bench operator watching multiple nodes, I want live motor telemetry over MQTT so I can monitor motion and thermal headroom without staying tethered to a USB cable.
- As a CLI/TUI user running in MQTT mode, I want the same columns and cadence I get from serial `STATUS` so I can switch transports without relearning the interface.
- As a firmware engineer, I want the telemetry publisher to reuse the existing MQTT stack so we avoid duplicate connections and stay within ESP32 resource limits.

## Core Requirements
### Functional Requirements
- Firmware must publish a single JSON snapshot on `devices/<mac>/status` (MAC normalized per `mqtt::MqttPresenceClient`) that includes node-level fields and the full set of motor states each time it emits telemetry.
- The payload mirrors the serial `STATUS` fields from `lib/MotorControl/src/command/CommandHandlers.cpp` by embedding a `motors` object keyed by motor id (`"0"` - `"7"`) with numeric JSON values and boolean literals. Example:
  ```json
  {
    "node_state": "ready",
    "ip": "192.168.1.42",
    "motors": {
      "0": {
        "id": 0,
        "position": 120,
        "moving": true,
        "awake": true,
        "homed": true,
        "steps_since_home": 360,
        "budget_s": 1.8,
        "ttfc_s": 0.4,
        "speed": 4000,
        "accel": 16000,
        "est_ms": 240,
        "started_ms": 812345
      }
    }
  }
  ```
  Omit the `actual_ms` property while `last_op_ongoing` is true to preserve parity with serial output.
- Every live publish sets `node_state` to `ready`. Configure the MQTT Last Will on the same topic with payload `{"node_state":"offline","motors":{}}` so subscribers observe an immediate offline transition when the connection drops, and remove the legacy retained `devices/<mac>/state` message entirely (the aggregate topic now carries presence and telemetry in one stream).
- Cadence control: maintain a 1 Hz keepalive cadence while no motors are moving, and switch to 5 Hz as long as any motor reports `moving=true`. Outside these periodic ticks, emit additional payloads only when the aggregated snapshot changes (e.g., motor wake/sleep, homed flag flips) to keep traffic lean, but always send at least one payload per cadence interval.
- Change detection compares the newly serialized snapshot against the last transmitted snapshot (e.g., via checksum/hash) so redundant publishes are suppressed during the 5 Hz motion window without missing state changes.
- Online publishes use QoS0 with `retain=false` to match earlier requirements; rely on the broker-propagated Last Will payload for offline visibility without adding extra metadata like UUIDs or timestamps (`msg_id` removed from status/presence payloads).
- Reuse the existing AsyncMqttClient plumbing established by `mqtt::AsyncMqttPresenceClient`. Provide a shared publish queue so the combined presence/status client maintains a single connection, keepalive, and reconnect policy.
- Integrate the telemetry loop alongside the existing presence loop in `src/console/SerialConsole.cpp`, sampling `MotorController::state(i)` to guarantee the MQTT snapshot reflects the same authoritative data used by serial `STATUS` responses.
- Extend the CLI/TUI MQTT pathway (`tools/serial_cli/mqtt_runtime.py` and `tools/serial_cli/tui/textual_ui.py`) to subscribe to `devices/+/status`, parse the aggregated snapshot, and render the same per-motor table columns already used for serial `STATUS` (including `actual_ms` when present). Leave command dispatch disabled in MQTT mode per existing behavior.
- Expose the aggregate telemetry to non-interactive CLI commands (`mirrorctl status --transport mqtt`) so they print the same tabular view as serial mode once at least one snapshot has been received.

### Non-Functional Requirements
- Aggregate payloads (≈1.5 kB worst case for eight motors) must be generated without heap churn; the publish path should reuse preallocated buffers and complete in <1 ms per cadence tick.
- Stay within QoS0/non-retained semantics for live updates to keep broker load minimal while still meeting the local latency targets.
- Host tooling must gracefully handle delayed or missing snapshots (e.g., highlight stale `node_state` data once age exceeds 2 s) without blocking the UI thread.
- Maintain compliance with logging conventions; telemetry code must not spam serial output beyond existing `CTRL:` lines.

## Visual Design
No visual assets provided; reuse existing CLI/TUI layouts and column widths so MQTT mode renders identically to serial mode.

## Reusable Components
### Existing Code to Leverage
- `lib/MotorControl/include/MotorControl/MotorController.h` and `CommandExecutionContext` for authoritative motor state and serial parity logic.
- `lib/MotorControl/src/command/CommandHandlers.cpp` for the canonical STATUS formatting and field list that must map 1:1 into JSON.
- `lib/mqtt_presence/include/mqtt/MqttPresenceClient.h` and `src/console/SerialConsole.cpp` for AsyncMqttClient setup, MAC-topic normalization, and the existing connectivity loop the new publisher will piggyback on (while keeping the UUID generator utility available for upcoming MQTT command handling).
- `tools/serial_cli/mqtt_runtime.py` and `tools/serial_cli/tui/textual_ui.py` for MQTT subscription scaffolding and the interactive TUI table refresh pipeline.

### New Components Required
- `lib/mqtt_status/include/mqtt/MqttStatusPublisher.h` (+ `src/MqttStatusPublisher.cpp`) encapsulating aggregate snapshot construction, cadence control, and change detection while consuming the shared publish queue.
- Extension points on `mqtt::AsyncMqttPresenceClient` (or a renamed `AsyncMqttNodeClient`) that expose a shared AsyncMqttClient instance to both presence and status publishers without duplicating reconnect logic, keeping the existing UUID generator accessible for future command-routing parity even though status payloads no longer include `msg_id`.
- Host-side telemetry cache within `tools/serial_cli/mqtt_runtime.py` that parses the aggregate JSON snapshot into per-motor rows while keeping the public API compatible with `SerialWorker` consumers.

## Technical Approach
- Database: No new persistence; reuse `include/secrets.h` for broker defaults and `net_onboarding::Net()` for MAC/IP discovery.
- API: Topic `devices/<mac>/status` (QoS0, non-retained). Payload schema `{node_state, ip, motors:{"0":{...}, ...}}`; document the field mapping in `docs/mqtt-status-schema.md`, note that this topic replaces the legacy retained `devices/<mac>/state` presence message, and codify the offline Last Will payload.
- Frontend: Update `MqttWorker` to subscribe to the aggregate topic, maintain a `{device: {motor_id: row}}` cache derived from `motors`, and emit row dictionaries shaped like serial mode. Update the TUI status table to show last-update age per device and highlight stale (>2 s) entries.
- Testing: Add PlatformIO native unit tests covering JSON payload formatting, change-detection throttling, and cadence transitions. Extend host-side tests to inject sample MQTT status payloads into `MqttWorker.ingest_message` and verify the rendered table matches serial expectations. Perform bench validation with a hardware loop (two motors moving) to confirm 5 Hz publish rate and CLI refresh latency (<100 ms p95) using the local Mosquitto broker described in `docs/mqtt-local-broker-setup.md`.
- UUID utility: Retain the existing UUIDv4 generator APIs introduced for presence so roadmap item 11 (MQTT command processing) can attach `cmd_id` fields, even though telemetry/presence payloads no longer include `msg_id`.

## Out of Scope
- MQTT command execution (`devices/<id>/cmd`) or serial-to-MQTT command forwarding remains future work.
- Per-motor MQTT subtopics (`devices/<id>/status/<motor_id>`) and historical status retention remain deferred unless future consumers require them.
- Adding UUID/message IDs or firmware-side timestamp fields to telemetry payloads.
- Altering existing serial `STATUS` formatting beyond sharing data structures.

## Success Criteria
- With motors idle, each node publishes an aggregate snapshot exactly once per second (QoS0, non-retained) with `node_state="ready"`, and the CLI in MQTT mode shows matching rows within 1.5 s of startup.
- During motion across ≥2 motors, firmware emits 5 Hz aggregate JSON updates without missing cadence windows, and host tooling renders the updates with the same columns as serial mode.
- Running `mirrorctl status --transport mqtt` on a local broker shows per-motor data identical (field-for-field) to the serial `STATUS` response captured at the same moment, demonstrating parity across transports.
