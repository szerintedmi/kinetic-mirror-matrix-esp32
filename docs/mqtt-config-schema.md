# MQTT Config Schema

Firmware publishes device configuration on `devices/<node_id>/config` where `<node_id>` is the device MAC address rendered in lowercase without separators. Publishes use QoS 0 and `retain=true` so new subscribers immediately receive the current configuration.

## Payload Structure

```json
{
  "thermal_limiting": "ON",
  "max_budget_s": 90,
  "microstep": "FULL",
  "microstep_mult": 1,
  "speed": 4000,
  "accel": 16000,
  "decel": 16000,
  "move_overshoot": 80,
  "dither_amplitude": 0,
  "dither_cycles": 3,
  "dither_min_amplitude": 20,
  "motor_count": 8,
  "uptime_ms": 123456789,
  "firmware_version": "9953783",
  "firmware_date": "2025-12-02T19:59:32+0000"
}
```

## Fields

| Field              | Type   | Description |
|--------------------|--------|-------------|
| `thermal_limiting` | string | Thermal limiting state: `"ON"` or `"OFF"`. |
| `max_budget_s`     | number | Maximum runtime budget per motor in seconds (constant from firmware). |
| `microstep`        | string | Current microstepping mode: `"FULL"`, `"HALF"`, `"1/4"`, `"1/8"`, `"1/16"`, or `"1/32"`. |
| `microstep_mult`   | number | Microstepping multiplier (1, 2, 4, 8, 16, or 32). |
| `speed`            | number | Default speed in steps per second. |
| `accel`            | number | Default acceleration in steps per second squared. |
| `decel`              | number | Default deceleration in steps per second squared. |
| `move_overshoot`     | number | Default overshoot in steps (sign = approach direction, 0 = disabled). |
| `dither_amplitude`   | number | Default dither oscillation amplitude in steps (0 = disabled). |
| `dither_cycles`      | number | Default number of dither oscillation cycles. |
| `dither_min_amplitude` | number | Minimum dither amplitude threshold; cycles stop when amplitude decays below this. |
| `motor_count`        | number | Number of motors connected to the device. |
| `uptime_ms`        | number | Milliseconds since device boot (`millis()`). |
| `firmware_version` | string | Git commit hash at build time (optional, requires `GIT_COMMIT_HASH` macro). |
| `firmware_date`    | string | Git commit date in ISO 8601 format (optional, requires `GIT_COMMIT_DATE` macro). |

## Publish Triggers

The config topic is published:

1. **On MQTT connect** - with `retain=true` so the broker stores the current config
2. **On config change** - after any `SET` command modifies speed, accel, decel, thermal_limiting, microstep, move_overshoot, dither_amplitude, dither_cycles, or dither_min_amplitude

Hash-based deduplication ensures duplicate payloads are not published.

## Relationship to Status Topic

The config topic (`devices/<node_id>/config`) contains session-level settings that change infrequently, while the status topic (`devices/<node_id>/status`) contains real-time motor telemetry that updates at 1-5 Hz.

| Topic | Cadence | Content |
|-------|---------|---------|
| `.../status` | 1 Hz idle, 5 Hz motion | Per-motor positions, moving state, thermal budget |
| `.../config` | On change only | Global settings: microstep, thermal flag, speed/accel/decel defaults, firmware info, uptime |

This separation allows subscribers to efficiently track motor state without repeated config data in every status update.

## Example Subscribe (Python)

```python
import paho.mqtt.client as mqtt
import json

def on_config(client, userdata, msg):
    config = json.loads(msg.payload)
    print(f"Thermal: {config['thermal_limiting']}")
    print(f"Microstep: {config['microstep']}")
    print(f"Motors: {config.get('motor_count', 'N/A')}")
    print(f"Firmware: {config.get('firmware_version', 'N/A')}")

client = mqtt.Client()
client.on_message = on_config
client.connect("192.168.1.25", 1883)
client.subscribe("devices/+/config", qos=0)
client.loop_forever()
```
