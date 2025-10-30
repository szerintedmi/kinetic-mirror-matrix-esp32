# MQTT Status Snapshot Schema

Firmware publishes real-time telemetry on `devices/<mac>/status` where `<mac>` is the device MAC address rendered in lowercase without separators. Publishes use QoS 0 and `retain=false`. The broker Last Will is registered on the same topic with the payload `{"node_state":"offline","motors":{}}` so subscribers immediately observe offline transitions.

## Payload Structure

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
      "started_ms": 812345,
      "actual_ms": 230
    }
  }
}
```

### Top-Level Fields

| Field        | Type   | Description |
|--------------|--------|-------------|
| `node_state` | string | Node readiness state. Live publishes send `"ready"`; the Last Will payload sets this to `"offline"` with an empty `motors` object. |
| `ip`         | string | Current IPv4 address reported by `NetOnboarding`. Defaults to `"0.0.0.0"` if not available. |
| `motors`     | object | Map of motor ids to per-motor telemetry objects. Keys are stringified motor indices (`"0"`-`"7"`). |

### Motor Object Fields

| Field               | Type    | Description |
|---------------------|---------|-------------|
| `id`                | number  | Numeric motor identifier (mirrors the object key). |
| `position`          | number  | Absolute step position. |
| `moving`            | boolean | `true` while a MOVE/HOME is in progress. |
| `awake`             | boolean | `true` if the motor driver is powered. |
| `homed`             | boolean | `true` once the motor successfully completes HOME. |
| `steps_since_home`  | number  | Steps accumulated since the last HOME. |
| `budget_s`          | number  | Remaining runtime budget (seconds, single decimal). Negative values indicate over-budget runtime. |
| `ttfc_s`            | number  | Estimated time-to-full-cooldown (seconds, single decimal). |
| `speed`             | number  | Last commanded speed in steps per second. |
| `accel`             | number  | Last commanded acceleration in steps per second^2. |
| `est_ms`            | number  | Estimated duration for the active MOVE/HOME (milliseconds). |
| `started_ms`        | number  | Firmware millis timestamp when the active MOVE/HOME began. |
| `actual_ms`         | number  | Duration of the most recently completed MOVE/HOME in milliseconds. This field is omitted while `moving=true` / `last_op_ongoing=true`. |

## Cadence Guarantees

- 1 Hz idle cadence when all motors report `moving=false`.
- 5 Hz cadence while any motor reports `moving=true`.
- Additional publishes occur immediately when the serialized snapshot changes (wake/sleep transitions, homed flag flips, etc.).
- Duplicate payloads are suppressed between cadence ticks via payload hashing.

The host CLI (`mirrorctl status --transport mqtt`) and TUI subscribe to this topic and render tables identical to the serial `STATUS` command.
