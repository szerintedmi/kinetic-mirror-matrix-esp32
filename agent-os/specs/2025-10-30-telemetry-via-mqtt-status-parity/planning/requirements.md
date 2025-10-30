# Spec Requirements: telemetry-via-mqtt-status-parity

## Initial Description
Telemetry via MQTT (STATUS Parity) — Publish per-motor STATUS over MQTT with parity to the current serial fields. Topic: `devices/<id>/status/<motor_id>` (QoS0) on change and at 2–5 Hz. CLI/TUI defaults to MQTT transport for live status views from this step onward; serial remains selectable as a debug backdoor. Acceptance: serial `STATUS` and MQTT status match during motion across ≥2 motors; TUI reflects updates within a few milliseconds on a local broker. Depends on: 9. `S`

> **Updated direction (2025-10-30):** Replace the per-motor topics with a single aggregate snapshot on `devices/<id>/status` that includes node presence plus all motor fields, remove the legacy retained `devices/<id>/state` message, and keep payloads lean (no `msg_id`). Cadence remains 1 Hz idle / 5 Hz motion with change suppression.

## Requirements Discussion

### First Round Questions

**Q1:** I assume we should mirror the serial `STATUS` payload fields exactly (positions, homed flag, moving/sleep states, thermal metrics, steps since last home). Is that correct, or should MQTT omit or extend any fields?
**Answer:** yes mirror it in json format

**Q2:** I’m thinking we default to publishing at 3 Hz when idle and escalate to 5 Hz only during motion. Should we lock to a single fixed rate instead?
**Answer:** idle: 1hz (constant) - motion: 5hz (constant)

**Q3:** I assume we only publish when a motor’s payload changes or when the periodic timer fires, whichever comes first, to keep traffic lean. Is that acceptable, or do you prefer every publish to include all motors regardless of change?
**Answer:** beyond keepalive publish only when changed.

**Q4:** For message format, I plan on structured JSON with stable keys (e.g., `{"motor_id":1,"position":...}`). Should we instead reuse the current serial text blobs for easier parity?
**Answer:** yes, structured json

**Q5:** I expect the firmware to send one MQTT message per motor (`devices/<id>/status/<motor_id>`) plus an aggregated `devices/<id>/status/all` snapshot for CLI convenience. Should we skip the aggregate topic?
**Answer:** skip the agreegate , consumers can do it

**Q6:** I assume these status messages are non-retained QoS0 so dashboards only see fresh data. Do you need them retained or bumped to QoS1 for reliability?
**Answer:** non retained qos0

**Q7:** I’m planning to include heartbeat metadata (e.g., `published_at_ms`, firmware build) in each payload to help the CLI detect stale nodes. Would you rather keep the payload lean and rely on presence topics for freshness?
**Answer:** no need published_at, consumers should rely on the state (and I assume msg recevied timestamp available from broker)

**Q8:** Are there specific behaviors or metrics we should explicitly exclude from this step so we don’t over-scope it?
**Answer:** commands processing is out of scope - if mqtt is selected tui / cli returns current not implemented with mqtt yet

### Existing Code to Reference
No similar existing features identified for reference.

### Follow-up Questions

**Follow-up 1:** Are there existing components, services, or CLI/TUI flows we should reference for this status telemetry work, or should we treat it as net-new?
**Answer:** no

**Follow-up 2:** Just confirming there are no visual assets (mockups, screenshots, wireframes) we should review for this spec, right?
**Answer:** no

## Visual Assets

### Files Provided:
No visual assets provided.

### Visual Insights:
None (no visual references supplied).

## Requirements Summary

### Functional Requirements
- Publish aggregate MQTT status snapshots on `devices/<id>/status` containing node presence fields and every motor's serial `STATUS` data.
- Use structured JSON payloads keyed by motor id within a single snapshot; retain structured keys but omit per-motor subtopics.
- Maintain a 1 Hz publish cadence while idle for keepalive, increasing to a constant 5 Hz when the node detects motion.
- Beyond the periodic keepalive cadence, emit status messages only when relevant snapshot data changes.
- Keep payloads lean without additional metadata such as `published_at` timestamps or UUID message IDs, while leaving the UUID generator available for future command flows.
- Broker Last Will on `devices/<id>/status` must broadcast `{"node_state":"offline","motors":{}}`, replacing the prior retained presence topic.

### Non-Functional Requirements
- MQTT live snapshots use QoS0/non-retained, relying on the Last Will for offline signaling.
- Status parity must keep CLI/TUI views responsive even when MQTT is the selected transport, despite commands remaining serial-only for now.
- Ensure snapshot generation stays within ESP32 resource budgets at 5 Hz across multiple motors (no heap churn, sub-millisecond assembly where possible).

### Reusability Opportunities
- None identified; treat telemetry implementation as net-new while staying consistent with existing serial `STATUS` logic.

### Scope Boundaries
**In Scope:**
- Implementing aggregate MQTT telemetry snapshots aligned with serial `STATUS` fields and timing.
- Handling state-change detection to suppress redundant publishes outside keepalive cadence.
- Maintaining CLI/TUI read paths that surface MQTT-derived telemetry once available and switch presence monitoring to the aggregate topic.

**Out of Scope:**
- Implementing MQTT command processing or altering CLI/TUI command transport (remains serial-only for now).
- Re-introducing per-motor MQTT topics or additional payload metadata like message UUIDs or timestamps.

### Technical Considerations
- Reuse existing serial `STATUS` data structures to guarantee one-to-one field parity with minimal translation logic.
- Coordinate with presence/LWT behavior so the aggregate topic carries both presence and telemetry without conflicting retained messages.
- Plan for efficient snapshot change detection to minimize unnecessary MQTT traffic within the 5 Hz motion window.
