# Command Response Dispatcher Design

## Overview
The dispatcher is now the single conduit for command responses. Command handlers emit structured `transport::response::Event`s and the `ResponseDispatcher` fans them out to every interested transport (currently serial and MQTT). Legacy string plumbing has been removed: serial output is produced only from dispatcher events, and transports never re-parse `CTRL:*` strings. `motor::command::CommandResult` now carries only structured responses, eliminating dual-mode (`lines` vs. structured) behaviour.

## Core Components
- **Event**: `{ type: Ack|Done|Warn|Error|Info|Data, cmd_id, action, code, reason, attributes }`.
- **CommandResponse** (`transport::response::CommandResponse`): aggregation derived from emitted events (captures `cmd_id`, `est_ms`, `actual_ms`, and ordered event log).
- **ResponseDispatcher**:
  - Emits every event immediately to registered sinks.
  - Provides `Replay(cmd_id, cb)` so transports can satisfy duplicate requests without re-running handlers.
- **CompletionTracker**: Watches long-running commands (MOVE/HOME) and emits synthetic `Done` events when motion settles.
- **Serial sink**: Registered in `serial_console_setup()`; converts each event to a `transport::command::ResponseLine` via `EventToLine` and prints the `CTRL:*` text. No other serial fallback remains.
- **Naming update:** the schema formerly referred to these structures as `command::Line`; after the refactor they’re `command::ResponseLine`, clarifying that they represent response payloads (ACK/WARN/ERROR/INFO/DATA) rather than command inputs.
- **MQTT sink**: Implemented inside `mqtt::MqttCommandServer`. Dispatcher events keep per-command streams in sync with published ACK/DONE payloads, removing the need to reparse strings.

## Flow
1. Command handlers build structured control/data lines using `EventToLine` helpers and call `ResponseDispatcher::Emit`.
2. For asynchronous operations, handlers emit an initial `Ack`. `CompletionTracker` observes controller state and emits a `Done` event when work finishes.
3. Dispatcher broadcasts events to sinks. Each sink decides how to render/store them.
4. MQTT paths rely exclusively on dispatcher data:
   - Incoming command is executed once.
   - Streams consult dispatcher events to publish ACK / completion payloads (or replay cached ones on duplicates).
5. Serial console no longer inspects `CommandResult`; it simply waits for dispatched events to be printed.

## Transport Interaction
- Sinks register lambdas with `ResponseDispatcher::RegisterSink`. We currently register:
  - Serial sink (Arduino `Serial.println`).
  - MQTT sink (internal `DispatchStream` bookkeeping).
- Sinks process the full event stream and may cache results. MQTT’s stream caches ACK/completion payloads per `cmd_id` for duplicate handling and late data binding.
- Structured payload generation (`buildAckPayload`, `buildCompletionPayload`) now works directly off `transport::command::Response` objects produced by handlers.

## Threading / Timing
- ESP32 environment: dispatcher emits synchronously from command handlers. `serial_console_tick()` and `MqttCommandServer::loop()` drive `CompletionTracker::Tick(now_ms)` so asynchronous completions materialize.
- MQTT duplicate replay uses dispatcher cache without rerunning handlers.
- No background polling API is required beyond the existing `Tick`.

## Current Coverage
- All command handlers now emit structured responses only.
- `CommandResult` carries structured lines exclusively; legacy `lines`, `appendRaw`, and `FormatForSerial` fallbacks are gone.
- Serial console writes dispatcher output only.
- MQTT command server is fully event-driven and no longer reparses `CTRL:*` text. Duplicate handling, ACK replay, and completion synthesis are handled via structured data.
- Unit suites (`native`) cover the full matrix: motor control, MQTT command server, and shared utilities validate structured flows end-to-end.

## Open Questions
- Should we formalise richer event lifecycles (e.g. explicit “queued” / “progress” states) for transports that want streaming progress?
- How do we want to expose bulk data (e.g. STATUS tables) over MQTT—structured JSON objects, streamed `Data` events, or both?
- What contracts should we guarantee when sinks fail (MQTT publish rejected, serial buffer overflow)? Today we log but do not retry.

## Potential Improvements & Next Steps
1. **Dispatcher module boundaries** – the dispatcher, completion tracker, and response model currently live across several files. Consider pulling them under a single `transport/response/` module with clearer namespaces and a thin facade.
2. **MQTT command server decomposition** – while the largest legacy branches are gone, `MqttCommandServer` still has high cyclomatic complexity across command parsing and payload builders. Splitting it into parser, executor, and transport adapters would make future transports easier to add.
3. **Unified data-channel contract** – define a schema for `Data` events so both serial and MQTT can emit structured tables without bespoke handling.
4. **Backpressure & buffering strategy** – today sinks process synchronously. Introducing bounded queues or coroutine-style processing could make it easier to add transports with heavier IO (e.g. BLE, WebSockets).
5. **Test scaffolding for dispatcher sinks** – create targeted unit tests for sink registration, replay semantics, and completion tracker behaviour to guard against regressions when refactoring.

### Transport Command Builder Outlook
- `buildCommandLine()` has been decomposed into dedicated helpers (`buildMoveCommand`, `buildHomeCommand`, `buildWakeSleepCommand`, `buildNetCommand`, `buildGetCommand`, `buildSetCommand`) backed by shared utilities (`parseMotorTargetSelector`, `parseIntegerField`). This trims the cyclomatic complexity drastically and eliminates the duplicated validation blocks highlighted by static analysis.
- Follow-on opportunities:
  1. Lift the new helpers into a standalone builder module (or files) so other transports can reuse them without depending on `MqttCommandServer` internals.
  2. Formalise a typed `BuildResult` struct to cleanly pipe both the serial façade and future transports through the same contracts.
  3. Extend unit coverage to additional edge cases (e.g. NET credentials with Unicode SSIDs, GET resource aliases) as we onboard new clients.
- With this foundation, MQTT action handling fully aligns with the dispatcher’s structured contract and is ready to be shared with future transport adapters.

### Other Duplicate Hotspots
- Motor command handlers still repeat mask parsing, warning/ACK scaffolding, and motion-specific checks across MOVE/HOME/WAKE/SLEEP. A later pass can pull these into shared helpers once transport-side builders are simplified.
- With legacy parsing removed, `transport::CommandSchema` is lean, but remaining serialization helpers (e.g. repeated field-to-line conversions) could migrate into a consolidated utility to avoid future duplication.

We can spin these items into a follow-up plan, prioritising the `MqttCommandServer` decomposition and dispatcher module consolidation to keep complexity trending down.
