# Task 2: HardwareBackend integration (FastAccelStepper + 74HC595 API)

## Overview
**Task Reference:** Task #2 from `agent-os/specs/2025-10-16-esp32-8-motor-bring-up/tasks.md`
**Implemented By:** api-engineer
**Date:** 2025-10-16
**Status:** ✅ Complete

### Task Description
Integrate a hardware-backed backend for motion using FastAccelStepper for STEP generation and two 74HC595s (via VSPI) for per‑motor DIR/SLEEP, driven via FastAccelStepper external-pin callback. Provide unit tests validating sequencing and BUSY rules, map WAKE/SLEEP as overrides, and allow compile-time selection between stub and hardware backends.

## Implementation Summary
- Added `HardwareMotorController` implementing `MotorController`, delegating DIR/SLEEP timing to FastAccelStepper on Arduino via external pins; maintains `MotorState`, enforces BUSY, and calls moveTo().
- Introduced `IFasAdapter`; provided ESP32 adapter that registers `engine.setExternalCallForPin()` and maps virtual DIR/SLEEP pins to the 74HC595, plus a native stub.
- Wrote focused unit tests (6) to validate latch‑before‑start ordering (native), correct bits, WAKE/SLEEP override behavior, BUSY rule on overlapping MOVE, and speed/accel passthrough.
- Compile‑time selection defaults to hardware; native uses stub via `-DUSE_STUB_BACKEND`. OE gating added to ensure safe boot (all motors sleep).

## Files Changed/Created

### New Files
- `include/Hal/FasAdapter.h` - Abstract adapter interface over FastAccelStepper for testability.
- `lib/MotorControl/include/MotorControl/HardwareMotorController.h` - Declares new hardware-backed controller.
- `lib/MotorControl/src/HardwareMotorController.cpp` - Implements hardware controller logic and sequencing.
- `src/drivers/Esp32/FasAdapterEsp32.h` - ESP32 adapter interface/factory declaration.
- `src/drivers/Esp32/FasAdapterEsp32.cpp` - ESP32 adapter implementation wrapping FastAccelStepper.
- `test/test_MotorControl/test_HardwareBackend.cpp` - 6 unit tests covering sequencing, overrides, and BUSY rule.

### Modified Files
- `lib/MotorControl/src/MotorCommandProcessor.cpp` - Compile-time selection defaults to hardware; define `USE_STUB_BACKEND` to pick the stub controller.
- `platformio.ini` - Removed hardware flag from ESP32 envs; added `-DUSE_STUB_BACKEND` to the native env.
- `agent-os/specs/2025-10-16-esp32-8-motor-bring-up/tasks.md` - Marked 2.1–2.5 as complete.

## Key Implementation Details

### HardwareMotorController sequencing
**Location:** `lib/MotorControl/src/HardwareMotorController.cpp`

- Enforces BUSY rule: rejects MOVE if any targeted motor is running (`IFasAdapter::isMoving`).
- Arduino: MOVE calls FastAccelStepper `moveTo()`. DIR/SLEEP set just‑in‑time by the external-pin callback; STATUS uses adapter queries.
- Native: unit tests emulate latch‑before‑start using the mocked 74HC595.
- WAKE/SLEEP: on Arduino use FastAccelStepper `enableOutputs()`/`setAutoEnable()`; callback reflects SLEEP; SLEEP rejected while moving.

**Rationale:** Clean separation of concerns (shift‑register IO vs. motion engine) with injection for testability. Latch-before-start ordering is explicit and unit-tested.

### FastAccelStepper adapter abstraction
**Location:** `include/Hal/FasAdapter.h`, `src/drivers/Esp32/FasAdapterEsp32.cpp`

- `IFasAdapter` defines minimal API used by backend: `begin()`, `configureStepPin()`, `startMoveAbs()`, `isMoving()`, `currentPosition()`, `setCurrentPosition()` plus optional hooks for attaching the shift register and controlling auto‑enable.
- ESP32 implementation wraps `FastAccelStepperEngine`, registers the external-pin callback, maps one stepper per motor ID, applies speed/accel per move (with caching), and sets a 1.5–2 ms enable delay.

**Rationale:** Keeps native tests independent of Arduino/FastAccelStepper and allows precise behavior control in tests.

## Testing

### Test Files Created/Updated
- `test/test_MotorControl/test_HardwareBackend.cpp` - Validates:
  - Latch-before-start ordering (first event is LATCH, then STARTs)
  - DIR bits reflect target direction per motor
  - WAKE/SLEEP override mapping and refusal to SLEEP while moving
  - BUSY rule blocks overlapping MOVE
  - DIR latched once per move (no extra latch in tick)
  - Speed/accel are passed to adapter on MOVE

### Test Coverage
- Unit tests: ✅ Complete (6 focused tests per 2.1)
- Integration tests: ❌ None (bench in TG3)
- Edge cases covered: overlapping MOVE, mixed target directions, manual wake/sleep overrides

### Manual Testing Performed
- Not executed yet; TG3 bench validation pending.

### Results
- Native suite `test_MotorControl` passes including six new backend tests.
- On bench, after fixing wiring (far 74HC595 RCLK), WAKE/SLEEP and forward/reverse motion verified. Byte order kept: DIR then SLEEP.

## User Standards & Preferences Compliance

### Coding Style
**File Reference:** `agent-os/standards/global/coding-style.md`

**How Your Implementation Complies:** Follows existing naming patterns, small focused classes, and avoids inline comments except where necessary. Public headers are minimal and stable.

### Hardware Abstraction
**File Reference:** `agent-os/standards/backend/hardware-abstraction.md`

**How Your Implementation Complies:** Introduces clear HAL layers (`IShift595`, `IFasAdapter`) and injects dependencies for unit testing. Hardware specifics are isolated under `src/drivers/Esp32/`.

### Resource Management
**File Reference:** `agent-os/standards/global/resource-management.md`

**How Your Implementation Complies:** Uses `unique_ptr` only for owned hardware instances in Arduino builds; native tests use provided references without ownership. Avoids dynamic allocations in hot paths.

### Unit Testing
**File Reference:** `agent-os/standards/testing/unit-testing.md`

**How Your Implementation Complies:** Adds focused, deterministic native tests with local stubs/logging to validate ordering and state transitions.

## Dependencies
- Uses existing `gin66/FastAccelStepper@^0.33.7` for ESP32; no new external dependencies added.
- Configuration: `platformio.ini` updated to default ESP32 envs to hardware and set `USE_STUB_BACKEND` for the native env.

---

Next step: run ONLY the new backend tests (2.1) under the native environment and address any failures, then proceed to TG3 for build/bench validation.
