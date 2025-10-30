# Initial Spec Idea

## User's Initial Description
Telemetry via MQTT (STATUS Parity) — Publish per-motor STATUS over MQTT with parity to the current serial fields. Topic: `devices/<id>/status/<motor_id>` (QoS0) on change and at 2–5 Hz. CLI/TUI defaults to MQTT transport for live status views from this step onward; serial remains selectable as a debug backdoor. Acceptance: serial `STATUS` and MQTT status match during motion across ≥2 motors; TUI reflects updates within a few milliseconds on a local broker. Depends on: 9. `S`

## Metadata
- Date Created: 2025-10-30
- Spec Name: telemetry-via-mqtt-status-parity
- Spec Path: agent-os/specs/2025-10-30-telemetry-via-mqtt-status-parity
