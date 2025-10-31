# Task 1: Firmware MQTT Presence

## Overview
**Task Reference:** Task #1 from `agent-os/specs/2025-10-29-mqtt-steel-thread-presence/tasks.md`
**Implemented By:** api-engineer
**Date:** 2025-10-29
**Status:** ✅ Complete

### Task Description
Deliver the firmware-side MQTT presence steel thread: generate UUID-backed message IDs, publish retained presence and LWT payloads, keep motion loops non-blocking when MQTT fails, and document the new expectations.

## Implementation Summary
Introduced a hardware-seeded UUIDv4 generator so both serial acknowledgments and MQTT presence messages share a common `msg_id`, replacing the legacy monotonic CID flow. The generator now guards against consecutive duplicates by tracking both the active and last-issued IDs, protecting downstream tooling even when the deterministic test hook repeats values. Built a dedicated `MqttPresenceClient` with an AsyncMqttClient wrapper to publish retained JSON presence updates (`{"state":"ready","ip":"...","msg_id":"..."}`) at ~1 Hz and on motion/power transitions while logging a single broker failure warning. Integrated the presence client into the Arduino console loop, updated all control-plane logs to emit `msg_id`, emitted a positive `CTRL: MQTT_CONNECTED broker=<host>:<port>` confirmation on handshake, adjusted docs, and added native unit tests (plus a regression in the shared test suite) that validate payload formatting, UUID propagation, STATUS uniqueness, and the connect-failure warning path.

## Files Changed/Created

### New Files
- `lib/mqtt_presence/include/mqtt/MqttPresenceClient.h` - Declares the presence publisher core logic and the AsyncMqtt wrapper.
- `lib/mqtt_presence/src/MqttPresenceClient.cpp` - Implements presence scheduling, AsyncMqtt integration, and broker failure handling.
- `lib/net_onboarding/include/net_onboarding/MessageId.h` - Public API for UUID-based message IDs and test hooks.
- `lib/net_onboarding/src/MessageId.cpp` - Implements the UUIDv4 generator with ESP RNG support and active-ID tracking.
- `test/test_MqttPresence/test_MqttPresenceClient.cpp` - Native tests covering payload formatting, UUID sequencing, and broker-failure logging.

### Modified Files
- `lib/MotorControl/include/MotorControl/MotorCommandProcessor.h` - Exposes controller access for presence telemetry.
- `lib/MotorControl/include/MotorControl/command/CommandExecutionContext.h` - Replaces CID helpers with `msg_id` accessors.
- `lib/MotorControl/src/command/CommandExecutionContext.cpp` - Binds the new message ID API.
- `lib/MotorControl/src/command/CommandHandlers.cpp` - Emits `msg_id` on all CTRL responses and NET flows.
- `lib/MotorControl/src/command/CommandRouter.cpp` - Returns `msg_id` in unknown-action errors.
- `lib/MotorControl/src/command/CommandBatchExecutor.cpp` - Aggregates batch ACKs using UUID message IDs.
- `src/console/SerialConsole.cpp` - Instantiates the MQTT presence client and feeds motion/power state into it.
- `src/main.cpp` - Logs NET events with `msg_id` correlation instead of numeric CIDs.
- `include/secrets.h` - Adds default MQTT broker host/user/pass placeholders.
- `README.md` - Documents MQTT presence behaviour and follow-up credential work.
- `agent-os/specs/2025-10-29-mqtt-steel-thread-presence/tasks.md` - Marks firmware presence tasks complete.
- `test/test_MotorControl/test_CommandPipeline.cpp` - Updates assertions for `msg_id=` formatting.
- `test/test_MotorControl/test_FeatureFlows.cpp` - Same as above.
- `test/test_MotorControl/test_ProtocolBasicsAndMotion.cpp` - Same as above.
- `test/test_MotorControl/test_Thermal.cpp` - Same as above.
- `test/test_NetCommands/test_NetCommands_native.cpp` - Adjusts NET command assertions for `msg_id`.
- `test/test_Serial_CLI/test_cli_thermal_flag.py` - Aligns CLI parsing with `msg_id`.

### Deleted Files
- `lib/net_onboarding/include/net_onboarding/Cid.h` - Replaced by the new MessageId API.
- `lib/net_onboarding/src/Cid.cpp` - Superseded by UUID-backed message IDs.

## Key Implementation Details

### UUID-backed message IDs
**Location:** `lib/net_onboarding/src/MessageId.cpp:30`

Introduced a mutex-guarded UUIDv4 generator that uses `esp_random()` on-device and a `std::mt19937_64` fallback for native builds. The API now exposes `NextMsgId`, active ID tracking, and test hooks so firmware and host tools share the same correlation identifiers.

**Rationale:** Guarantees globally unique correlation tokens across both transports while keeping the interface testable and hardware-seeded.

### Presence publisher and Async wrapper
**Location:** `lib/mqtt_presence/src/MqttPresenceClient.cpp:21`

The core client tracks motion/power state changes, throttles periodic publishes, builds ready/offline payloads, and logs a single broker-failure warning. The Arduino-only wrapper wires those hooks into `AsyncMqttClient`, configures will credentials, and defers connect attempts until Wi‑Fi onboarding reports `CONNECTED`.

**Rationale:** Keeps timing-sensitive motor control free of MQTT work while providing a reusable publishing primitive testable on the native target.

### Console integration
**Location:** `src/console/SerialConsole.cpp:14`

The serial console now owns an `AsyncMqttPresenceClient`, forwarding motion/awake summaries after every controller tick and driving the presence loop with `millis()`. Setup lazily constructs the client once Wi‑Fi services are ready.

**Rationale:** Reuses the existing Arduino loop cadence without introducing new tasks or blocking workqueues.

### NET logging and command acknowledgments
**Location:** `lib/MotorControl/src/command/CommandHandlers.cpp:66`

All CTRL/NET responses emit `msg_id=<uuid>` and NET flows stash the active ID through the onboarding helpers. Serial command batches, router errors, and NET async logs now align with the MQTT presence `msg_id` field.

**Rationale:** Aligns serial and MQTT transports for future parity while preserving downstream parsing expectations.

## Database Changes (if applicable)
_No database changes._

## Dependencies (if applicable)

### New Dependencies Added
- `marvinroger/AsyncMqttClient` (requested addition) - Provides the AsyncMqttClient implementation required by the new presence publisher.

### Configuration Changes
- Added default MQTT broker host/port/user/pass macros in `include/secrets.h`.

## Testing

### Test Files Created/Updated
- `test/test_MqttPresence/test_MqttPresenceClient.cpp` - Validates JSON payload formatting, UUID propagation, and connect-failure logging.
- `test/test_common/test_msgid.cpp` - Asserts that back-to-back STATUS commands emit distinct UUIDs.
- Updated existing unit/integration tests to expect JSON presence payloads carrying `msg_id`.

### Test Coverage
- Unit tests: ✅ Complete (targeted native suite)
- Integration tests: ❌ None
- Edge cases covered: retained payload formatting, sequential UUID usage, broker failure logging once.

### Manual Testing Performed
_No manual testing performed; awaiting broker integration._

## User Standards & Preferences Compliance

### coding-style.md
**File Reference:** `agent-os/standards/global/coding-style.md`

**How Your Implementation Complies:** Presence logic is encapsulated in a dedicated module with small, focused methods (`MqttPresenceClient::loop`, `publishReady`) and clear naming, matching the lean-module guidance.

**Deviations (if any):** None.

### resource-management.md
**File Reference:** `agent-os/standards/global/resource-management.md`

**How Your Implementation Complies:** Presence publishing avoids dynamic allocations in hot paths (reuses strings, retains 1 Hz cadence) and defers MQTT work outside motor loops to maintain CPU headroom.

**Deviations (if any):** None.

### unit-testing.md
**File Reference:** `agent-os/standards/testing/unit-testing.md`

**How Your Implementation Complies:** Added fast native tests with deterministic assertions and explicit failure messages, isolated from hardware by using the stub backend.

**Deviations (if any):** None.

## Integration Points (if applicable)

### External Services
- MQTT broker at `MQTT_BROKER_HOST` (defaults to local LAN); presence publishes retained QoS1 `devices/<mac>/state` messages.
