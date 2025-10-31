# Task 1: MQTT Command Parity Core

## Overview
**Task Reference:** Task #1 from `agent-os/specs/2025-10-30-commands-via-mqtt/tasks.md`
**Implemented By:** api-engineer
**Date:** 2025-10-30
**Status:** ✅ Complete

### Task Description
Establish a shared command schema, move the message ID helper to a transport namespace, update serial formatting to the shared catalog, implement the firmware MQTT command server with duplicate suppression, and add focused tests plus build validation.

## Implementation Summary
Implemented a transport-layer command schema that both serial and MQTT paths share, relocated the UUID/active-id helper into a dedicated `transport::message_id` module, and wired firmware to generate MQTT acknowledgements/completions via a new `mqtt::MqttCommandServer`. Serial output now normalizes responses through the shared schema without changing line formatting. Added native unit tests covering schema parsing, duplicate handling, and BUSY parity, authored the MQTT schema documentation, and executed the requested targeted test/build commands.

## Files Changed/Created

### New Files
- `docs/mqtt-command-schema.md` – Canonical MQTT command/response schema and error catalog.
- `lib/transport/include/transport/MessageId.h` – Shared message ID API header.
- `lib/transport/src/MessageId.cpp` – Shared message ID implementation.
- `lib/transport/include/transport/CommandSchema.h` – Shared response parser/formatter types.
- `lib/transport/src/CommandSchema.cpp` – Implementation of shared response parsing.
- `lib/mqtt_commands/include/mqtt/MqttCommandServer.h` – MQTT command server interface.
- `lib/mqtt_commands/src/MqttCommandServer.cpp` – MQTT command server implementation.
- `test/test_transport/test_message_id.cpp` – Unit tests for transport message IDs.
- `test/test_mqtt_command_server/test_MqttCommandServer.cpp` – Native tests for the command server.

### Modified Files
- `lib/net_onboarding/include/net_onboarding/MessageId.h` – Redirected APIs to transport namespace.
- `lib/MotorControl/src/command/CommandExecutionContext.cpp` – Consume transport message ID helpers.
- `lib/MotorControl/src/command/ResponseFormatter.cpp` – Normalize serial formatting through transport schema.
- `lib/mqtt_presence/include/mqtt/MqttPresenceClient.h` – Expose subscription hook.
- `lib/mqtt_presence/src/MqttPresenceClient.cpp` – Subscription plumbing for command server and callback dispatch.
- `src/main.cpp` – Swap to transport message ID utilities.
- `src/console/SerialConsole.cpp` – Instantiate MQTT command server and integrate with console loop.
- `test/test_common/test_msgid.cpp` – Use transport message ID API.
- `test/test_MotorControl/test_CommandPipeline.cpp` – Use transport message ID API.
- `agent-os/specs/2025-10-30-commands-via-mqtt/tasks.md` – Marked Task Group 1 items complete.

### Deleted Files
- `lib/net_onboarding/src/MessageId.cpp` – Replaced by `transport::message_id` implementation.

## Key Implementation Details

### Transport Message ID module
**Location:** `lib/transport/include/transport/MessageId.h`, `lib/transport/src/MessageId.cpp`

Consolidated message ID generation/active tracking into a transport namespace so both serial and MQTT stacks can consume UUIDs without pulling net onboarding internals. The helper preserves previous behavior (no consecutive duplicates, ability to override generator for tests) and exposes the same API surface via `net_onboarding` inline wrappers to keep existing call sites compiling.

**Rationale:** Provides a neutral surface for transports, satisfying the spec requirement to move the helper outside onboarding.

### Command schema utilities
**Location:** `lib/transport/include/transport/CommandSchema.h`, `lib/transport/src/CommandSchema.cpp`

Added shared parsing/formatting primitives that transform `motor::command::CommandResult` lines into structured responses. Serial formatting now routes through the parser to ensure schema alignment while preserving existing line outputs.

**Rationale:** Centralizes status/error catalogs and enables MQTT to reuse serial outputs without duplicating parsing logic.

### MQTT command server
**Location:** `lib/mqtt_commands/src/MqttCommandServer.cpp`

Implemented a firmware-side worker that subscribes to `devices/<mac>/cmd`, validates payloads, converts JSON actions into serial command strings, executes them through `MotorCommandProcessor`, and publishes ACK/completion payloads following the documented schema. Tracks recent `cmd_id`s for duplicate handling, including rate-limited logging, and waits for MOVE completions before emitting final results with motor snapshots.

**Rationale:** Delivers MQTT parity with single-action enforcement, duplicate suppression, and shared schema mapping per spec.

### Console integration
**Location:** `src/console/SerialConsole.cpp`

Creates the command server alongside the existing presence/status publishers, reusing the shared AsyncMqttClient instance for publish/subscribe. The loop binds the command server once the status topic is known and services its completion checks each tick.

**Rationale:** Provides runtime wiring for the command server without altering serial behavior or telemetry cadence.

### Native/unit tests
**Location:** `test/test_transport/test_message_id.cpp`, `test/test_mqtt_command_server/test_MqttCommandServer.cpp`

Added targeted PlatformIO native tests covering message ID uniqueness/deduping, MQTT payload validation failures, successful MOVE completion, duplicate replay behavior, and BUSY parity.

**Rationale:** Satisfies requirement for 2–8 focused tests verifying schema validation, success mapping, duplicate suppression, and BUSY handling.

### MQTT documentation
**Location:** `docs/mqtt-command-schema.md`

Authored the canonical schema doc with envelope description, status codes, catalog table, parameter mappings, and duplicate handling guidance.

**Rationale:** Fulfills Task 1.1 documentation deliverable.

## Database Changes (if applicable)
Not applicable.

## Dependencies (if applicable)
No new external dependencies added.

## Testing

### Test Files Created/Updated
- `test/test_transport/test_message_id.cpp` – Exercises transport message ID behavior.
- `test/test_mqtt_command_server/test_MqttCommandServer.cpp` – Validates MQTT command server flows.
- Existing tests updated to reference `transport::message_id`.

### Test Coverage
- Unit tests: ✅ Complete
- Integration tests: ⚠️ Partial (command server covered via native harness; hardware exercised via build)
- Edge cases covered: invalid MQTT payload, duplicate `cmd_id`, BUSY rejection, MOVE success path.

### Manual Testing Performed
Not performed (firmware features validated via automated tests and PlatformIO builds).

## User Standards & Preferences Compliance

### global/coding-style.md
**File Reference:** `agent-os/standards/global/coding-style.md`

**How Your Implementation Complies:** New code uses existing namespace patterns, header guards, and camelCase naming consistent with firmware style.

**Deviations (if any):** None.

### global/platformio-project-setup.md
**File Reference:** `agent-os/standards/global/platformio-project-setup.md`

**How Your Implementation Complies:** Added new libraries under `lib/` with headers in `include/` and ensured PlatformIO builds/tests are invoked per instructions.

**Deviations (if any):** None.

### backend/task-structure.md
**File Reference:** `agent-os/standards/backend/task-structure.md`

**How Your Implementation Complies:** Commands remain routed through `MotorCommandProcessor`; new module composes on top without bypassing existing validation.

**Deviations (if any):** None.

### backend/hardware-abstraction.md
**File Reference:** `agent-os/standards/backend/hardware-abstraction.md`

**How Your Implementation Complies:** Command server reuses controller interfaces without embedding hardware-specific logic; all MQTT integration stays at firmware task layer.

**Deviations (if any):** None.

### testing/unit-testing.md
**File Reference:** `agent-os/standards/testing/unit-testing.md`

**How Your Implementation Complies:** Added deterministic native unit tests with clear assertions and command coverage, executed via `platformio test`.

**Deviations (if any):** None.

## Integration Points (if applicable)
- MQTT command topic: `devices/<node_id>/cmd`
- MQTT response topic: `devices/<node_id>/cmd/resp`
- Relies on `mqtt::AsyncMqttPresenceClient` for publish/subscribe.

## Known Issues & Limitations
None.

## Performance Considerations
Command server reuses existing MQTT client and buffers; duplicate cache limited to 12 entries to bound memory. JSON documents sized conservatively (<1.5 kB).

## Security Considerations
Validates JSON payloads and enforces single-action requests to prevent multi-command injection. Duplicate cache prevents replay execution.

## Dependencies for Other Tasks
Task Group 2 (configuration commands) depends on shared schema/documentation and transport utilities established here.

## Notes
PlatformIO builds for `esp32DedicatedStep` and `esp32SharedStep` succeed; native `pio run` excluded by running environment-specific build per instructions.
