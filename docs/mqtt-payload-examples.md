# MQTT Payload Examples

This document provides real-world examples of MQTT payloads exchanged with the firmware. For schema definitions, see:

- [mqtt-status-schema.md](./mqtt-status-schema.md) - Status topic structure
- [mqtt-config-schema.md](./mqtt-config-schema.md) - Config topic structure
- [mqtt-command-schema.md](./mqtt-command-schema.md) - Command/response protocol

## Broadcasted config

`devices/8857212316bc/config` (retained)

```json
{
  "thermal_limiting": "ON",
  "max_budget_s": 90,
  "microstep": "FULL",
  "microstep_mult": 1,
  "speed": 4000,
  "accel": 16000,
  "decel": 16000
}
```

## Broadcasted status

`devices/8857212316bc/status`

```json
{
  "node_state": "ready",
  "ip": "192.168.1.8",
  "motors": {
    "0": {
      "id": 0,
      "position": 1200,
      "moving": false,
      "awake": false,
      "homed": true,
      "steps_since_home": 6000,
      "budget_s": 90.0,
      "ttfc_s": 0.0,
      "speed": 300,
      "accel": 16000,
      "est_ms": 8019,
      "started_ms": 440952,
      "actual_ms": 8018
    },
    "1": {
      "id": 1,
      "position": 0,
      "moving": false,
      "awake": false,
      "homed": true,
      "steps_since_home": 0,
      "budget_s": 90.0,
      "ttfc_s": 0.0,
      "speed": 4000,
      "accel": 16000,
      "est_ms": 1778,
      "started_ms": 90800,
      "actual_ms": 1791
    },
    "2": {
      "id": 2,
      "position": 0,
      "moving": false,
      "awake": false,
      "homed": true,
      "steps_since_home": 0,
      "budget_s": 90.0,
      "ttfc_s": 0.0,
      "speed": 4000,
      "accel": 16000,
      "est_ms": 1778,
      "started_ms": 90800,
      "actual_ms": 1791
    },
    "3": {
      "id": 3,
      "position": 0,
      "moving": false,
      "awake": false,
      "homed": true,
      "steps_since_home": 0,
      "budget_s": 90.0,
      "ttfc_s": 0.0,
      "speed": 4000,
      "accel": 16000,
      "est_ms": 1778,
      "started_ms": 90800,
      "actual_ms": 1791
    },
    "4": {
      "id": 4,
      "position": 0,
      "moving": false,
      "awake": false,
      "homed": true,
      "steps_since_home": 0,
      "budget_s": 90.0,
      "ttfc_s": 0.0,
      "speed": 4000,
      "accel": 16000,
      "est_ms": 1778,
      "started_ms": 90800,
      "actual_ms": 1791
    },
    "5": {
      "id": 5,
      "position": 0,
      "moving": false,
      "awake": false,
      "homed": true,
      "steps_since_home": 0,
      "budget_s": 90.0,
      "ttfc_s": 0.0,
      "speed": 4000,
      "accel": 16000,
      "est_ms": 1778,
      "started_ms": 90800,
      "actual_ms": 1791
    },
    "6": {
      "id": 6,
      "position": 0,
      "moving": false,
      "awake": false,
      "homed": true,
      "steps_since_home": 0,
      "budget_s": 90.0,
      "ttfc_s": 0.0,
      "speed": 4000,
      "accel": 16000,
      "est_ms": 1778,
      "started_ms": 90800,
      "actual_ms": 1793
    },
    "7": {
      "id": 7,
      "position": 0,
      "moving": false,
      "awake": false,
      "homed": true,
      "steps_since_home": 0,
      "budget_s": 90.0,
      "ttfc_s": 0.0,
      "speed": 4000,
      "accel": 16000,
      "est_ms": 1778,
      "started_ms": 90800,
      "actual_ms": 1793
    }
  }
}
```

## Homing

`devices/8857212316bc/cmd`

```json
{
  "action": "HOME",
  "params": { "target_ids": "ALL" },
  "cmd_id": "8d6d3766-cafd-4f4f-9622-3feff1f105c0"
}
```

`devices/8857212316bc/cmd/resp`

```json
{
  "cmd_id": "8d6d3766-cafd-4f4f-9622-3feff1f105c0",
  "action": "HOME",
  "status": "ack",
  "result": { "est_ms": 1778 }
}
```

`devices/8857212316bc/cmd/resp`

```json
{
  "cmd_id": "8d6d3766-cafd-4f4f-9622-3feff1f105c0",
  "action": "HOME",
  "status": "done",
  "result": { "actual_ms": 1793, "started_ms": 90800 }
}
```

## MOVE

`devices/8857212316bc/cmd`

```json
{
  "action": "MOVE",
  "params": { "target_ids": 0, "position_steps": 1200 },
  "cmd_id": "12ee73f9-be7e-42f6-ab20-1b5c6bcc0662"
}
```

`devices/8857212316bc/cmd/resp`

```json
{
  "cmd_id": "12ee73f9-be7e-42f6-ab20-1b5c6bcc0662",
  "action": "MOVE",
  "status": "ack",
  "result": { "est_ms": 550 }
}
```

`devices/8857212316bc/cmd/resp`

```json
{
  "cmd_id": "12ee73f9-be7e-42f6-ab20-1b5c6bcc0662",
  "action": "MOVE",
  "status": "done",
  "result": { "actual_ms": 549, "started_ms": 311978 }
}
```

## Errors

### 1. Busy

`devices/8857212316bc/cmd`

```json
{
  "action": "MOVE",
  "params": { "target_ids": 0, "position_steps": 1200 },
  "cmd_id": "b4b99d4a-5656-4576-a620-cb3c276112a2"
}
```

`devices/8857212316bc/cmd/resp`

```json
{
  "cmd_id": "b4b99d4a-5656-4576-a620-cb3c276112a2",
  "action": "MOVE",
  "status": "error",
  "errors": [
    { "code": "E04", "reason": "BUSY", "message": "Controller is busy executing another command." }
  ]
}
```

### 2. Position out of range (error)

`devices/8857212316bc/cmd`

```json
{
  "action": "MOVE",
  "params": { "target_ids": 1, "position_steps": 1201 },
  "cmd_id": "66bf4c67-00d3-4b51-8e94-60371de5bd81"
}
```

`devices/8857212316bc/cmd/resp`

```json
{
  "cmd_id": "66bf4c67-00d3-4b51-8e94-60371de5bd81",
  "action": "MOVE",
  "status": "error",
  "errors": [
    {
      "code": "E07",
      "reason": "POS_OUT_OF_RANGE",
      "message": "Requested position is outside the allowed travel range."
    }
  ]
}
```

### 3. Thermal budget error when limiting is on (default)

If a command would result exceeding thermal budget and `THERMAL_LIMITING == "ON"` the command is rejected with error and not executed.

`devices/8857212316bc/cmd`

```json
{
  "action": "MOVE",
  "params": { "target_ids": 1, "position_steps": 1200 },
  "cmd_id": "d957e5e7-a07e-4e62-9be4-42234992cf0d"
}
```

`devices/8857212316bc/cmd/resp`

```json
{
  "cmd_id": "d957e5e7-a07e-4e62-9be4-42234992cf0d",
  "action": "MOVE",
  "status": "error",
  "errors": [
    {
      "code": "E11",
      "reason": "THERMAL_NO_BUDGET",
      "message": "Insufficient thermal budget to run the command.",
      "id": 1,
      "req_ms": 8019,
      "budget_s": 0,
      "ttfc_s": 148
    }
  ]
}
```

### 4. Thermal budget warning(s) when limiting is off

When `THERMAL_LIMITING == "OFF"` and the command execution will exceed thermal limits then commands executed and ack / done responses include warnings

`devices/8857212316bc/cmd`

```json
{
  "action": "MOVE",
  "params": { "target_ids": 1, "position_steps": -1200 },
  "cmd_id": "15e9147c-a9ef-4724-8a5d-6c4e6c8b31f6"
}
```

`devices/8857212316bc/cmd/resp`

```json
{
  "cmd_id": "15e9147c-a9ef-4724-8a5d-6c4e6c8b31f6",
  "action": "MOVE",
  "status": "ack",
  "result": { "est_ms": 8019 },
  "warnings": [
    { "code": "THERMAL_NO_BUDGET", "budget_s": 0, "id": 1, "req_ms": 8019, "ttfc_s": 69 }
  ]
}
```

`devices/8857212316bc/cmd/resp`

```json
{
  "cmd_id": "15e9147c-a9ef-4724-8a5d-6c4e6c8b31f6",
  "action": "MOVE",
  "status": "done",
  "warnings": [
    { "code": "THERMAL_NO_BUDGET", "budget_s": 0, "id": 1, "req_ms": 8019, "ttfc_s": 69 }
  ],
  "result": { "actual_ms": 8017, "started_ms": 1506008 }
}
```

## GET ALL

`devices/8857212316bc/cmd`

```json
{
  "action": "GET",
  "params": { "resource": "ALL" },
  "cmd_id": "03f8a2b1-7c4d-4e5f-9a1b-2c3d4e5f6a7b"
}
```

`devices/8857212316bc/cmd/resp`

```json
{
  "cmd_id": "03f8a2b1-7c4d-4e5f-9a1b-2c3d4e5f6a7b",
  "action": "GET",
  "status": "done",
  "result": {
    "ACCEL": 16000,
    "DECEL": 16000,
    "SPEED": 4000,
    "THERMAL_LIMITING": "ON",
    "max_budget_s": 90,
    "free_heap_bytes": 51264
  }
}
```

## SET SPEED

`devices/8857212316bc/cmd`

```json
{
  "action": "SET",
  "params": { "speed_sps": 300 },
  "cmd_id": "0f1e2d3c-4b5a-6978-8796-a5b4c3d2e1f0"
}
```

`devices/8857212316bc/cmd/resp`

```json
{
  "cmd_id": "0f1e2d3c-4b5a-6978-8796-a5b4c3d2e1f0",
  "action": "SET",
  "status": "done",
  "result": { "SPEED": 300 }
}
```

## SET MICROSTEP

Note: Requires all motors to be stopped and asleep.

`devices/8857212316bc/cmd`

```json
{
  "action": "SET",
  "params": { "MICROSTEP": "1/16" },
  "cmd_id": "5a9b8c7d-6e5f-4a3b-2c1d-0e9f8a7b6c5d"
}
```

`devices/8857212316bc/cmd/resp`

```json
{
  "cmd_id": "5a9b8c7d-6e5f-4a3b-2c1d-0e9f8a7b6c5d",
  "action": "SET",
  "status": "done",
  "result": { "MICROSTEP": "1/16", "multiplier": 16 }
}
```

After this command, the config topic is republished:

`devices/8857212316bc/config` (retained)

```json
{
  "thermal_limiting": "ON",
  "max_budget_s": 90,
  "microstep": "1/16",
  "microstep_mult": 16,
  "speed": 300,
  "accel": 16000,
  "decel": 16000
}
```

## NET:STATUS

`devices/8857212316bc/cmd`

```json
{
  "action": "NET:STATUS",
  "cmd_id": "7e6f5d4c-3b2a-1908-8796-a5b4c3d2e1f0"
}
```

`devices/8857212316bc/cmd/resp`

```json
{
  "cmd_id": "7e6f5d4c-3b2a-1908-8796-a5b4c3d2e1f0",
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

## MQTT:GET_CONFIG

`devices/8857212316bc/cmd`

```json
{
  "action": "MQTT:GET_CONFIG",
  "cmd_id": "d3c2b1a0-9f8e-7d6c-5b4a-3928170605f4"
}
```

`devices/8857212316bc/cmd/resp`

```json
{
  "cmd_id": "d3c2b1a0-9f8e-7d6c-5b4a-3928170605f4",
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
