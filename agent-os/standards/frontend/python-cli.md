# Python CLI

## Transport Workers

- `SerialWorker` and `MqttWorker` implement the same interface via duck typing
- Shared methods: `queue_cmd()`, `get_state()`, `get_thermal_state()`, `get_net_info()`, `stop()`
- No shared base class yet (ABC/Protocol would be welcome)
- Default transport: MQTT (not serial)

## Command Timeout Amplification

- Base timeout: configurable, min 0.5s (default 2.0s)
- ACK reception **extends deadline** by another base_timeout window
- Streaming commands (NET:LIST, SCANNING) get 2-3x multipliers
- Poll commands use shorter idle grace (~50ms vs 100ms for user commands)
- No explicit DONE = implicit completion on idle timeout

## MQTT Device Selection Fallback

Cascade: explicit selection → `--node` arg → single device → freshest telemetry

- Stale device selection auto-cleared if device disappears
- TUI device targeting: `/N` (switch), `/N cmd` (temp switch), `/all cmd` (broadcast)
- Device index is 1-based in TUI

## Command Building

- Batch splitting: `;` delimiter, respects quoted strings
- Optional positional params: empty strings for skipped fields (`HOME:0,,100` = no overshoot, backoff=100)
- CSV parser handles quotes and escape sequences
- Colon syntax preferred; space syntax supported for legacy

## MQTT Worker Internals

- Pending commands triple-indexed: by cmd_id, by local handle, by FIFO order
- Silent commands return handle `-1` (not tracked, not logged)
- Thermal/microstep state stored per-device AND globally (last-device-wins for single-device compat)
