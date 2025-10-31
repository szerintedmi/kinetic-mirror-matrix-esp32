# Unified Command Response Refactor Plan

Goal: eliminate protocol-specific response handling, support async serial completions, and unify MQTT/serial response flow around a single structured contract.

## Phase 1 — Define Canonical Response Model *(Status: ✅ Complete)*
- [x] Capture all existing response semantics (ACK, DONE, WARN, ERROR, result data) in a structured C++ schema (`CommandResponse`, `CommandEvent`, etc.).  
  *Implemented in* `lib/transport/include/transport/ResponseModel.h`, `lib/transport/src/ResponseModel.cpp`.
- [x] Document the contract (fields, status transitions, required metadata) so firmware, MQTT, and tooling share the same expectations.  
  *References:* this plan; design note in `docs/response-dispatcher-design.md`.

## Phase 2 — Build Shared Dispatcher & Formatters *(Status: ✅ Complete)*
- [x] **Design dispatcher:** document queue semantics, event lifecycle (ACK → DONE), duplicate suppression, and transport subscribers.  
  *Design doc:* `docs/response-dispatcher-design.md`.
- [x] Implement a transport-agnostic dispatcher that emits response events asynchronously for serial and MQTT.  
  *Code:* `lib/transport/include/transport/ResponseDispatcher.h`, `lib/transport/src/ResponseDispatcher.cpp`.
- [x] Implement a serial formatter that renders dispatcher events into the legacy `CTRL:*` lines (ensuring no behaviour change).  
  *Current scope:* `serial_console_setup()` now registers a dispatcher sink that feeds `EventToLine` output through `Serial.println`, and `serial_console_tick()` ticks `transport::response::CompletionTracker` so MOVE/HOME `EventType::kDone` emissions appear as `CTRL:DONE`. Commands that still return legacy strings continue to flow through `motor::command::FormatForSerial` until their handlers emit dispatcher events.
- [x] Expose dispatcher cache replay for duplicate suppression.  
  *Implemented:* new `ResponseDispatcher::Replay(cmd_id, cb)` lets transports request cached ACK/DONE events, enabling duplicate handling without re-parsing legacy strings.
- [x] Implement an MQTT formatter that converts the same events into JSON payloads.  
  *Status:* `MqttCommandServer` now registers a dispatcher sink, aggregates `transport::response::Event`s, and publishes ACK/DONE JSON directly from those events (with dispatcher-backed duplicate replay). No command handlers produce transport-specific strings anymore; remaining cleanup is confined to shared helpers tracked in Phase 3/4.

## Phase 3 — Refactor Command Producers *(Status: ✅ Complete)*
- [x] Update handlers (MOVE, HOME, NET, GET/SET, STATUS, etc.) to populate the new response structures and enqueue events via the dispatcher.  
  *Current scope:* MOVE, HOME, STATUS, GET/SET, WAKE/SLEEP, NET, and the multi-command batch executor (`lib/MotorControl/src/command/CommandBatchExecutor.cpp`) now emit structured events. All command-level string emitters have been replaced; remaining work targets shared helpers captured in Phase 4.
- [x] Ensure long-running commands enqueue both ACK and completion events; short commands emit a single DONE event.  
  *Status:* `transport::response::CompletionTracker` now watches MOVE/HOME via `lib/transport/src/CompletionTracker.cpp`, with `serial_console_tick()` and `MqttCommandServer::loop()` driving the tick; no commands rely on legacy completion strings.

## Phase 4 — Retire Legacy Parsing Paths & Simplify Transport Stack *(Status: ✅ Complete)*
- [x] Delete redundant parsing code from `CommandResult`, `ResponseFormatter`, and `MqttCommandServer` now that all handlers emit structured events.  
  *Outcome:* `CommandResult` now stores only structured responses; serial fallback printing was removed; MQTT no longer reparses `CTRL:*` lines.
- [x] Excise legacy helper utilities.  
  *Outcome:* `transport::CommandSchema` no longer exposes `ParseCommandResponse`; other reverse-parsing helpers were removed alongside string-mode code paths.
- [x] Break up overgrown transport/command classes to reduce complexity.  
  *Outcome:* `MqttCommandServer` now routes through discrete helpers (`handleDuplicateCommand`, `makeDispatch`, `executeDispatch`, `respondWithError`), with structured payload builders operating on dispatcher data.
- [x] Decompose monolithic transport handlers.  
  *Outcome:* MQTT request flow is split into parsing, dispatch execution, stream processing, and payload publication; serial processing now relies entirely on dispatcher events, allowing further decomposition in future iterations.
- [x] Reorganize the MQTT/transport command stack.  
  *Outcome:* Structured response handling lives in `transport::response`; MQTT code consumes those interfaces directly, aligning `lib/mqtt_commands` with `lib/transport` responsibilities.

## Phase 5 — Transport Command Builder Decomposition *(Status: ✅ Complete)*
- [x] Break `buildCommandLine()` into action-specific builders.  
  *Result:* action routing now delegates to `buildMoveCommand`, `buildHomeCommand`, `buildWakeSleepCommand`, `buildNetCommand`, `buildGetCommand`, and `buildSetCommand`, cutting the method CCN dramatically.
- [x] Extract shared motor-target parsing utilities.  
  *Result:* new helpers (`parseMotorTargetSelector`, `parseIntegerField`) collapse the repeated MOVE/HOME/WAKE/SLEEP selector logic and numeric validation.
- [x] Separate NET command payload handling.  
  *Result:* Wi-Fi payload parsing now lives inside `buildNetCommand`, keeping motor-specific codepaths isolated.
- [x] Add builder-focused unit coverage.  
  *Result:* Added `test_wake_missing_target_ids` to ensure validation errors surface via the new builder flow; existing suites continue to exercise the happy paths.

## Phase 6 — Update Tooling, Tests, and Docs *(Status: ⏳ Not Started)*
- [ ] Refresh `tools/serial_cli` to consume dispatcher events directly.  
  *Focus:* surface ACK/DONE transitions, streaming INFO/DATA payloads, and response metadata (msg_id, action, status) instead of assuming single-line `CTRL:*` strings.
- [ ] Extend CLI command coverage to match firmware parity.  
  *Focus:* ensure MOVE/HOME batches, NET actions, GET/SET variants, and motion completion tracking work identically over serial and MQTT transports.
- [ ] Add host-side regression coverage for the new CLI flows.  
  *Focus:* snapshot tests or golden transcripts that validate ACK/DONE sequencing, async completions, error propagation, and duplicate replay handling.
- [ ] Update host MQTT clients / TUI tooling to leverage structured responses.  
  *Focus:* replace string parsing with structured payload handling and expose dispatcher-derived fields (est_ms, actual_ms, warnings/errors).
- [ ] Refresh documentation/specs to describe the unified response flow and updated tooling expectations.  
  *Docs:* `docs/mqtt-command-schema.md`, CLI usage guides, relevant `agent-os/specs/*` entries.

## Execution Notes
- Focus next on eliminating the last legacy formatting pathways (`CommandResult` string mode, `FormatForSerial`) so the dispatcher contract becomes the lone source of truth.
- Maintain behavioural parity on serial during transition; once CLI tooling consumes dispatcher events directly, remove the `FormatForSerial` fallback.
- MQTT dispatcher sink now binds `msg_id`→`cmd_id` so ACK/DONE events publish JSON directly; remaining fallbacks exist only in shared helpers (see Phase 4 tasks).
- SET handlers now emit dispatcher `DONE` events (no interim ACK), consolidating serial `CTRL:DONE … status=done` output with the MQTT completion payload.
- NET command responses now flow through the dispatcher; `NET:SET`/`NET:RESET` still ACK when asynchronous follow-ups are expected, while status/list actions emit immediate `DONE`. ESP32 network state transitions emit structured INFO/ERROR events and finalize outstanding commands with dispatcher `DONE` events.
- Event lifecycle guidance: use `ACK` when additional responses are expected (command accepted but still running), and emit `DONE` once execution is complete and no further responses will follow; this ensures both serial `CTRL:DONE` lines and MQTT completions align.
- Regression coverage: native MQTT suite now exercises HOME completions (`test_home_command_success`) and the structured GET/SET flow (`test_get_*`, `test_set_speed_command_success`), guarding dispatcher-format parity alongside serial estimates while ensuring empty-result completions omit `result`.
- Further simplification opportunities: pursue dispatcher module consolidation, deeper MQTT server decomposition, and richer `Data` event contracts (see `docs/response-dispatcher-design.md` “Potential Improvements”).
- Duplicate hotspots identified by static analysis:
  - Remaining duplication largely lives in motor command handlers (mask parsing, ACK/DONE scaffolding). A future pass can extract these patterns now that transport-side builders are modular.
- Terminology cleanup: renamed `transport::command::Line` to `transport::command::ResponseLine` and updated related helpers so the naming reflects their role in the response pipeline.
