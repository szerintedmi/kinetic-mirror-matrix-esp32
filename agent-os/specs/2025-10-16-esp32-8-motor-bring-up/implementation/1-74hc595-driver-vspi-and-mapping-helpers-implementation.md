# 1 — 74HC595 driver (VSPI) + mapping helpers — Implementation

## Summary
- Implemented a clean HAL + driver for two daisy-chained 74HC595s on ESP32 VSPI and pure bit‑packing helpers for DIR/SLEEP.
- Removed UNIT_TEST ifdefs in favor of platform‑selected sources and libraries; added board pin constants.
- Added focused native and device tests; API is usable standalone (no motion backend required).

## Files Changed / Added
- HAL + drivers
  - include/Hal/Shift595.h:1 — `IShift595` interface (public HAL).
  - include/drivers/Esp32/Shift595Vspi.h:1, src/Drivers/Esp32/Shift595Vspi.cpp:1 — ESP32 VSPI driver; shifts two bytes (DIR then SLEEP) and latches once via RCLK.
  - include/drivers/Stub/Shift595Stub.h:1 — Native stub driver (records last bytes + latch count) for host tests.
- Mapping helpers
  - lib/MotorControl/include/MotorControl/Bitpack.h:1 — `compute_dir_bits()` and `compute_sleep_bits()` (SLEEP polarity 1=awake).
- Command processor rename (protocol → MotorCommandProcessor)
  - lib/MotorControl/include/MotorControl/MotorCommandProcessor.h:1, lib/MotorControl/src/MotorCommandProcessor.cpp:1 — Transport‑agnostic command parser.
  - lib/MotorControl/include/MotorControl/MotorController.h:1, lib/MotorControl/src/StubMotorController.cpp:1 — Stub controller (logic only).
  - src/console/SerialConsole.cpp:1 — Uses `MotorCommandProcessor` (`commandProcessor` variable) for interactive console.
- Board pin map
  - include/boards/Esp32Dev.hpp:1 — `VSPI_SCK=18`, `VSPI_MISO=19`, `VSPI_MOSI=23`, `VSPI_SS=-1`, `SHIFT595_RCLK=5`, `STEP_PINS[8]`.
- Tests (renamed suites to CamelCase after test_)
  - test/test_MotorControl/test_Bitpack.cpp:1 — Validates DIR pass‑through and SLEEP overrides.
  - test/test_Drivers/test_Shift595Stub.cpp:1 — Confirms bytes (0xAA,0x55) and exactly one latch per call.
  - test/test_Drivers/test_Shift595Esp32.cpp:1 — Minimal on‑device sanity; latch pin pulses and returns LOW.
- PlatformIO
  - platformio.ini:1 — Simplified build; native excludes Arduino/ESP32 sources via build_src_filter; `-Iinclude` retained.
- Tasks
  - agent-os/specs/2025-10-16-esp32-8-motor-bring-up/tasks.md:1 — TG1 items 1.0–1.4 checked complete.

## Implementation Details

### VSPI driver and latch
**Location:** src/Drivers/Esp32/Shift595Vspi.cpp:1

- Uses VSPI (`SPIClass spi(VSPI)`) with pins from `include/boards/Esp32Dev.hpp`.
- `setDirSleep(dir,sleep)` sends two transfers: first DIR byte, then SLEEP byte, then toggles RCLK (HIGH ~1 µs → LOW) once.
- Daisy‑chain polarity: last byte shifted occupies the 595 nearest the ESP32. We send SLEEP last, so the near 595 = SLEEP, far 595 = DIR.

**Rationale:** Deterministic two‑byte update and single latch keeps bring‑up and probing straightforward, and avoids relying on SS.

### Native stub for host tests
**Location:** include/drivers/Stub/Shift595Stub.h:1

- Lightweight recorder exposes `last_dir()`, `last_sleep()`, and `latch_count()`; no hardware dependencies.

### Mapping helpers
**Location:** lib/MotorControl/include/MotorControl/Bitpack.h:1

- `compute_dir_bits` is pass‑through of the forward mask.
- `compute_sleep_bits` sets/clears bits over a base mask with SLEEP=1 (awake) polarity.

### Command processor rename (prep for TG2)
**Location:** lib/MotorControl/src/MotorCommandProcessor.cpp:1

- Renamed Protocol → MotorCommandProcessor; still backed by a pure stub controller for TG1. WAKE/SLEEP actions are not yet wired to the driver (owned by TG2).

## Testing

### Test Files
- test/test_MotorControl/test_Bitpack.cpp:1 — DIR pass‑through and SLEEP set/clear with examples.
- test/test_Drivers/test_Shift595Stub.cpp:1 — Verifies bytes and single latch per call (host).
- test/test_Drivers/test_Shift595Esp32.cpp:1 — Confirms latch behavior on device.

### Coverage
- Unit tests: ✅ Complete for TG1 acceptance (bit‑packing and shift→latch sequence via stub + on‑device latch sanity).
- Integration tests: ❌ Defer HW sequencing to TG2.

### Manual Testing
- Wiring: 74HC595 SER←GPIO23 (MOSI), SRCLK←GPIO18 (SCK), RCLK←GPIO5, OE→GND, MR→VCC; second 595 cascaded via QH'→SER.
- Command: `pio test -e esp32dev -f test_Drivers -v` (runs the on‑device driver test without backend).
- Expectations after `setDirSleep(0xAA,0x55)`: far 595 (DIR) = 0xAA on QA..QH; near 595 (SLEEP) = 0x55 on QA..QH. Latch pin pulses once then returns LOW.

## Standards Compliance

### Hardware abstraction
**File:** agent-os/standards/backend/hardware-abstraction.md

**Compliance:** HAL interface in `include/Hal/` with concrete drivers in `src/Drivers/`; board pins are declarative under `include/boards/`.

### Unit testing
**File:** agent-os/standards/testing/unit-testing.md

**Compliance:** Host‑first tests use a stub HAL; device‑specific checks isolated; suites are fast and focused.

## Dependencies / Integration Points
- Intended for TG2 HardwareMotorController to set DIR/SLEEP via `IShift595` before motion and for WAKE/SLEEP actions.
- No external runtime deps beyond Arduino SPI on ESP32.

## Known Issues & Limitations
- Latch delay is conservative; adjust on bench if needed.
- No SPI bus mutex yet; add if bus becomes shared.

## Performance Considerations
- Two‑byte transfers at 5 MHz; negligible impact. Single latch pulse per update.

## Security Considerations
- None beyond standard firmware concerns; no external inputs parsed at the driver level.

## Dependencies for Other Tasks
- TG2: HardwareMotorController wiring (DIR/SLEEP latch‑before‑start, WAKE/SLEEP actions, FastAccelStepper sequencing) builds directly on this HAL + driver.

## Notes
- Driver byte order is intentional: DIR first, SLEEP second → SLEEP sits in the near 595; document this for wiring clarity.
