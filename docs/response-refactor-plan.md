# Unified Command Response Refactor Plan

Goal: eliminate protocol-specific response handling, support async serial completions, and unify MQTT/serial response flow around a single structured contract.

## Phase 1 — Define Canonical Response Model *(Status: ✅ Complete)*
- [x] Capture all existing response semantics (ACK, DONE, WARN, ERROR, result data) in a structured C++ schema (`CommandResponse`, `CommandEvent`, etc.).  
  *Implemented in* `lib/transport/include/transport/ResponseModel.h`, `lib/transport/src/ResponseModel.cpp`.
- [x] Document the contract (fields, status transitions, required metadata) so firmware, MQTT, and tooling share the same expectations.  
  *References:* this plan; design note in `docs/response-dispatcher-design.md`.

## Phase 2 — Build Shared Dispatcher & Formatters *(Status: ⚠️ In Progress)*
- [x] **Design dispatcher:** document queue semantics, event lifecycle (ACK → DONE), duplicate suppression, and transport subscribers.  
  *Design doc:* `docs/response-dispatcher-design.md`.
- [x] Implement a transport-agnostic dispatcher that emits response events asynchronously for serial and MQTT.  
  *Code:* `lib/transport/include/transport/ResponseDispatcher.h`, `lib/transport/src/ResponseDispatcher.cpp`.
- [x] Implement a serial formatter that renders dispatcher events into the legacy `CTRL:*` lines (ensuring no behaviour change).  
  *Current scope:* `serial_console_setup()` now registers a dispatcher sink that feeds `EventToLine` output through `Serial.println`, and `serial_console_tick()` ticks `transport::response::CompletionTracker` so MOVE/HOME `EventType::kDone` emissions appear as `CTRL:DONE`. Commands that still return legacy strings continue to flow through `motor::command::FormatForSerial` until their handlers emit dispatcher events.
- [x] Expose dispatcher cache replay for duplicate suppression.  
  *Implemented:* new `ResponseDispatcher::Replay(cmd_id, cb)` lets transports request cached ACK/DONE events, enabling duplicate handling without re-parsing legacy strings.
- [~] Implement an MQTT formatter that converts the same events into JSON payloads.  
  *Progress:* `MqttCommandServer` now registers a dispatcher sink, aggregates `transport::response::Event`s, and publishes ACK/DONE JSON directly from those events (with dispatcher-backed duplicate replay); legacy payload construction remains as a fallback until non-dispatcher handlers are migrated.

## Phase 3 — Refactor Command Producers *(Status: ⚠️ In Progress)*
- [~] Update handlers (MOVE, HOME, NET, GET/SET, STATUS, etc.) to populate the new response structures and enqueue events via the dispatcher.  
  *Current scope:* MOVE, HOME, STATUS, and GET/SET emit structured events in `lib/MotorControl/src/command/CommandHandlers.cpp`; WAKE/SLEEP and NET handlers still fall back to legacy strings via `CommandResult`.
- [~] Ensure long-running commands enqueue both ACK and completion events; short commands emit a single DONE event.  
  *Progress:* `transport::response::CompletionTracker` now watches MOVE/HOME via `lib/transport/src/CompletionTracker.cpp`, with `serial_console_tick()` and `MqttCommandServer::loop()` driving the tick; other commands still bypass the dispatcher.
- [ ] Remove ad-hoc string helpers that are no longer needed once handlers emit structured data.  
  *Targets:* `lib/MotorControl/src/command/CommandHandlers.cpp`, `CommandUtils.cpp`.

## Phase 4 — Retire Legacy Parsing Paths *(Status: ⏳ Not Started)*
- [ ] Delete redundant parsing code from `CommandResult`, `ResponseFormatter`, and `MqttCommandServer` once all handlers use the dispatcher.  
  *Blocker:* legacy fallbacks still active in `CommandResult` / `FormatForSerial` and MQTT server.
- [ ] Remove unused helpers in `transport::CommandSchema` that existed solely for reverse-parsing.  
  *Pending cleanup* after Phase 3 migration.

## Phase 5 — Update Tooling, Tests, and Docs *(Status: ⏳ Not Started)*
- [ ] Update unit and integration tests to validate structured responses and async serial behaviour.  
  *To plan:* expand native tests + host-side coverage once dispatcher drives all responses.
- [ ] Update CLI/TUI and host tooling to consume the unified response contract.  
  *Affected:* `tools/serial_cli`, host MQTT clients.
- [ ] Refresh documentation/specs to reflect the new response flow (serial + MQTT).  
  *Docs:* `docs/mqtt-command-schema.md`, relevant `agent-os/specs/*` entries.

## Execution Notes
- Migrate command families incrementally: next focus is `NetCommandHandler`, power (WAKE/SLEEP), and remaining niche actions.
- Maintain behavioural parity on serial during transition; the dispatcher-backed serial formatter must mirror current output until all commands are migrated.
- MQTT dispatcher sink now binds `msg_id`→`cmd_id` so ACK/DONE events publish JSON directly; keep legacy `buildAckPayload`/`buildCompletionPayload` path only as a fallback until remaining handlers emit structured events.
- SET handlers now emit dispatcher `DONE` events (no interim ACK), consolidating serial `CTRL:DONE … status=done` output with the MQTT completion payload.
- Event lifecycle guidance: use `ACK` when additional responses are expected (command accepted but still running), and emit `DONE` once execution is complete and no further responses will follow; this ensures both serial `CTRL:DONE` lines and MQTT completions align.
- Track progress by checking off tasks per phase; do not remove legacy parsing until every handler and transport is converted.
- Regression coverage: native MQTT suite now exercises HOME completions (`test_home_command_success`) and the structured GET/SET flow (`test_get_*`, `test_set_speed_command_success`), guarding dispatcher-format parity alongside serial estimates while ensuring empty-result completions omit `result`.
