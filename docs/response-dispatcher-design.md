# Command Response Dispatcher Design (Draft)

## Overview
Create a transport-agnostic dispatcher that accepts structured `CommandEvent`s from command handlers and fans them out to all interested transports (serial, MQTT). This replaces ad-hoc synchronous emission in handlers.

## Core Components
- **CommandEvent**: `{ type: Ack|Done|Warn|Error|Info|Data, cmd_id, action, code, reason, attrs }`
- **CommandResponse**: aggregation of events plus derived fields (`est_ms`, `actual_ms`, duplicates cache).
- **ResponseDispatcher**:
  - Queues events per `cmd_id`.
  - Emits events to registered sinks immediately (ack) or after completion (done).
  - Maintains duplicate suppression (replay ACK/DONE on duplicate cmd_id).

## Flow
1. Handler invokes `dispatcher.emit(Event{Ack, ...})` when a command is accepted; optional for synchronous commands (can jump to Done).
2. Long-running operations store context; upon completion they emit `Event{Done, ...}`.
3. Dispatcher forwards events to subscribed transports:
   - **SerialSink**: renders events via legacy `CTRL:*` formatter and writes to `Serial`.
   - **MqttSink**: formats JSON payloads and publishes via `AsyncMqttClient`.
4. Dispatcher caches last ACK/DONE per `cmd_id` for duplicate replay (MQTT requirement).

## Transport Interaction
- Sinks register callbacks with dispatcher.
- Sinks declare interest in certain event types (e.g. serial only needs Ack/Done/Warn/Error, not Data).

## Threading / Timing
- ESP32 environment: dispatcher invoked from main loop; ensure events flush in `serial_console_tick()` and `MqttCommandServer::loop()`.
- Provide `dispatcher.poll(now_ms)` to flush delayed completions if needed.

## Migration Strategy (MOVE first)
- Update MOVE handler to emit events through dispatcher instead of returning strings.
- Serial console subscribes to dispatcher and prints events.
- MQTT server subscribes to dispatcher and publishes events.
- Existing tests can still parse printed output; add dispatcher-oriented tests.

## Open Questions
- Do we need explicit event lifecycle states (Queued → Acked → Done) or is simple event emission sufficient?
- How do we handle multi-line data (e.g. STATUS) — separate `Data` events or embed into Done?
- Preferred error handling when sink fails (e.g. MQTT publish failure).

