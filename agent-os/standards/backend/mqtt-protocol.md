# MQTT Protocol

## Topic Naming

- Base: `devices/<mac>/` where MAC is lowercase, no colons/dashes
- `devices/<mac>/status` — telemetry snapshots (retain=false)
- `devices/<mac>/config` — device configuration (retain=true)
- `devices/<mac>/cmd` — inbound commands
- `devices/<mac>/cmd/resp` — command responses
- MAC-based identity is firm (immutable at hardware level)

## Response Lifecycle (Two-Phase)

1. **ACK** (`"status":"ack"`) — command accepted, execution started
2. **Completion** (`"status":"done"` or `"status":"error"`) — execution finished

```json
{"cmd_id":"...", "action":"MOVE", "status":"ack", "result":{...}}
{"cmd_id":"...", "action":"MOVE", "status":"done", "result":{...}, "warnings":[], "errors":[]}
```

- Motor snapshot included only in completion (not ACK)
- MOVE completions delayed until motors finish (`pending_` vector, polled in loop)

## QoS Hierarchy

- Command responses: **QoS 1** (at-least-once delivery)
- Presence/status: **QoS 0** (fire-and-forget)
- Config: **QoS 0**, retain=true

## Publish Queue

- Capacity: 32 messages
- Priority policy: status messages (replaceable) dropped first to make room for command responses
- Only one status message in queue at a time

## Status Publishing

- Idle interval: 1000ms (1 Hz)
- Motion interval: 200ms (5 Hz) — auto-switches based on `state.moving`
- Hash-based dedup: only publishes if payload changed or interval due

## Presence Heartbeat

- Publishes `node_state:"ready"` with IP, motor state
- Offline payload: `{"node_state":"offline","motors":{}}`
- State-change detection triggers immediate publish (IP change, motion start/stop)

## Duplicate Detection

- 12-message cache for command dedup
- Duplicate commands get cached ACK/completion replayed
