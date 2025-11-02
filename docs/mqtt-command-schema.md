# Unified Serial & MQTT Command Schema

The serial console and MQTT transport now emit the same dispatcher-backed responses. This playbook shows the lifecycle, error catalog, and per-command payloads side by side so you can translate between transports quickly.

## Response Lifecycle

1. **Validation** – payload parsed and validated. Failures produce a single completion with `status="error"`; no ACK is sent.
2. **ACK (optional)** – long-running commands send an `ack` event once execution starts. Short commands skip this.
3. **Completion** – every command ends with either `status="done"` or `status="error"` plus result fields (`actual_ms`, NET state, etc.).

Serial sinks render dispatcher events as `CTRL:*` lines. MQTT publishes JSON on `devices/<node_id>/cmd/resp` with matching content. Duplicate requests replay cached responses.

### MQTT Request Envelope

Commands are published to `devices/<node_id>/cmd` with this JSON structure:

```json
{
  "cmd_id": "<uuid optional>",
  "action": "MOVE",
  "params": { ... },
  "meta": { ... optional ... }
}
```

- `cmd_id` – if omitted, firmware allocates a UUID and echoes it in responses.
- `action` – case-insensitive; normalized to upper-case.
- `params` – per-command arguments identical in meaning to the serial interface.
- `meta` – currently ignored (reserved for clients).

Responses are published to `devices/<node_id>/cmd/resp` with QoS1. Duplicate requests (ie. same `cmd_id`) replay the cached responses without re-executing the command.

### Status Values

| Status | Meaning | Notes |
|--------|---------|-------|
| `ack`  | Command accepted; more output expected. | Only emitted for async commands (MOVE, HOME, STATUS, NET:RESET, NET:LIST). |
| `done` | Command finished successfully. | Completion payload. |
| `error`| Command rejected or failed. | Completion payload with `errors[]`. |

### Error / Warning Codes

| Code | Description |
|------|-------------|
| `E01` | BAD_CMD – Unknown/unsupported action |
| `E02` | BAD_ID – Invalid target motor |
| `E03` | BAD_PARAM – Validation failure |
| `E04` | BUSY – Controller already executing |
| `E07` | POS_OUT_OF_RANGE – Position outside limits |
| `E10` | THERMAL_REQ_GT_MAX – Requested move exceeds thermal cap |
| `E11` | THERMAL_NO_BUDGET – Insufficient runtime budget |
| `E12` | THERMAL_NO_BUDGET_WAKE – WAKE blocked by thermal limits |
| `NET_BAD_PARAM` | Wi‑Fi credential payload invalid |
| `NET_SAVE_FAILED` | Failed to persist Wi‑Fi credentials |
| `NET_SCAN_AP_ONLY` | Network scan allowed only in AP mode |
| `NET_BUSY_CONNECTING` | Wi‑Fi subsystem busy connecting |
| `NET_CONNECT_FAILED` | Wi‑Fi connection attempt failed |
| `MQTT_BAD_PAYLOAD` | MQTT payload schema invalid |
| `MQTT_UNSUPPORTED_ACTION` | Action not available via MQTT transport |
| `MQTT_BAD_PARAM` | MQTT command parameters failed validation |
| `MQTT_CONFIG_SAVE_FAILED` | Persisting MQTT configuration failed |

Warnings reuse the same codes and appear alongside `ack`/`done` without changing the overall status.

## Command Reference (Serial vs MQTT)

### MOVE

| Aspect | Serial |
|--------|--------|
| Request | `MOVE:0,1200` |
| ACK | `CTRL:ACK msg_id=aa... est_ms=1778` |
| Completion | `CTRL:DONE cmd_id=6c... action=MOVE status=done actual_ms=1760` |

#### MQTT request

```json
{
  "cmd_id": "6c...",
  "action": "MOVE",
  "params": {
    "target_ids": 0,
    "position_steps": 1200
  }
}
```

#### MQTT ACK

```json
{
  "cmd_id": "6c...",
  "action": "MOVE",
  "status": "ack",
  "result": {
    "est_ms": 1778
  }
}
```

#### MQTT completion

```json
{
  "cmd_id": "6c...",
  "action": "MOVE",
  "status": "done",
  "result": {
    "actual_ms": 1760
  }
}
```

### HOME

| Aspect | Serial |
|--------|--------|
| Request | `HOME:ALL,600,150` |
| ACK | `CTRL:ACK msg_id=bb... est_ms=1820` |
| Completion | `CTRL:DONE cmd_id=48... action=HOME status=done actual_ms=1805` |

#### MQTT request

```json
{
  "cmd_id": "48...",
  "action": "HOME",
  "params": {
    "target_ids": "ALL",
    "overshoot_steps": 600,
    "backoff_steps": 150
  }
}
```

#### MQTT ACK

```json
{
  "cmd_id": "48...",
  "action": "HOME",
  "status": "ack",
  "result": {
    "est_ms": 1820
  }
}
```

#### MQTT completion

```json
{
  "cmd_id": "48...",
  "action": "HOME",
  "status": "done",
  "result": {
    "actual_ms": 1805
  }
}
```

### WAKE

| Aspect | Serial |
|--------|--------|
| Request | `WAKE:ALL` |
| Completion | `CTRL:DONE cmd_id=a5... action=WAKE status=done` |

#### MQTT request

```json
{
  "cmd_id": "a5...",
  "action": "WAKE",
  "params": {
    "target_ids": "ALL"
  }
}
```

#### MQTT completion

```json
{
  "cmd_id": "a5...",
  "action": "WAKE",
  "status": "done"
}
```

### SLEEP

| Aspect | Serial |
|--------|--------|
| Request | `SLEEP:0` |
| Completion | `CTRL:DONE cmd_id=44... action=SLEEP status=done` |

#### MQTT request

```json
{
  "cmd_id": "44...",
  "action": "SLEEP",
  "params": {
    "target_ids": 0
  }
}
```

#### MQTT completion

```json
{
  "cmd_id": "44...",
  "action": "SLEEP",
  "status": "done"
}
```

### STATUS

See [mqtt-status-schema.md](mqtt-status-schema).

STATUS streams a snapshot in the ACK and does not emit a DONE.

| Aspect | Serial |
|--------|--------|
| Request | `STATUS` |
| ACK (snapshot) | `CTRL:ACK msg_id=92... id=0 pos=0 moving=0 ...` |

#### MQTT request

 STATUS is not supported via MQTT but MQTT emmits status in devices/<mac>/status topic.

### GET ALL

| Aspect | Serial |
|--------|--------|
| Request | `GET ALL` |
| Completion | `CTRL:DONE cmd_id=d8... action=GET ACCEL=16000 DECEL=0 SPEED=4000 THERMAL_LIMITING=ON max_budget_s=90 free_heap_bytes=51264 status=done` |

#### MQTT request

```json
{
  "cmd_id": "03...",
  "action": "GET",
  "params": {
    "resource": "ALL"
  }
}
```

#### MQTT completion

```json
{
  "cmd_id": "03...",
  "action": "GET",
  "status": "done",
  "result": {
    "ACCEL": 16000,
    "DECEL": 0,
    "SPEED": 4000,
    "THERMAL_LIMITING": "ON",
    "max_budget_s": 90,
    "free_heap_bytes": 51264
  }
}
```

### `GET <resource>`

| Aspect | Serial |
|--------|--------|
| Request | `GET SPEED` |
| Completion | `CTRL:DONE cmd_id=03... action=GET status=done SPEED=4000` |

#### MQTT request

```json
{
  "cmd_id": "03...",
  "action": "GET",
  "params": {
    "resource": "speed"
  }
}
```

#### MQTT completion

```json
{
  "cmd_id": "03...",
  "action": "GET",
  "status": "done",
  "result": {
    "SPEED": 4000
  }
}
```

### SET

| Aspect | Serial |
|--------|--------|
| Request | `SET SPEED=5000` |
| Completion | `CTRL:DONE cmd_id=0f... action=SET status=done SPEED=5000` |

#### MQTT request

```json
{
  "cmd_id": "0f...",
  "action": "SET",
  "params": {
    "speed_sps": 5000
  }
}
```

#### MQTT completion

```json
{
  "cmd_id": "0f...",
  "action": "SET",
  "status": "done",
  "result": {
    "SPEED": 5000
  }
}
```

### NET:STATUS

| Aspect | Serial |
|--------|--------|
| Request | `NET:STATUS` |
| Completion | `CTRL:DONE cmd_id=7e... action=NET status=done sub_action=STATUS state=CONNECTED rssi=-55 ...` |

#### MQTT request

```json
{
  "cmd_id": "7e...",
  "action": "NET:STATUS"
}
```

#### MQTT completion

```json
{
  "cmd_id": "7e...",
  "action": "NET:STATUS",
  "status": "done",
  "result": {
    "sub_action": "STATUS",
    "state": "CONNECTED",
    "rssi": -55,
    "ssid": "\"TestNet\"",
    "ip": "192.168.4.1"
  }
}
```

### NET:RESET

| Aspect | Serial |
|--------|--------|
| Request | `NET:RESET` |
| ACK | `CTRL:ACK msg_id=51... state=CONNECTING` |
| Completion | `CTRL: NET:AP_ACTIVE msg_id=51... ssid="DeviceAP" ip=192.168.4.1`<br>`CTRL:DONE cmd_id=51... action=NET status=done sub_action=RESET state=AP_ACTIVE` |

#### MQTT request

```json
{
  "cmd_id": "51...",
  "action": "NET:RESET"
}
```

#### MQTT ACK

```json
{
  "cmd_id": "51...",
  "action": "NET:RESET",
  "status": "ack",
  "result": {
    "sub_action": "RESET",
    "state": "CONNECTING"
  }
}
```

#### MQTT completion

```json
{
  "cmd_id": "51...",
  "action": "NET:RESET",
  "status": "done",
  "result": {
    "sub_action": "RESET",
    "state": "AP_ACTIVE",
    "ssid": "\"DeviceAP\"",
    "ip": "192.168.4.1"
  }
}
```

### NET:LIST

| Aspect | Serial |
|--------|--------|
| Request | `NET:LIST` |
| ACK (results streamed) | `CTRL:ACK msg_id=29... scanning=1`<br>`NET:LIST msg_id=29...`<br>`SSID="Lab" rssi=-42 secure=1 channel=6` |
| Completion | — (scan data already delivered) | — |

#### MQTT request

```json
{
  "cmd_id": "29...",
  "action": "NET:LIST"
}
```

#### MQTT ACK

```json
{
  "cmd_id": "29...",
  "action": "NET:LIST",
  "status": "ack"
}
```

### NET:SET

| Aspect | Serial |
|--------|--------|
| Request | `NET:SET,"MyNet","password123"` |
| Completion | `CTRL:DONE cmd_id=b4... action=NET status=done sub_action=SET` |

#### MQTT request

```json
{
  "cmd_id": "b4...",
  "action": "NET:SET",
  "params": {
    "ssid": "MyNet",
    "pass": "password123"
  }
}
```

#### MQTT completion

```json
{
  "cmd_id": "b4...",
  "action": "NET:SET",
  "status": "done",
  "result": {
    "sub_action": "SET"
  }
}
```

### NET:LIST (error example)

| Aspect | Serial |
|--------|--------|
| Request | `NET:LIST` |
| Completion | `CTRL:ERR msg_id=63... NET_SCAN_AP_ONLY` |

#### MQTT request

```json
{
  "cmd_id": "63...",
  "action": "NET:LIST"
}
```

**MQTT completion (error)**

```json
{
  "cmd_id": "63...",
  "action": "NET:LIST",
  "status": "error",
  "errors": [
    {
      "code": "NET_SCAN_AP_ONLY"
    }
  ]
}
```

### MQTT:GET_CONFIG

| Aspect | Serial |
|--------|--------|
| Request | `MQTT:GET_CONFIG` |
| Completion | `CTRL:DONE cmd_id=d3... action=MQTT status=done host="192.168.1.25" port=1883 user="mirror" pass="steelthread"` |

#### MQTT request

```json
{
  "cmd_id": "d3...",
  "action": "MQTT:GET_CONFIG"
}
```

#### MQTT completion

```json
{
  "cmd_id": "d3...",
  "action": "MQTT:GET_CONFIG",
  "status": "done",
  "result": {
    "host": "\"192.168.1.25\"",
    "port": "1883",
    "user": "\"mirror\"",
    "pass": "\"steelthread\""
  }
}
```

### MQTT:SET_CONFIG

| Aspect | Serial |
|--------|--------|
| Request | `MQTT:SET_CONFIG host=lab-broker.local port=1884 user=lab pass="newsecret"` |
| Completion | `CTRL:DONE cmd_id=c8... action=MQTT status=done host="lab-broker.local" port=1884 user="lab" pass="newsecret"` |

#### MQTT request

```json
{
  "cmd_id": "c8...",
  "action": "MQTT:SET_CONFIG",
  "params": {
    "host": "lab-broker.local",
    "port": 1884,
    "user": "lab",
    "pass": "newsecret"
  }
}
```

#### MQTT completion

```json
{
  "cmd_id": "c8...",
  "action": "MQTT:SET_CONFIG",
  "status": "done",
  "result": {
    "host": "\"lab-broker.local\"",
    "port": "1884",
    "user": "\"lab\"",
    "pass": "\"newsecret\""
  }
}
```

#### Resetting to defaults

- Serial: `MQTT:SET_CONFIG RESET`
- MQTT JSON:

```json
{
  "cmd_id": "e1...",
  "action": "MQTT:SET_CONFIG",
  "params": {
    "reset": true
  }
}
```

 To return to compile-time defaults, use RESET.

## Duplicate Handling

1. Firmware logs `CTRL:INFO MQTT_DUPLICATE cmd_id=<...>` (rate limited).
2. Previously published ACK/Completion payloads are replayed verbatim.
3. The underlying command is not executed again.

## Client Guidance

- Prefer the MQTT JSON payload for machine consumption; serial output remains useful for diagnostics or manual control.
- Commands that emit only a completion (`WAKE`, `SLEEP`, `GET`, `SET`, `NET:STATUS`, `NET:SET`) can be considered complete after the first `status:"done"` payload.
- STATUS and NET:LIST stream their payload in the ACK and do not send DONE.
- Warnings provide additional context (e.g., thermal budget) without affecting success/failure state.
- Serial supports multi-command batches (`MOVE:0,100;MOVE:1,200`); MQTT clients should submit individual JSON commands.
- Firmware normalises action/resource casing; clients may send lower-case tokens if desired.
- Broker overrides persist in Preferences; use `MQTT:GET_CONFIG` to inspect active settings and `MQTT:SET_CONFIG` with either specific fields or `reset:true` to update or revert them.
- Host CLI/TUI tooling accepts traditional serial command syntax (`MOVE:0,1200`, `NET:RESET`) even when connected over MQTT. The client maps those lines into the JSON envelope described here, publishes to `devices/<node_id>/cmd`, and logs `[ACK]` / `[DONE]` entries derived from dispatcher events (including `cmd_id`, warnings, and timing metadata). The CLI never synthesises `cmd_id` values; if omitted in the request the firmware allocates one and echoes it in subsequent responses.
