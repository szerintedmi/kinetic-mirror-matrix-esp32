# Task 2: CLI & TUI MQTT Transport

## Overview
**Task Reference:** Task #2 from `agent-os/specs/2025-10-29-mqtt-steel-thread-presence/tasks.md`  
**Implemented By:** ui-designer  
**Date:** October 29, 2025  
**Status:** ✅ Complete

### Task Description
Deliver MQTT transport support in the host CLI/TUI so presence updates can be monitored over the broker while clearly signalling that command verbs remain serial-only. Implementation must cover argument parsing, runtime subscriptions, documentation, and focused host-side tests.

## Implementation Summary
Extended the CLI to surface a first-class MQTT transport, sharing defaults with firmware and ensuring both the interactive TUI and one-shot invocations consume broker presence data. The new `MqttWorker` mirrors the serial worker surface, providing duplicate suppression, age tracking, and transport metadata while cleanly handling missing dependencies.

Updated the Textual TUI to auto-configure its status table based on the active worker, render MQTT broker context, and disable command entry in presence-only mode. Documentation now explains how to install dependencies and run the CLI with `--transport mqtt`. Added targeted unit tests to cover presence rendering, duplicate suppression, transport selection wiring, and user feedback when commands are invoked over MQTT.

## Files Changed/Created

### New Files
- `tools/serial_cli/mqtt_runtime.py` — Implements the threaded MQTT presence subscriber consumed by CLI/TUI runners.
- `test/test_Serial_CLI/test_cli_mqtt.py` — Host-side tests covering MQTT transport behaviour and logging.
- `agent-os/specs/2025-10-29-mqtt-steel-thread-presence/implementation/2-cli-tui-mqtt-transport-implementation.md` — This task documentation.

### Modified Files
- `tools/serial_cli/__init__.py` — Integrates MQTT worker, table rendering, and transport routing for commands/status.
- `tools/serial_cli/tui/textual_ui.py` — Adapts the interactive TUI to MQTT presence mode and dynamic columns.
- `README.md` — Documents MQTT transport usage and dependency requirements.
- `agent-os/specs/2025-10-29-mqtt-steel-thread-presence/tasks.md` — Marks Task Group 2 items complete.

### Deleted Files
- _None_

## Key Implementation Details

### MQTT Presence Worker
**Location:** `tools/serial_cli/mqtt_runtime.py`

Implements a daemon thread around `paho-mqtt` that subscribes to `devices/+/state`, parses payload key-value pairs into normalized rows, tracks message ages, and records log lines only for new `msg_id` values. Provides `get_state`, `get_net_info`, `queue_cmd`, and `get_table_columns` so the worker can slot into existing CLI/TUI expectations without touching serial-specific code paths. Prevents deadlocks by avoiding nested lock acquisition when appending log entries.

**Rationale:** A dedicated worker keeps MQTT handling out of the main UI thread, mirrors the serial worker API for minimal integration churn, and encapsulates defaults loading from firmware headers.

### CLI Routing & Rendering
**Location:** `tools/serial_cli/__init__.py`

Extends argument parsing to accept `--transport mqtt` everywhere, routes `status` to a lightweight MQTT subscriber, blocks other verbs with an explicit error, and normalizes presence table rendering with age columns. Maintains backwards compatibility for serial workflows while providing clear user feedback.

**Rationale:** Centralizing transport selection ensures both CLI commands and the interactive entry point respect user intent and simplifies future expansion once MQTT command verbs arrive.

### Interactive TUI Adaptation
**Location:** `tools/serial_cli/tui/textual_ui.py`

Detects column schemas from the current worker, updates status bars with MQTT broker info, disables command input when commands are unsupported, and keeps log rendering responsive. The TUI now refreshes presence tables based on dynamic columns instead of assuming firmware STATUS layouts.

**Rationale:** Presence monitoring requires different UI affordances than motor control; adapting columns and input affordances keeps the TUI intuitive in both transports.

## Database Changes (if applicable)

### Migrations
- _None_

### Schema Impact
- Not applicable.

## Dependencies (if applicable)

### New Dependencies Added
- _None_ (paho-mqtt/textual remain optional runtime dependencies; documentation clarifies installation.)

### Configuration Changes
- No configuration files were altered; broker defaults continue to load from `include/secrets.h`.

## Testing

### Test Files Created/Updated
- `test/test_Serial_CLI/test_cli_mqtt.py` — Validates presence rendering, duplicate handling, command errors, and CLI status wiring for MQTT mode.

### Test Coverage
- Unit tests: ✅ Complete  
- Integration tests: ⚠️ Partial — Host CLI tests cover worker logic; full broker integration exercised manually when hardware is available.
- Edge cases covered: Duplicate `msg_id` suppression, missing dependencies, unsupported commands over MQTT, presence table formatting.

### Manual Testing Performed
- `python test/test_Serial_CLI/test_cli_mqtt.py` — Confirms all MQTT CLI host tests pass (fast execution).

## User Standards & Preferences Compliance

### Global Coding Style
**File Reference:** `agent-os/standards/global/coding-style.md`

**How Your Implementation Complies:** Followed existing formatting (PEP 8-aligned), used descriptive names, and kept imports ordered.  
**Deviations:** None.

### Serial Interface Guidelines
**File Reference:** `agent-os/standards/frontend/serial-interface.md`

**How Your Implementation Complies:** Extended CLI without breaking command grammar, preserved explicit error messaging for unsupported operations, and reused existing logging patterns per guideline.  
**Deviations:** None.

### Build Validation
**File Reference:** `agent-os/standards/testing/build-validation.md`

**How Your Implementation Complies:** Added targeted host-side tests and executed them independently per spec, noting manual run results.  
**Deviations:** None.

### Hardware Validation
**File Reference:** `agent-os/standards/testing/hardware-validation.md`

**How Your Implementation Complies:** Documented that hardware/bench validation remains pending for full latency verification, aligning with staged validation expectations.  
**Deviations:** None.

### Unit Testing
**File Reference:** `agent-os/standards/testing/unit-testing.md`

**How Your Implementation Complies:** Added concise, deterministic unit tests scoped to the new functionality and ran only the new suite in line with the standard.  
**Deviations:** None.

## Integration Points (if applicable)

### APIs/Endpoints
- Subscribes to MQTT topic `devices/+/state` (QoS 1 retained) to surface presence data.

### External Services
- Uses `paho-mqtt` client libraries when installed to connect to the configured broker.

### Internal Dependencies
- Reuses existing CLI rendering utilities and Textual TUI infrastructure for consistent presentation.

## Known Issues & Limitations

### Issues
1. **MQTT Command Execution**  
   - Description: Command verbs remain unsupported over MQTT; worker surfaces explicit errors.  
   - Impact: Operators must continue using serial transport for command dispatch.  
   - Workaround: Switch back to `--transport serial` for motor control.  
   - Tracking: Covered by future roadmap items (see Deferred Follow-up task group).

### Limitations
1. **Manual Broker Validation Required**  
   - Description: Automated integration tests do not spin up a real broker; manual runs should validate end-to-end latency.  
   - Reason: Lightweight host tests prefer deterministic, dependency-free execution.  
   - Future Consideration: Incorporate a mock broker harness or containerized Mosquitto for CI once available.

## Performance Considerations
- Worker uses lightweight parsing and in-memory dictionaries; presence updates remain O(devices) with duplicate suppression preventing log spam.

## Security Considerations
- Continues to rely on plaintext MQTT on trusted LAN per spec; credentials loaded from compile-time secrets without new storage paths.

## Dependencies for Other Tasks
- Provides the CLI-side substrate required by Validation & Test Coverage (Task Group 3) to measure presence latency through the MQTT transport.

## Notes
- Re-running host-side tests is quick (`<1s`); ensure `paho-mqtt` is installed before live broker validation.
