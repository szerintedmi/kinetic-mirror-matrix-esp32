# Task 2: MQTT CLI/TUI Parity

## Overview
**Task Reference:** Task #2 from `agent-os/specs/2025-10-30-telemetry-via-mqtt-status-parity/tasks.md`
**Implemented By:** api-engineer
**Date:** October 30, 2025
**Status:** ✅ Complete

### Task Description
Update host tooling so the MQTT transport renders the same STATUS table as the serial path, highlights stale telemetry, and documents the new aggregate snapshot topic/payload.

## Implementation Summary
Refactored the MQTT worker to subscribe to `devices/+/status`, parse aggregate `{motors:{...}}` payloads, and expose rows shaped like serial STATUS output. The TUI now keeps the motor table identical to the serial layout while surfacing node-level age/state/IP summaries in the header bar (with stale highlighting) and leaving command dispatch disabled in MQTT mode. Documentation and helper messaging were refreshed to describe the new topic and schema.

## Files Changed/Created

### New Files
- _None (tests land under Task 3)._ 

### Modified Files
- `tools/serial_cli/mqtt_runtime.py` - Parses aggregate snapshots, maintains per-device caches, and exposes node summaries alongside STATUS-shaped rows.
- `tools/serial_cli/__init__.py` - Adjusted table routing so MQTT status rows render with STATUS columns.
- `tools/serial_cli/tui/textual_ui.py` - Added MQTT-specific header summaries with stale-age highlighting while keeping the motor table column-parity with serial output.
- `README.md` - Updated CLI usage examples and MQTT transport description to reflect the new topic/payload.
- `docs/mqtt-local-broker-setup.md` - Updated smoke-test guidance for the aggregate status topic.

### Deleted Files
- _None_

## Key Implementation Details

### Aggregate Snapshot Parsing
**Location:** `tools/serial_cli/mqtt_runtime.py`

The worker now decodes `{motors:{...}}` JSON, normalizes booleans to the serial `0/1` convention, and tracks age/device metadata alongside STATUS-aligned fields. A shared cache ensures the CLI and TUI receive consistently sorted rows per device.

**Rationale:** Aligning row shape with serial parsing lets host views reuse existing rendering logic without special cases.

### TUI Stale Highlighting
**Location:** `tools/serial_cli/tui/textual_ui.py`

When running over MQTT, the TUI keeps the motor table identical to the serial layout but injects a header summary per node, coloring age values yellow (>2 s) or red (>5 s), showing IPs, and flagging offline states. Command input remains disabled, preserving the existing transport constraint.

**Rationale:** Surface freshness at a glance while keeping interaction semantics unchanged.

### Host Documentation Refresh
**Location:** `README.md`, `docs/mqtt-local-broker-setup.md`

Examples and broker notes now call out the aggregate snapshot topic, QoS 0 semantics, and replaced presence message expectations.

**Rationale:** Keeps operators aligned with the new telemetry pipeline and topic schema.

## Database Changes (if applicable)

_Not applicable._

## Dependencies (if applicable)

_No dependency changes required._

## Testing

### Test Files Created/Updated
- `tools/serial_cli/tests/test_mqtt_runtime.py` - (see Task 3) Covers ingest and table rendering behavior for aggregate payloads.

### Test Coverage
- Unit tests: ✅ Complete (Python unit tests cover parsing and ordering)
- Integration tests: ❌ None
- Edge cases covered: Mixed device ordering, motors lacking `actual_ms`, stale-age computation.

### Manual Testing Performed
- CLI/TUI manual verification was not possible in this environment. Automated unit tests confirm the data pipeline; interactive smoke-tests should be run on a workstation with a broker once available.

## User Standards & Preferences Compliance

### global/coding-style.md
**File Reference:** `agent-os/standards/global/coding-style.md`

**How Your Implementation Complies:** Python changes preserve lean functions, avoid duplication by centralizing JSON parsing, and follow existing naming conventions.

**Deviations (if any):** None.

### global/conventions.md
**File Reference:** `agent-os/standards/global/conventions.md`

**How Your Implementation Complies:** Documentation stays in canonical README/docs locations and references the new schema doc for discoverability.

**Deviations (if any):** None.

### testing/unit-testing.md
**File Reference:** `agent-os/standards/testing/unit-testing.md`

**How Your Implementation Complies:** Added lightweight Python unit tests that run quickly on host and assert both parsing outputs and rendering alignment.

**Deviations (if any):** None.

## Integration Points (if applicable)

### CLI Rendering
- `mirrorctl status --transport mqtt` now prints the same STATUS table once a snapshot arrives.
- The interactive TUI mirrors serial columns, augmenting with device/age metadata when running in MQTT mode.
