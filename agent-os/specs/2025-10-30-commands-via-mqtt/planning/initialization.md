# Initial Spec Idea

## User's Initial Description
Commands via MQTT — Implement command request/response with strict `cmd_id` correlation (no QoS2) and full feature parity with the current serial command set (all actions and parameters). Requests: `devices/<id>/cmd` (QoS1). Responses: `devices/<id>/cmd/resp` (QoS1) with `{ ok|err, est_ms }`. Node does not queue; if busy, immediately replies `BUSY`. CLI/TUI defaults to MQTT transport (serial selectable via `--transport serial`). Acceptance: every serial command and parameter works end‑to‑end over MQTT with correlated acks; concurrent `MOVE` rejected with `BUSY`; events/status reflect progress. Depends on: 10. `M`

## Metadata
- Date Created: 2025-10-30
- Spec Name: commands-via-mqtt
- Spec Path: agent-os/specs/2025-10-30-commands-via-mqtt
