# MQTT Command Schema

This document describes the MQTT command protocol for controlling the kinetic mirror firmware. For real-world payload examples, see [mqtt-payload-examples.md](./mqtt-payload-examples.md).

## Overview

Commands are published to `devices/<node_id>/cmd` and responses are published to `devices/<node_id>/cmd/resp`. The `<node_id>` is the device MAC address in lowercase without separators (e.g., `8857212316bc`).

### Request Envelope

```json
{
  "cmd_id": "<uuid optional>",
  "action": "MOVE",
  "params": { ... },
  "meta": { ... optional ... }
}
```

| Field    | Required | Description                                                               |
| -------- | -------- | ------------------------------------------------------------------------- |
| `cmd_id` | No       | Client-provided UUID for correlation. If omitted, firmware generates one. |
| `action` | Yes      | Command action (case-insensitive, normalized to uppercase).               |
| `params` | Varies   | Command-specific parameters object.                                       |
| `meta`   | No       | Reserved for client use; ignored by firmware.                             |

### Response Lifecycle

1. **Validation** - Payload parsed and validated. Failures produce `status="error"` immediately.
2. **ACK** - Long-running commands (MOVE, HOME, NET:RESET, NET:LIST) send `status="ack"` when execution starts.
3. **Completion** - Every command ends with `status="done"` or `status="error"`.

### Status Values

| Status  | Meaning                                                    |
| ------- | ---------------------------------------------------------- |
| `ack`   | Command accepted; execution started. More output expected. |
| `done`  | Command completed successfully.                            |
| `error` | Command rejected or failed. Includes `errors[]` array.     |

### Error Codes

| Code                      | Reason                 | Description                               |
| ------------------------- | ---------------------- | ----------------------------------------- |
| `E01`                     | BAD_CMD                | Unknown or unsupported action             |
| `E02`                     | BAD_ID                 | Invalid motor ID or target mask           |
| `E03`                     | BAD_PARAM              | Parameter validation failure              |
| `E04`                     | BUSY                   | Controller is executing another command   |
| `E07`                     | POS_OUT_OF_RANGE       | Position outside allowed travel range     |
| `E10`                     | THERMAL_REQ_GT_MAX     | Requested move exceeds max thermal budget |
| `E11`                     | THERMAL_NO_BUDGET      | Insufficient thermal budget available     |
| `E12`                     | THERMAL_NO_BUDGET_WAKE | WAKE blocked by thermal limits            |
| `NET_BAD_PARAM`           | -                      | Wi-Fi credential payload invalid          |
| `NET_SAVE_FAILED`         | -                      | Failed to persist Wi-Fi credentials       |
| `NET_SCAN_AP_ONLY`        | -                      | Network scan only allowed in AP mode      |
| `NET_BUSY_CONNECTING`     | -                      | Wi-Fi subsystem busy connecting           |
| `NET_CONNECT_FAILED`      | -                      | Wi-Fi connection attempt failed           |
| `MQTT_BAD_PAYLOAD`        | -                      | MQTT payload schema invalid               |
| `MQTT_UNSUPPORTED_ACTION` | -                      | Action not available via MQTT             |
| `MQTT_BAD_PARAM`          | -                      | MQTT parameters failed validation         |
| `MQTT_CONFIG_SAVE_FAILED` | -                      | Failed to persist MQTT configuration      |

Warnings use the same codes and appear in a `warnings[]` array alongside successful responses.

---

## Motor Commands

### MOVE

Move motor(s) to an absolute position.

**Request:**

```json
{
  "action": "MOVE",
  "params": {
    "target_ids": 0,
    "position_steps": 1200,
    "speed": 4000,
    "accel": 16000
  }
}
```

| Parameter        | Type         | Required | Default      | Description                      |
| ---------------- | ------------ | -------- | ------------ | -------------------------------- |
| `target_ids`     | int \| "ALL" | No       | 0            | Motor ID or "ALL" for all motors |
| `position_steps` | int          | Yes      | -            | Target position in steps         |
| `speed`          | int          | No       | Global SPEED | Steps per second                 |
| `accel`          | int          | No       | Global ACCEL | Acceleration in steps/s²         |

**ACK Response:**

```json
{
  "cmd_id": "...",
  "action": "MOVE",
  "status": "ack",
  "result": { "est_ms": 1778 }
}
```

**Completion:**

```json
{
  "cmd_id": "...",
  "action": "MOVE",
  "status": "done",
  "result": { "actual_ms": 1760, "started_ms": 311978 }
}
```

### HOME

Home motor(s) by moving until the limit switch triggers.

**Request:**

```json
{
  "action": "HOME",
  "params": {
    "target_ids": "ALL",
    "overshoot_steps": 600,
    "backoff_steps": 150,
    "speed": 4000,
    "accel": 16000,
    "full_range_steps": 1200
  }
}
```

| Parameter          | Type         | Required | Default           | Description                      |
| ------------------ | ------------ | -------- | ----------------- | -------------------------------- |
| `target_ids`       | int \| "ALL" | Yes      | -                 | Motor ID or "ALL"                |
| `overshoot_steps`  | int          | No       | 600               | Steps to overshoot past home     |
| `backoff_steps`    | int          | No       | 150               | Steps to back off after homing   |
| `speed`            | int          | No       | Global SPEED      | Steps per second                 |
| `accel`            | int          | No       | Global ACCEL      | Acceleration in steps/s²         |
| `full_range_steps` | int          | No       | MAX_POS - MIN_POS | Full travel range for estimation |

**ACK Response:**

```json
{
  "cmd_id": "...",
  "action": "HOME",
  "status": "ack",
  "result": { "est_ms": 1820 }
}
```

**Completion:**

```json
{
  "cmd_id": "...",
  "action": "HOME",
  "status": "done",
  "result": { "actual_ms": 1805, "started_ms": 90800 }
}
```

### WAKE

Wake motor driver(s) from sleep mode.

**Request:**

```json
{
  "action": "WAKE",
  "params": { "target_ids": "ALL" }
}
```

| Parameter    | Type         | Required | Description       |
| ------------ | ------------ | -------- | ----------------- |
| `target_ids` | int \| "ALL" | Yes      | Motor ID or "ALL" |

**Completion:**

```json
{
  "cmd_id": "...",
  "action": "WAKE",
  "status": "done"
}
```

### SLEEP

Put motor driver(s) into sleep mode.

**Request:**

```json
{
  "action": "SLEEP",
  "params": { "target_ids": 0 }
}
```

| Parameter    | Type         | Required | Description       |
| ------------ | ------------ | -------- | ----------------- |
| `target_ids` | int \| "ALL" | Yes      | Motor ID or "ALL" |

**Completion:**

```json
{
  "cmd_id": "...",
  "action": "SLEEP",
  "status": "done"
}
```

---

## Configuration Commands

### GET

Retrieve configuration values.

**Request (all values):**

```json
{
  "action": "GET",
  "params": { "resource": "ALL" }
}
```

**Request (specific resource):**

```json
{
  "action": "GET",
  "params": { "resource": "SPEED" }
}
```

| Resource           | Description                     |
| ------------------ | ------------------------------- |
| `ALL`              | All configuration values        |
| `SPEED`            | Default speed (steps/s)         |
| `ACCEL`            | Default acceleration (steps/s²) |
| `DECEL`            | Default deceleration (steps/s²) |
| `THERMAL_LIMITING` | Thermal limiting state (ON/OFF) |
| `MICROSTEP`        | Microstepping mode              |
| `LAST_OP_TIMING`   | Last operation timing info      |

**Completion (ALL):**

```json
{
  "cmd_id": "...",
  "action": "GET",
  "status": "done",
  "result": {
    "SPEED": 4000,
    "ACCEL": 16000,
    "DECEL": 16000,
    "MICROSTEP": "1/32",
    "THERMAL_LIMITING": "ON",
    "max_budget_s": 90,
    "free_heap_bytes": 51264,
    "firmware_version": "41a147e",
    "firmware_date": "2025-11-29T04:20:26Z"
  }
}
```

### SET

Update configuration values. Only one field per request.

**Request:**

```json
{
  "action": "SET",
  "params": { "SPEED": 5000 }
}
```

| Parameter          | Type   | Valid Values                                 | Description                      |
| ------------------ | ------ | -------------------------------------------- | -------------------------------- |
| `SPEED`            | int    | > 0                                          | Default speed in steps/s         |
| `ACCEL`            | int    | > 0                                          | Default acceleration in steps/s² |
| `DECEL`            | int    | >= 0                                         | Default deceleration in steps/s² |
| `THERMAL_LIMITING` | string | "ON", "OFF"                                  | Enable/disable thermal limiting  |
| `MICROSTEP`        | string | "FULL", "HALF", "1/4", "1/8", "1/16", "1/32" | Microstepping mode               |

**Note:** SET MICROSTEP requires all motors to be stopped and asleep. Returns `E04 BUSY` otherwise.

**Completion:**

```json
{
  "cmd_id": "...",
  "action": "SET",
  "status": "done",
  "result": { "SPEED": 5000 }
}
```

**SET MICROSTEP Completion:**

```json
{
  "cmd_id": "...",
  "action": "SET",
  "status": "done",
  "result": { "MICROSTEP": "1/16", "multiplier": 16 }
}
```

---

## Network Commands

### NET:STATUS

Get current network status.

**Request:**

```json
{ "action": "NET:STATUS" }
```

**Completion:**

```json
{
  "cmd_id": "...",
  "action": "NET:STATUS",
  "status": "done",
  "result": {
    "sub_action": "STATUS",
    "state": "CONNECTED",
    "rssi": -55,
    "ssid": "\"HomeNetwork\"",
    "ip": "192.168.1.8"
  }
}
```

### NET:SET

Set Wi-Fi credentials and connect.

**Request:**

```json
{
  "action": "NET:SET",
  "params": {
    "ssid": "MyNetwork",
    "pass": "password123"
  }
}
```

**Completion:**

```json
{
  "cmd_id": "...",
  "action": "NET:SET",
  "status": "done",
  "result": { "sub_action": "SET" }
}
```

### NET:RESET

Clear stored credentials and start AP mode.

**Request:**

```json
{ "action": "NET:RESET" }
```

**ACK:**

```json
{
  "cmd_id": "...",
  "action": "NET:RESET",
  "status": "ack",
  "result": { "sub_action": "RESET", "state": "CONNECTING" }
}
```

**Completion:**

```json
{
  "cmd_id": "...",
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

Scan for nearby networks. Only available in AP mode.

**Request:**

```json
{ "action": "NET:LIST" }
```

**ACK:**

```json
{
  "cmd_id": "...",
  "action": "NET:LIST",
  "status": "ack"
}
```

---

## MQTT Configuration Commands

### MQTT:GET_CONFIG

Get current MQTT broker configuration.

**Request:**

```json
{ "action": "MQTT:GET_CONFIG" }
```

**Completion:**

```json
{
  "cmd_id": "...",
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

Update MQTT broker configuration.

**Request:**

```json
{
  "action": "MQTT:SET_CONFIG",
  "params": {
    "host": "broker.local",
    "port": 1884,
    "user": "newuser",
    "pass": "newpass"
  }
}
```

**Reset to defaults:**

```json
{
  "action": "MQTT:SET_CONFIG",
  "params": { "reset": true }
}
```

**Completion:**

```json
{
  "cmd_id": "...",
  "action": "MQTT:SET_CONFIG",
  "status": "done",
  "result": {
    "host": "\"broker.local\"",
    "port": "1884",
    "user": "\"newuser\"",
    "pass": "\"newpass\""
  }
}
```

---

## Utility Commands

### HELP

Get list of available commands.

**Request:**

```json
{ "action": "HELP" }
```

**Completion:**

```json
{
  "cmd_id": "...",
  "action": "HELP",
  "status": "done",
  "result": {
    "lines": [
      "HELP",
      "MOVE:<id|ALL>,<abs_steps>[,<speed>][,<accel>]",
      "HOME:<id|ALL>[,<overshoot>][,<backoff>][,<speed>][,<accel>][,<full_range>]",
      "..."
    ]
  }
}
```

### STATUS

**Note:** STATUS command is not supported via MQTT. Motor telemetry is published automatically to `devices/<node_id>/status`. See [mqtt-status-schema.md](./mqtt-status-schema.md).

---

## Duplicate Handling

When the firmware receives a command with a previously-seen `cmd_id`:

1. Logs `CTRL:INFO MQTT_DUPLICATE cmd_id=<...>` (rate limited)
2. Replays cached ACK and completion payloads
3. Does not re-execute the command

---

## Client Guidance

- Commands that emit only completion (`HELP`, `WAKE`, `SLEEP`, `GET`, `SET`, `NET:STATUS`, `NET:SET`) complete after the first `status:"done"`.
- Long-running commands (`MOVE`, `HOME`, `NET:RESET`, `NET:LIST`) emit `ack` then `done`.
- Warnings appear in `warnings[]` without affecting success/failure status.
- Action and resource names are case-insensitive.
- MQTT clients should submit individual JSON commands (no batching like serial).

---

## Appendix: Serial Protocol Quick Reference

The firmware also accepts commands via serial console. Serial uses a text-based protocol that maps to the same internal dispatcher.

### Serial Command Format

```
<ACTION>:<args>
```

Examples:

- `MOVE:0,1200` - Move motor 0 to position 1200
- `MOVE:0,1200,2000,8000` - Move with speed=2000, accel=8000
- `HOME:ALL,600,150` - Home all motors with overshoot=600, backoff=150
- `HOME:0,600,150,2000,8000,1500` - Home with all parameters
- `SET SPEED=5000` - Set default speed
- `GET ALL` - Get all configuration
- `NET:STATUS` - Get network status

### Serial Response Format

```
CTRL:ACK msg_id=<id> <fields...>
CTRL:DONE cmd_id=<id> action=<ACTION> status=done <fields...>
CTRL:ERR msg_id=<id> <code> <reason>
CTRL:WARN msg_id=<id> <code> <fields...>
```

### Serial Command Mapping

| Serial                                              | MQTT Action     | Notes                                  |
| --------------------------------------------------- | --------------- | -------------------------------------- |
| `MOVE:<id>,<pos>[,<speed>,<accel>]`                 | MOVE            | Position and optional overrides        |
| `HOME:<id>[,<over>,<back>,<speed>,<accel>,<range>]` | HOME            | All params optional except target      |
| `WAKE:<id>`                                         | WAKE            |                                        |
| `SLEEP:<id>`                                        | SLEEP           |                                        |
| `STATUS`                                            | -               | Serial only; MQTT uses telemetry topic |
| `GET [resource]`                                    | GET             |                                        |
| `SET <key>=<value>`                                 | SET             |                                        |
| `NET:STATUS`                                        | NET:STATUS      |                                        |
| `NET:SET,"<ssid>","<pass>"`                         | NET:SET         | Quoted strings                         |
| `NET:RESET`                                         | NET:RESET       |                                        |
| `NET:LIST`                                          | NET:LIST        |                                        |
| `MQTT:GET_CONFIG`                                   | MQTT:GET_CONFIG |                                        |
| `MQTT:SET_CONFIG <key>=<val>...`                    | MQTT:SET_CONFIG |                                        |
| `HELP`                                              | HELP            |                                        |

### Serial Shortcuts

| Shortcut | Full Command |
| -------- | ------------ |
| `M`      | `MOVE`       |
| `H`      | `HOME`       |
| `ST`     | `STATUS`     |

### Serial Multi-Command

Serial supports batching multiple commands with semicolons:

```
MOVE:0,100;MOVE:1,200
```

Note: No command queuing; only distinct motors allowed per batch.
