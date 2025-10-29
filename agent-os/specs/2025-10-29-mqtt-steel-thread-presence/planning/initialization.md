# Initial Spec Idea

## User's Initial Description
9. [ ] MQTT Steel Thread: Presence — Connect to a local broker with username/password; publish retained birth message and LWT on `devices/<id>/state`. Acceptance: node publishes “ready” immediately after connect; CLI shows online/offline flips within a few milliseconds on a local broker (target <25 ms median, <100 ms p95). Depends on: 8. `S`

## Metadata
- Date Created: 2025-10-29
- Spec Name: mqtt-steel-thread-presence
- Spec Path: agent-os/specs/2025-10-29-mqtt-steel-thread-presence
