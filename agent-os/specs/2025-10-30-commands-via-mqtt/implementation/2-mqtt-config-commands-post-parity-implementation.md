# Task 2: MQTT Config Commands (Post-Parity)

## Overview
**Task Reference:** Task #2 from `agent-os/specs/2025-10-30-commands-via-mqtt/tasks.md`
**Implemented By:** api-engineer
**Date:** 2025-11-01
**Status:** ✅ Complete

### Task Description
Add runtime configuration commands that let operators fetch or update MQTT broker settings, persist overrides in ESP32 Preferences, surface the commands over both serial and MQTT transports, document the workflow, and back the flow with focused tests.

## Implementation Summary
Implemented a dedicated MQTT config store backed by Preferences with slot-based atomic writes, exposed `MQTT:GET_CONFIG` / `MQTT:SET_CONFIG` through the command processor and MQTT command server, updated the presence client to react to config changes without rebooting, and refreshed documentation plus targeted native tests that cover round-trip persistence and reset behaviour.

## Files Changed/Created

### New Files
- `lib/mqtt_config/include/mqtt/MqttConfigStore.h`
- `lib/mqtt_config/src/MqttConfigStore.cpp`
- `test/test_MotorControl/test_MqttConfig.cpp`

### Modified Files
- `agent-os/specs/2025-10-30-commands-via-mqtt/tasks.md`
- `agent-os/specs/2025-10-30-commands-via-mqtt/implementation/prompts/2-mqtt-config-commands-post-parity.md`
- `docs/mqtt-command-schema.md`
- `docs/mqtt-local-broker-setup.md`
- `README.md`
- `lib/MotorControl/include/MotorControl/command/CommandHandlers.h`
- `lib/MotorControl/src/command/CommandHandlers.cpp`
- `lib/MotorControl/src/MotorCommandProcessor.cpp`
- `lib/mqtt_commands/include/mqtt/MqttCommandServer.h`
- `lib/mqtt_commands/src/MqttCommandServer.cpp`
- `lib/mqtt_presence/src/MqttPresenceClient.cpp`
- `lib/transport/src/CommandSchema.cpp`
- `test/test_MotorControl/test_main.cpp`

### Deleted Files
- None

## Key Implementation Details

### Preferences-backed config store
**Location:** `lib/mqtt_config/include/mqtt/MqttConfigStore.h`, `lib/mqtt_config/src/MqttConfigStore.cpp`

Created a singleton `mqtt::ConfigStore` that owns the broker config defaults and loads overrides from ESP32 Preferences under the `"mqtt"` namespace. The store uses dual slots (`A`/`B`) with an `active_slot` pointer so writes stage into the inactive slot before flipping, giving us an atomic swap that survives brown-outs. Each slot records `host`, `port`, `user`, and `pass` plus a version guard (`kConfigVersion = 1`). The API exposes `Current()`, `ApplyUpdate()`, `Reload()`, and `ResetForTests()` (UNIT_TEST only), and tracks a monotonically increasing `revision_` so consumers can detect changes without polling flash directly.

### Command handler plumbing
**Location:** `lib/MotorControl/src/command/CommandHandlers.cpp`

Added `MqttConfigCommandHandler` to the command router so serial and batch flows understand the new `MQTT:*` actions. `MQTT:GET_CONFIG` emits a single DONE line with quoted values and `{host,port,user,pass}_source` flags marking default vs override. `MQTT:SET_CONFIG` accepts space-delimited `key=value` pairs (with quoted values for whitespace) plus a `RESET` shortcut and per-field `default` sentinel. Validation funnels through `ConfigStore::ApplyUpdate`, returning `MQTT_BAD_PARAM` for schema issues and `MQTT_CONFIG_SAVE_FAILED` if the persistence step fails. 

### MQTT command translation
**Location:** `lib/mqtt_commands/include/mqtt/MqttCommandServer.h`, `lib/mqtt_commands/src/MqttCommandServer.cpp`

Extended the JSON→serial transformer so the MQTT command server recognises `MQTT:GET_CONFIG` (no params) and `MQTT:SET_CONFIG` bodies. String params are auto-quoted when necessary, numerics validated within 1–65535, and a `reset` JSON flag maps to the serial `RESET` token. Unsupported fields raise `MQTT_UNSUPPORTED_ACTION`, aligning MQTT error payloads with the serial schema.

### Presence client reconfiguration
**Location:** `lib/mqtt_presence/src/MqttPresenceClient.cpp`

Hooked the shared AsyncMqttClient into `ConfigStore` by caching the last applied revision and calling `applyBrokerConfig` on boot and each loop tick when the revision changes. The helper updates host/port/user/pass on the client, resets credentials when cleared, and triggers a controlled disconnect/reconnect sequence so new settings take effect without rebooting the node.

### Error catalog & docs
**Location:** `lib/transport/src/CommandSchema.cpp`, `docs/mqtt-command-schema.md`, `README.md`, `docs/mqtt-local-broker-setup.md`

Registered new error descriptors (`MQTT_BAD_PARAM`, `MQTT_CONFIG_SAVE_FAILED`) and expanded the schema doc with request/response examples plus reset guidance. README and the broker troubleshooting guide now walk operators through using `MQTT:GET_CONFIG` / `MQTT:SET_CONFIG` to adjust broker endpoints on the fly.

## Database Changes (if applicable)
- None (Preferences-based configuration only)

## Dependencies (if applicable)
- No new package dependencies were added.

## Testing

### Test Files Created/Updated
- `test/test_MotorControl/test_MqttConfig.cpp` – Native tests covering defaults, override persistence, and reset-to-default flows.
- `test/test_MotorControl/test_main.cpp` – Registered the new tests with the suite.

### Test Coverage
- Unit tests: ✅ Complete (native PlatformIO tests for config commands)
- Integration tests: ❌ None required for this task
- Edge cases covered: defaults vs overrides, persistence across reload, global reset

### Manual Testing Performed
- Not performed (native test suite exercises the flow)

## User Standards & Preferences Compliance

### Global Coding Style
**File Reference:** `agent-os/standards/global/coding-style.md`

Followed existing naming and header/implementation split conventions; config store API lives in `include/` and logic in `src/`. Utility helpers remain small/single-purpose.

### Global Conventions
**File Reference:** `agent-os/standards/global/conventions.md`

Extended existing command handler patterns (new handler registered via router, uppercase action detection) rather than inventing alternates, keeping serial and MQTT flows consistent.

### PlatformIO Project Setup
**File Reference:** `agent-os/standards/global/platformio-project-setup.md`

Placed new firmware sources under `lib/` with the expected `include/` + `src/` layout so PlatformIO auto-discovers the module without manifest changes.

### Resource Management
**File Reference:** `agent-os/standards/global/resource-management.md`

Cache config in RAM with a mutex and reuse existing AsyncMqttClient buffers; Preferences writes are staged via slots to avoid partial updates, meeting atomic write guidance.

### Backend Data Persistence
**File Reference:** `agent-os/standards/backend/data-persistence.md`

Introduced `kConfigVersion`, staged writes via inactive slot, and mirrored active settings in memory guarded by a mutex so runtime reads avoid flash churn.

### Backend Hardware Abstraction
**File Reference:** `agent-os/standards/backend/hardware-abstraction.md`

All persistence lives behind the new store; command handlers and presence logic consume the abstraction without direct Preferences calls, keeping hardware-specific code isolated.

### Backend Task Structure
**File Reference:** `agent-os/standards/backend/task-structure.md`

Extended the command router with a discrete handler and routed MQTT translation through existing builder methods, preserving the pipeline’s modular structure.

### Testing – Build Validation
**File Reference:** `agent-os/standards/testing/build-validation.md`

Executed the targeted native tests for the new functionality (`pio test -e native --filter test_MotorControl/test_MqttConfig`), matching the “run only impacted scope” guidance.

### Testing – Hardware Validation
**File Reference:** `agent-os/standards/testing/hardware-validation.md`

No hardware validation executed; config changes affect firmware data paths only and are covered by native tests.

### Testing – Unit Testing
**File Reference:** `agent-os/standards/testing/unit-testing.md`

Added focused cases that assert defaults, overrides, and reset behaviour without relying on the full suite, satisfying the requirement for scoped unit coverage.

## Integration Points (if applicable)

### APIs/Endpoints
- Serial console command `MQTT:GET_CONFIG` / `MQTT:SET_CONFIG`
- MQTT topic `devices/<node_id>/cmd` with `action` = `MQTT:GET_CONFIG` / `MQTT:SET_CONFIG`

### External Services
- Mosquitto (or compatible MQTT broker) – commands configure the connection target.

### Internal Dependencies
- Shared AsyncMqttClient from `mqtt::AsyncMqttPresenceClient`
- `transport::CommandSchema` for consistent response formatting

## Known Issues & Limitations

### Issues
1. **Credentials stored as plaintext**
   - Description: Broker username/password remain unencrypted in Preferences, matching prior approach.
   - Impact: Sensitive deployments should protect flash access; no change from previous behaviour.
   - Workaround: None within this scope (future enhancement could add encryption).
   - Tracking: Not yet tracked (same as historical implementation).

### Limitations
1. **Single active slot pair**
   - Description: Only two slots (A/B) are maintained; corruption of both would fall back to defaults.
   - Reason: Keeps implementation lightweight; multi-slot journal not required yet.
   - Future Consideration: Revisit if field reports highlight flash wear issues.

## Performance Considerations
- Config store caches overrides in RAM, avoiding repeated flash reads; updates reset the MQTT client’s backoff state but reuse existing buffers, keeping CPU/heap impact negligible.

## Security Considerations
- No encryption of credentials; behaviour unchanged from compile-time constants. Update documentation flags this so operators secure physical access.

## Dependencies for Other Tasks
- Provides the persistence layer required for forthcoming validation/bench tasks that verify config survives reboot.

## Notes
- `ResetForTests()` exists only under `UNIT_TEST` to keep production firmware minimal while allowing deterministic native tests.
