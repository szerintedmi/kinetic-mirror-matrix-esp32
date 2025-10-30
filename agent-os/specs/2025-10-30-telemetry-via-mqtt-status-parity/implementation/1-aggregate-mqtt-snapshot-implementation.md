# Task 1: Aggregate MQTT Snapshot

## Overview
**Task Reference:** Task #1 from `agent-os/specs/2025-10-30-telemetry-via-mqtt-status-parity/tasks.md`
**Implemented By:** api-engineer
**Date:** October 30, 2025
**Status:** ✅ Complete

### Task Description
Deliver a firmware-side aggregate MQTT status publisher that reuses the existing client connection, emits `{node_state, ip, motors:{...}}` snapshots on `devices/<mac>/status`, configures the offline Last Will payload, and removes the legacy retained presence topic.

## Implementation Summary
Extended the asynchronous MQTT client wrapper to expose a shared publish queue so additional topics can share a single connection. Added a dedicated `MqttStatusPublisher` that builds STATUS-parity JSON snapshots with cadence-aware change suppression, and wired it into the serial console loop so telemetry aligns with controller ticks. The retained `devices/<mac>/state` publisher was retired and existing presence payload builders were updated to the new schema without `msg_id`.

## Files Changed/Created

### New Files
- `lib/mqtt_status/include/mqtt/MqttStatusPublisher.h` - Declares the cadence-aware aggregate telemetry publisher.
- `lib/mqtt_status/src/MqttStatusPublisher.cpp` - Implements snapshot construction, hashing, and publish queue integration.

### Modified Files
- `lib/mqtt_presence/include/mqtt/MqttPresenceClient.h` - Introduced the generic `PublishMessage` struct and exposed queue/payload accessors.
- `lib/mqtt_presence/src/MqttPresenceClient.cpp` - Added shared publish queue plumbing, switched topics to `/status`, and removed `msg_id` handling.
- `src/console/SerialConsole.cpp` - Instantiated the new status publisher and routed controller state into the MQTT loop without impacting motor control cadence.
- `test/test_MqttPresence/test_MqttPresenceClient.cpp` - Updated expectations for the new payload schema and motion-triggered republish behavior.
- `README.md` - Documented the aggregate status topic and schema updates.
- `docs/mqtt-local-broker-setup.md` - Refreshed broker smoke-test guidance for the new topic/payload.
- `docs/mqtt-status-schema.md` - Added a schema reference for consumers of the aggregate snapshot topic.

### Deleted Files
- _None_

## Key Implementation Details

### Shared MQTT Publish Queue
**Location:** `lib/mqtt_presence/src/MqttPresenceClient.cpp`

Added a bounded `std::deque<PublishMessage>` inside the async client implementation so firmware sub-systems enqueue publishes against the existing connection. The queue drains only when the underlying `AsyncMqttClient` reports a connected state, preventing duplicate sockets while guaranteeing ordering.

**Rationale:** Reusing the connection satisfies the resource-management constraint and eliminates reconnect storms from multiple clients.

### Aggregate Status Publisher
**Location:** `lib/mqtt_status/src/MqttStatusPublisher.cpp`

Implements snapshot serialization that mirrors `STATUS` output, including boolean flags and timing fields. Cadence control enforces 1 Hz idle / 5 Hz motion intervals, while payload hashing suppresses redundant publishes between ticks but still pushes immediate updates whenever the serialized snapshot changes.

**Rationale:** Centralizing snapshot logic keeps the console loop lightweight and ensures STATUS parity across transports.

### Console Integration
**Location:** `src/console/SerialConsole.cpp`

The serial console now owns both the async MQTT client and the status publisher. Each loop gathers controller state, updates motion/power hints, and enqueues telemetry without blocking existing serial workflows.

**Rationale:** Integrating alongside the existing controller tick guarantees that telemetry uses the same authoritative state as the serial STATUS command.

## Database Changes (if applicable)

_Not applicable._

## Dependencies (if applicable)

_No new external dependencies were required._

## Testing

### Test Files Created/Updated
- `test/test_MqttStatus/test_MqttStatusPublisher.cpp` - (Covered under Task 3) Validates serialization, cadence switching, and change detection.
- `test/test_MqttPresence/test_MqttPresenceClient.cpp` - Updated to reflect the new payload schema.

### Test Coverage
- Unit tests: ✅ Complete (native unit tests listed above)
- Integration tests: ❌ None (existing serial integration exercised separately)
- Edge cases covered: motion/idle cadence transitions, change-triggered publishes, offline Last Will configuration.

### Manual Testing Performed
- Hardware bench verification (Task 1.4) could not be executed in this environment. A follow-up bench run should confirm 1 Hz / 5 Hz cadence and QoS0 delivery on hardware.

## User Standards & Preferences Compliance

### global/coding-style.md
**File Reference:** `agent-os/standards/global/coding-style.md`

**How Your Implementation Complies:** New publisher logic keeps functions focused, avoids duplication by reusing helpers, and preserves existing naming patterns across MQTT modules.

**Deviations (if any):** None.

### global/resource-management.md
**File Reference:** `agent-os/standards/global/resource-management.md`

**How Your Implementation Complies:** The shared publish queue avoids spawning extra clients, and the status publisher pre-reserves buffers to minimize heap churn.

**Deviations (if any):** None.

### backend/task-structure.md
**File Reference:** `agent-os/standards/backend/task-structure.md`

**How Your Implementation Complies:** The console loop remains single-purpose, with telemetry work deferred to the publisher component to keep the main task responsive.

**Deviations (if any):** None.

## Integration Points (if applicable)

### MQTT Topic
- `devices/<mac>/status` - Aggregate STATUS snapshot (QoS0, non-retained).
  - Request format: JSON snapshot described above.
  - Response format: Subscribers receive live snapshots and the offline Last Will payload.

