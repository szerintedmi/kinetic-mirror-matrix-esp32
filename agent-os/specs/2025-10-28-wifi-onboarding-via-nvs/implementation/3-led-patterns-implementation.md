# Task 3: LED patterns

## Overview
**Task Reference:** Task #3 from `agent-os/specs/2025-10-28-wifi-onboarding-via-nvs/tasks.md`
**Implemented By:** api-engineer
**Date:** 2025-10-28
**Status:** ✅ Complete

### Task Description
Define the ESP32 status LED wiring, drive non-blocking blink patterns linked to onboarding states, and cover the behavior with focused host-side tests.

## Implementation Summary
Introduced a configurable status-LED pipeline inside `NetOnboarding`, allowing transports (serial today, MQTT later) to share the same visual feedback. The class now owns blink timing and state synchronization, while hardware specifics (pin and polarity) are injected via `configureStatusLed`. The Arduino entry point wires GPIO2 as active-low and calls the helper so the LED reacts to AP/CONNECTING/CONNECTED transitions automatically.

To keep the logic testable, the implementation tracks the logical LED state even in native builds and exposes debug accessors under `UNIT_TEST`. A dedicated Unity suite verifies fast/slow blink cadence along with the solid “connected” state. Board defaults live in `Esp32Dev.hpp` so future boards can re-map the LED without touching the library.

## Files Changed/Created

### Modified Files
- `include/boards/Esp32Dev.hpp` – Added `STATUS_LED_PIN`/polarity constants and BOOT button definitions used across onboarding tasks.
- `lib/net_onboarding/include/net_onboarding/NetOnboarding.h` – Published `configureStatusLed` plus private helpers/state to drive patterns; added unit-test hooks.
- `lib/net_onboarding/src/NetOnboarding.cpp` – Implemented LED pattern state machine, non-blocking toggles, and per-state refresh calls; ensured updates run every `loop()` iteration.
- `src/main.cpp` – Configured the status LED during setup and routed `NetOnboarding::loop()` to tick the LED logic.
- `test/test_NetOnboarding/test_NetOnboarding_native.cpp` – Extended native tests to cover AP/CONNECTING/CONNECTED LED expectations and cadence.

## Key Implementation Details

### Status LED configuration
**Location:** `lib/net_onboarding/include/net_onboarding/NetOnboarding.h`

Added `configureStatusLed(int pin, bool active_low)` so transports can declare hardware wiring without hard-coding. The class records the configuration, sets initial pattern based on current state, and remains a no-op when the LED is unavailable (pin < 0).

**Rationale:** Keeps hardware knowledge out of the core library while enabling both Arduino and native builds to share the same interfaces.

### Blink pattern scheduler
**Location:** `lib/net_onboarding/src/NetOnboarding.cpp`

Introduced an internal `LedPattern` enum with fast/slow/solid states. `refreshLedPattern_` maps onboarding state to a pattern, while `updateLed_` toggles the LED based on elapsed milliseconds (`125 ms` half-period for AP, `400 ms` for CONNECTING). The logic is entirely non-blocking and reuses the existing `nowMs_()` helper.

**Rationale:** Centralizes the timing policy and keeps future MQTT/shared transports aligned without duplicating logic.

### Host-visible verification hooks
**Location:** `lib/net_onboarding/include/net_onboarding/NetOnboarding.h`

Added `debugLedOn()/debugLedPattern()` under `UNIT_TEST` so native tests can assert LED behavior without hitting hardware-specific APIs.

**Rationale:** Maintains observability in the stub backend, enabling deterministic unit tests per the testing standards.

## Database Changes (if applicable)

N/A – no persistence schema changes.

## Dependencies (if applicable)

### New Dependencies Added

None.

### Configuration Changes

- `STATUS_LED_PIN` / `STATUS_LED_ACTIVE_LOW` constants introduced in `Esp32Dev.hpp`.

## Testing

### Test Files Created/Updated
- `test/test_NetOnboarding/test_NetOnboarding_native.cpp` – Added fast/slow/solid LED cadence checks.

### Test Coverage
- Unit tests: ✅ Complete
- Integration tests: ❌ None
- Edge cases covered: Re-entrancy for multi-command batches (via tests ensuring toggles do not retrigger); CONNECTING timeout fall-back to AP also validated.

### Manual Testing Performed
No hardware run performed yet; covered via native suite. Manual verification is scheduled in `planning/verification/2025-10-28-wifi-onboarding-manual.md`.

## User Standards & Preferences Compliance

### Global Coding Style
**File Reference:** `agent-os/standards/global/coding-style.md`

**How Your Implementation Complies:** Added focused helpers, avoided blocking calls, and kept state machine concise, matching the “small, focused functions” guidance.

**Deviations (if any):** None.

### Hardware Abstraction
**File Reference:** `agent-os/standards/backend/hardware-abstraction.md`

**How Your Implementation Complies:** All LED specifics are injected through board configuration; business logic remains in the library, keeping hardware knowledge centralized.

**Deviations (if any):** None.

### Unit Testing
**File Reference:** `agent-os/standards/testing/unit-testing.md`

**How Your Implementation Complies:** Expanded the native Unity suite with fast/slow/solid assertions, keeping runtime <3 s and avoiding hardware dependencies.

**Deviations (if any):** None.

## Integration Points (if applicable)

N/A – purely local LED signaling.

## Known Issues & Limitations

None identified in host testing. Hardware observation still pending to confirm perceived blink rates match expectation.

## Performance Considerations

Blink updates reuse existing `loop()` cadence and add negligible overhead (integer math and a digitalWrite when state changes), well within budget for the ESP32 main loop.

## Security Considerations

None. LED signaling does not expose new surfaces.

## Dependencies for Other Tasks

Task 4 (HTTP endpoints) and Task 7 (README updates) reference these state transitions when surfacing user guidance.

## Notes

- LED patterns were chosen to be clearly distinguishable even in bright lab settings; tweak constants in `refreshLedPattern_` if future UX research suggests different timings.
