# Task 6: Long-press reset

## Overview
**Task Reference:** Task #6 from `agent-os/specs/2025-10-28-wifi-onboarding-via-nvs/tasks.md`
**Implemented By:** api-engineer
**Date:** 2025-10-28
**Status:** ✅ Complete

### Task Description
Detect a ≥5 s hold on the ESP32 BOOT button, clear saved Wi-Fi credentials, and return the device to SoftAP mode while ignoring short taps.

## Implementation Summary
Board metadata now documents the BOOT button as active-low GPIO0, and the Arduino entry point initializes it with an internal pull-up. A small state machine in `loop()` samples the button every cycle, tracking press duration without blocking. Once the press exceeds `kResetHoldMs`, it emits a control line on serial and calls `Net().resetCredentials()`, which flips the onboarding state back to AP mode. Releasing the button resets the timer, ensuring single-fire behavior per press.

## Files Changed/Created

### Modified Files
- `include/boards/Esp32Dev.hpp` – Added `RESET_BUTTON_PIN`/polarity constants alongside the LED wiring.
- `src/main.cpp` – Configured the button during setup and added `reset_button_tick()` to drive long-press detection.

## Key Implementation Details

### Non-blocking hold detection
**Location:** `src/main.cpp`

`reset_button_tick()` keeps `was_pressed`, `press_started`, and `fired` static across calls. Each loop iteration compares `millis()` against the start time and fires once per hold when the configured threshold (5 s) is reached. Short taps reset the timer without side effects.

**Rationale:** Avoids `delay()` usage so motion/console loops remain responsive and conforms to the task-structure standard regarding bounded blocking.

### Credential reset hook
**Location:** `src/main.cpp`

When the long-press triggers, the code prints `CTRL: NET:RESET_BUTTON LONG_PRESS` to serial and invokes `Net().resetCredentials()`. The onboarding library takes care of re-entering SoftAP and updating status/LEDs.

**Rationale:** Reuses existing library functionality rather than duplicating NVS logic in the sketch.

## Database Changes (if applicable)

N/A – reuses existing NVS helpers.

## Dependencies (if applicable)

### New Dependencies Added

None.

### Configuration Changes

- `RESET_BUTTON_PIN` and `RESET_BUTTON_ACTIVE_LOW` defined for the ESP32 DevKit board.

## Testing

### Test Files Created/Updated

No automated tests (requires hardware input). Behaviour verified indirectly since `Net().resetCredentials()` is already covered by native tests; manual validation captured in verification checklist (Scenario C).

### Test Coverage
- Unit tests: ❌ None (hardware interaction)
- Integration tests: ❌ None
- Edge cases covered: Single-fire logic prevents repeated triggers during continuous hold.

### Manual Testing Performed
Pending – to be executed on hardware with serial + LED observation.

## User Standards & Preferences Compliance

### Resource Management
**File Reference:** `agent-os/standards/global/resource-management.md`

**How Your Implementation Complies:** Avoids blocking waits and ensures sampling stays lightweight inside the main loop.

**Deviations (if any):** None.

### Hardware Abstraction
**File Reference:** `agent-os/standards/backend/hardware-abstraction.md`

**How Your Implementation Complies:** Board-specific constants live in `Esp32Dev.hpp`; the loop logic uses those values so alternate boards can override them centrally.

**Deviations (if any):** None.

### Task Structure
**File Reference:** `agent-os/standards/backend/task-structure.md`

**How Your Implementation Complies:** Keeps long-press handling single-purpose and non-blocking, aligning with the “Keep tasks single-purpose” and “Keep blocking bounded” guidance.

**Deviations (if any):** None.

## Integration Points (if applicable)

- Hooks directly into `NetOnboarding` by calling `resetCredentials()` and relies on its state transitions.

## Known Issues & Limitations

- Requires physical hardware to validate timing tolerances; threshold chosen to balance accidental taps vs intentional reset.

## Performance Considerations

Minimal overhead (single digital read per loop); no impact on motion control timing.

## Security Considerations

None – physical interaction only.

## Dependencies for Other Tasks

Task 7 references this behavior in the README and manual verification plan.

## Notes

- If future hardware exposes a different BOOT button pin, update `Esp32Dev.hpp` only; the runtime logic will adapt automatically.
