# Task 3: CLI/TUI MQTT Command Support

## Overview
**Task Reference:** Task #3 from `agent-os/specs/2025-10-30-commands-via-mqtt/tasks.md`  
**Implemented By:** ui-designer  
**Date:** 2025-11-01  
**Status:** ⚠️ Partial

### Task Description
Bring the Python host tooling (CLI and TUI) to feature parity with the unified MQTT command pipeline: publish one action per MQTT message, surface ACK/complete states with latency metrics, keep telemetry polling independent, and extend tests to cover the new command client behaviour.

## Implementation Summary
Extended the MQTT worker to translate semicolon-delimited serial syntax into discrete JSON publishes, track pending commands keyed on `cmd_id`, and log structured ACK/DONE responses emitted by the dispatcher. The Serial worker now shares the same event parser so both transports consume dispatcher events uniformly. The TUI wiring was refreshed to read the MQTT worker’s log/latency updates without blocking the telemetry table, and the CLI defaults to MQTT while retaining the serial override.

Added host-side regression tests that mock MQTT publishes, assert per-command payload generation, exercise duplicate replay handling, and verify log output for ACK/DONE sequences. Documentation in `docs/mqtt-command-schema.md` now calls out the host tooling contract that mirrors the firmware schema. The remaining documentation update for forthcoming configuration commands is tracked under Task 3.6.

## Files Changed/Created

### New Files
- _None_

### Modified Files
- `tools/serial_cli/mqtt_runtime.py` – Implements publish helper, pending-command tracking, duplicate replay handling, and exposes command/device summaries for UI consumers.
- `tools/serial_cli/command_builder.py` – Ensures semicolon-delimited inputs split into single-action requests consumed by MQTT publishing.
- `tools/serial_cli/response_events.py` – Normalizes dispatcher ACK/DONE parsing for both transports and formats log output with latency hints.
- `tools/serial_cli/runtime.py` – Feeds Serial responses through structured event parsing for parity with the MQTT mode.
- `tools/serial_cli/__init__.py` – Updates CLI defaults/options to favour MQTT transport and reuses shared parsers.
- `tools/serial_cli/tui/textual_ui.py` – Streams MQTT worker logs, renders latency metadata, and maintains telemetry refresh cadence.
- `tools/serial_cli/tests/test_mqtt_runtime.py` – Adds focused tests covering command publish, ACK/DONE aggregation, duplicate replay, and net-info propagation.
- `docs/mqtt-command-schema.md` – Documents host CLI/TUI behaviour against the unified schema.

### Deleted Files
- _None_

## Key Implementation Details

### MQTT Command Queue & Publish Helper
**Location:** `tools/serial_cli/mqtt_runtime.py`

Introduced `MqttWorker.queue_cmd()` to call `build_requests()` and publish one MQTT JSON payload per command with QoS1, capturing local handles and wiring ACK/finish events back to the UI. Pending commands maintain timestamps so ACK latency and completion latency can be rendered in the log stream.

**Rationale:** Guarantees single-action MQTT publishes while mirroring the serial CLI UX, satisfying Tasks 3.2 and 3.3. Host-side command IDs are now generated up front so ACK/finish events correlate deterministically and latency measurements exclude unrelated command traffic. Stream caching now relies solely on dispatcher events to avoid duplicating warnings in MQTT payloads.

### Pending Command Tracking & Duplicate Handling
**Location:** `tools/serial_cli/mqtt_runtime.py`

Extended `PendingCommand` records to store events, latencies, and completion markers. `_handle_event` associates dispatcher responses by `cmd_id`, logs formatted events, updates net-state metadata, and honours `CTRL:INFO MQTT_DUPLICATE` notifications without requeueing commands.

**Rationale:** Provides parity with firmware duplicate suppression and surfaces BUSY/ERROR states, covering Tasks 3.3 and 3.4.

### CLI/TUI Integration
**Location:** `tools/serial_cli/__init__.py`, `tools/serial_cli/tui/textual_ui.py`

CLI default transport switches to MQTT while retaining serial overrides; TUI panels poll worker state and append formatted logs without stalling telemetry updates. Help text and interactive bindings ensure operators see command progress and latency metrics inline with status data.

**Rationale:** Delivers Task 3.5 requirements by integrating command updates into both CLI and Textual UI flows.

## Database Changes (if applicable)
Not applicable.

## Dependencies (if applicable)
No new external dependencies introduced; continues using `paho-mqtt`, `pyserial`, and `textual` where available.

## Testing

### Test Files Created/Updated
- `tools/serial_cli/tests/test_mqtt_runtime.py` – Validates command publish helper, ACK/DONE event handling, duplicate replay, and net-info caching.

### Test Coverage
- `pytest tools/serial_cli/tests/test_mqtt_runtime.py`

Tests confirm enqueue/publish behaviour, event logging, and state updates without exercising unrelated suites.

## Outstanding Work
- Task 3.6 (documentation refresh for CLI help + upcoming config commands) remains open and is tracked in `tasks.md`.
