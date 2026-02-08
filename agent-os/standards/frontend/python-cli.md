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

## Adding or Changing Commands/Settings (Checklist)

When adding new GET/SET keys, command parameters, or modifying existing ones, **all of the following** must be updated. Missing any one layer causes silent failures in specific transports.

### Code layers

1. **Firmware command handler** (`CommandHandlers.cpp`) — parses the serial command string
2. **MQTT command translator** (`MqttCommandServer.cpp`) — `buildSetCommand()`, `buildGetCommand()`, or the relevant `build*Command()` method must whitelist and validate the new fields. MQTT commands pass through `MqttCommandServer::buildCommandLine()` which translates JSON→serial; if a field isn't whitelisted here, MQTT silently rejects with "unsupported field/resource" while serial works fine.
3. **Python command builder** (`command_builder.py`) — parses TUI input into MQTT JSON params
4. **Serial HELP text** (`HelpText.cpp`) — update the command grammar shown by `HELP`
5. **TUI help overlay** (`textual_ui.py`) — the help panel shown in the interactive TUI

### Documentation

6. **`docs/mqtt-command-schema.md`** — MQTT command/response schema (params tables, GET resources, SET params, serial mapping appendix)
7. **`docs/mqtt-payload-examples.md`** — real-world payload examples (add examples for new commands/params, update GET ALL response)
8. **`docs/mqtt-config-schema.md`** — config topic schema (if the setting appears in the broadcasted config JSON)
9. **`MqttConfigPublisher.cpp`** — if the new setting should be published on the config topic (builds JSON manually, append before `motor_count`)

## MQTT Worker Internals

- Pending commands triple-indexed: by cmd_id, by local handle, by FIFO order
- Silent commands return handle `-1` (not tracked, not logged)
- Thermal/microstep state stored per-device AND globally (last-device-wins for single-device compat)
