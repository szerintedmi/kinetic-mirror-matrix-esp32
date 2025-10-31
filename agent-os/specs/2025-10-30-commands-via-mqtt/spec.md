# Specification: Commands via MQTT

## Goal
Deliver MQTT command handling with full parity to the serial protocol, using a unified command schema and response taxonomy so host tools can control nodes over MQTT without diverging behaviors.

## User Stories
- As a CLI/TUI operator, I want to send MOVE, HOME, STATUS, and auxiliary commands over MQTT so I can control remote nodes without attaching USB.
- As a firmware engineer, I want a single command execution pipeline that feeds both MQTT JSON responses and serial line output so I do not maintain two code paths.
- As a lab technician configuring installs, I want to fetch and update broker credentials via commands so I can switch networks without reflashing firmware.

## Core Requirements
### Functional Requirements
- Define a canonical command envelope (`cmd_id`, `action`, `params`, optional `meta`) and response schema (`status`, `result`, `errors`, `warnings`) that cover every current serial action (`HELP`, `STATUS`, `MOVE`, `HOME`, `WAKE`, `SLEEP`, `GET/SET THERMAL_LIMITING`, `GET LAST_OP_TIMING`, `NET:LIST`, `NET:RESET`, onboarding actions, etc.). Note: relocate the UUID helper currently exposed as `net_onboarding::NextMsgId()` into a shared transport module so both serial and MQTT can depend on it without pulling in onboarding code, then retire the legacy aliases.
- Accept commands on `devices/<node_id>/cmd` (QoS1). Requests may include a caller-supplied UUID `cmd_id`; when the field is missing or blank the firmware must allocate a fresh UUID via `transport::message_id::Next()` before processing. The canonical action and params object still mirror the serial argument set (e.g., `target_ids`, `position_steps`, `speed_sps`).
- Emit an immediate ACK on `devices/<node_id>/cmd/resp` (QoS1) with `{ "cmd_id": "...", "status": "ack", "warnings": [] }` whenever the action is accepted for execution. Invalid requests skip the ACK and go straight to a completion message indicating the error (see below).
- Emit a completion response on the same topic when execution finishes (success, error, or timeout) with `status: "done"|"error"|"busy"`, `result` payload (including `est_ms`, `actual_ms`, and command-specific data), and any structured warnings. Include `cmd_id` so clients correlate messages despite MQTT retries. Duplicate deliveries may be ignored once the command has already been processed.
- Keep firmware idempotent by tracking the most recent `cmd_id` per node; duplicate QoS1 deliveries return the already-determined outcome without re-running the command.
- Reuse the existing `MotorCommandProcessor` pipeline for validation and scheduling by adapting its `CommandResult`/`CommandExecutionContext` to emit normalized result objects consumed by both the serial formatter and the new MQTT JSON formatter.
- Preserve existing BUSY semantics. MQTT requests made while motion or cooldown blocks an action must return an immediate `status:"error"` completion populated with the existing serial error code (`E04 BUSY`).
- Update serial output to reference the shared error/status catalog (e.g., `CTRL:ERR code=E03 reason=BAD_PARAM`) to keep logs concise while aligning with the MQTT schema; maintain the existing line-based format.
- Extend `tools/serial_cli/mqtt_runtime.MqttWorker` to send command payloads, await ACK/finish pairs, mirror log output, and surface BUSY/ERROR states in the TUI and CLI exactly as serial mode does today. Preserve manual `--transport serial` override without introducing auto-fallback.
- Maintain CLI/TUI command queues so user-issued actions are serialized, ACK timeouts surface to users, and telemetry polling continues concurrently. When an operator enters `;`-separated actions, the CLI/TUI MUST emit one MQTT publish per action (preserving order) rather than batching them into a single multi-command request.
- After command parity ships, add `MQTT:GET_CONFIG` and `MQTT:SET_CONFIG host= port= user= pass=` commands (serial and MQTT entry points) that store values in Preferences with compile-time defaults used when unset. Both commands must participate in the same JSON/line encoding scheme as other actions.

### Non-Functional Requirements
- MQTT command handling must respond with an ACK within 100 ms on the bench (ESP32 loop + network) for accepted actions, and deliver completion messages within 10 ms of motion completion or rejection.
- All publish/subscribe paths use QoS1 with retained=false and keep heap allocations deterministic by reusing static buffers sized for worst-case JSON payloads (<1.5 kB).
- Firmware must guard against QoS1 replays by tracking the last N (≥8) processed `cmd_id` values, preventing duplicate execution, and emitting a rate-limited `CTRL:INFO MQTT_DUPLICATE cmd_id=<...>` (or equivalent) when a duplicate is ignored.
- Host tooling must handle broker disconnects by surfacing immediate publish failures to the operator while allowing MQTT QoS1 to handle retransmission; no bespoke retry loops should block the UI thread.

## Visual Design
No visual assets provided; reuse existing CLI/TUI layouts for command log panes and status tables.

## Reusable Components
### Existing Code to Leverage
- `lib/MotorControl` command stack (`CommandParser`, `CommandRouter`, `CommandExecutionContext`, `CommandResult`) for action validation, scheduling, and existing error codes.
- `lib/transport/MessageId` for UUID generation, storage of the active `cmd_id`, and integration with serial immediate print helpers.
- `lib/transport/MessageId` for UUID generation, storage of the active `cmd_id`, and integration with serial immediate print helpers (previously surfaced via `net_onboarding::NextMsgId`, now retired).
- `lib/mqtt_presence` + `lib/mqtt_status` (`AsyncMqttPresenceClient`, `PublishMessage`, shared publish queue) for AsyncMqttClient lifecycle, topic normalization, and payload batching.
- `tools/serial_cli/runtime.SerialWorker` for command queue semantics, ACK tracking, and log formatting patterns that MQTT mode must emulate.
- `tools/serial_cli/mqtt_runtime.MqttWorker` telemetry subscription scaffold, reconnect backoff, and column rendering so command responses slot into existing data caches.

### New Components Required
- Firmware-side `mqtt::MqttCommandServer` (name TBD) that subscribes to the command topic, validates envelopes, invokes `MotorCommandProcessor::execute`, and formats ACK/final JSON responses using the consolidated schema.
- A shared `CommandFormatter` utility that converts `CommandResult` objects into both serial line strings and JSON structures, ensuring parity while allowing richer metadata on MQTT.
- CLI-side `MqttCommandClient` helper layered onto `MqttWorker` to publish command payloads, await ACK/completion futures, and feed logs/TUI updates without blocking telemetry.
- Preferences-backed broker settings module extending `net_onboarding::NetOnboarding` or a new `mqtt::ConfigStore`, with serialization helpers used by the forthcoming configuration commands.

## Technical Approach
- Database: Store MQTT configuration overrides in ESP32 Preferences (`namespace="mqtt"`) with versioning per `data-persistence` standard; keep a RAM cache guarded by the existing net onboarding mutex, and apply atomic write swaps when settings change.
- API: Topics `devices/<mac>/cmd` (QoS1) for requests and `devices/<mac>/cmd/resp` (QoS1) for ACK + completion. Define JSON schema in `docs/mqtt-command-schema.md`, including enums for `status`, error codes, and per-action parameter objects; document parallel serial line formats referencing the same codes.
- Frontend: Update CLI command dispatch path so `mirrorctl --transport mqtt` and the TUI call a unified `CommandClient` interface; show ACK latency, completion status, and BUSY/ERROR codes inline with existing log panes. Ensure telemetry polling remains at 1 Hz / 5 Hz cadence by processing responses on the MQTT worker thread and emitting events to the UI loop.
- Testing: Add PlatformIO native tests covering JSON parsing, duplicate `cmd_id` handling, BUSY responses, and serialization symmetry between JSON and line-oriented outputs. Provide host-side tests that mock the MQTT client, verify command publish payloads, and assert that ACK/finish pairs update CLI state machines. Run hardware validation scripts that send MOVE/HOME sequences over MQTT, confirm BUSY behavior when issuing overlapping commands, and ensure the fallback configuration commands succeed via serial.

## Out of Scope
- Multi-node broadcast or master scheduling behavior (roadmap item 12+).
- QoS2 messaging, retained command requests, or MQTT topic wildcards beyond the single-node endpoints.
- Web UI command surfaces or REST bridges.

## Success Criteria
- Issuing `MOVE:ALL,2000` from the CLI in MQTT mode produces an ACK within 100 ms, a completion payload within the estimated motion time, and identical motor state updates to the serial transport.
- Concurrent command attempts while a move runs yield immediate `status:"error"` completions and leave the firmware motion schedule unaffected.
- Updating broker credentials via the new configuration commands persists to Preferences, survives a reboot, and allows reconnecting to the revised broker without reflashing.
