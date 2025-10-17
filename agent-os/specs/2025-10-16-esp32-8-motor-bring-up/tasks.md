# Task Breakdown: ESP32 8‑Motor Bring‑Up (FastAccelStepper + 74HC595)

## Overview
Total Tasks: 18
Assigned roles: api-engineer, testing-engineer

Notes:
- Clear separation: first build and test the SPI/74HC595 driver in isolation, then integrate it into the motion backend. Each group has self-contained acceptance criteria.
- Reuse protocol from item 1; add a compile-time/backend switch to select HardwareBackend.

## Task List

### SPI Driver & Bit Packing (Isolated, self-testable)

#### Task Group 1: 74HC595 driver (VSPI) + mapping helpers
Assigned implementer: api-engineer
Dependencies: None
Standards: `@agent-os/standards/backend/hardware-abstraction.md`, `@agent-os/standards/testing/unit-testing.md`

- [x] 1.0 Complete SPI driver & helpers (no FastAccelStepper dependency)
  - [x] 1.1 Write 2-8 focused unit tests (native/sim)
    - Verify bit packing: DIR in byte0, SLEEP in byte1; SLEEP polarity: 1=awake/high
    - Verify latch sequence: shift → latch; no extra toggles
    - Limit to 2-8 tests (target 4)
  - [x] 1.2 Implement low-level VSPI driver
    - Init VSPI (`SCK=GPIO18`, `MOSI=GPIO23`, `MISO=GPIO19` unused), `RCLK=GPIO5`
    - Provide `set_dir_sleep(dir_bits, sleep_bits)` API
  - [x] 1.3 Provide mapping helpers
    - `compute_dir_bits(target_targets)` and `compute_sleep_bits(target_targets, awake)`
  - [x] 1.4 Ensure tests pass

Acceptance Criteria (independent):
- Given target selections and WAKE/SLEEP overrides, unit tests verify `compute_dir_bits` and `compute_sleep_bits` produce expected bitmasks for motors 0–7
- With a stubbed SPI interface, tests capture and assert that the exact two bytes shifted/latch sequence matches the computed DIR (byte0) and SLEEP (byte1) bits (i.e., 74HC595 outputs reflect intended command effects)
- API is usable standalone (no dependency on motion backend)

### Firmware Backend Layer (Integration, uses TG1 API)

#### Task Group 2: HardwareBackend integration (FastAccelStepper + 74HC595 API)
Assigned implementer: api-engineer
Dependencies: Task Group 1
Standards: `@agent-os/standards/backend/hardware-abstraction.md`, `@agent-os/standards/global/resource-management.md`, `@agent-os/standards/testing/unit-testing.md`

- [x] 2.0 Implement HardwareBackend behind MotorBackend
  - [x] 2.1 Write 2-8 focused unit tests (native/sim) for backend sequencing
    - Use a mocked 74HC595 driver to assert: latch-before-start, correct bits per target, and WAKE/SLEEP overrides
    - Verify busy rule on overlapping MOVE; DIR latched once per move
    - Limit to 2-8 tests (target 6)
  - [x] 2.2 Configure FastAccelStepper for 8 motors (STEP pins)
    - Map STEP pins: `GPIO4,16,17,25,26,27,32,33`
    - Defaults: speed=4000, accel=16000 (override via protocol)
  - [x] 2.3 Enforce sequencing per move
    - Arduino: FastAccelStepper external-pin callback sets DIR/SLEEP with proper timing; backend starts steppers
    - Native: emulate latch-before-start via mocked 74HC595 for unit tests
  - [x] 2.4 Map WAKE/SLEEP protocol verbs as overrides
    - Keep FastAccelStepper auto-enable; WAKE uses enableOutputs (forced awake), SLEEP returns to auto mode; callback reflects SLEEP
  - [x] 2.5 Integrate backend selection
    - Provide compile-time switch to choose `StubBackend` vs `HardwareBackend`
  - [x] 2.6 Ensure backend unit tests pass
    - Run ONLY tests from 2.1

Acceptance Criteria (independent of TG3):
- Backend unit tests pass with a mocked 74HC595 driver (no hardware required)
- Busy rule and latch-before-start validated via mocks
- For MOVE requests, tests verify the backend issues FastAccelStepper start calls for targeted motors with the correct speed/accel (mocked FAS adapter), and on bench we observe STEP pulses generated on the configured STEP pins (validated in TG3 checklist)

### Build & Bench Validation

#### Task Group 3: Build validation & bench checklist
Assigned implementer: testing-engineer
Dependencies: Task Groups 1–2
Standards: `@agent-os/standards/testing/build-validation.md`, `@agent-os/standards/testing/hardware-validation.md`

- [ ] 3.0 Validate builds and run feature-specific tests only
  - [x] 3.1 Build for `esp32dev` via PlatformIO
  - [x] 3.2 Run ONLY unit tests from Task Groups 1–2
  - [x] 3.3 Create bench checklist (manual)
    - Verified DIR/SLEEP via logic probe/LEDs while issuing MOVE/WAKE/SLEEP (see `docs/esp32-74hc595-wiring.md`)
    - Confirmed concurrent MOVE on 2+ motors; DIR settles before pulses; STATUS reflects motion concurrently
    - Confirmed auto-sleep after completion and STATUS awake=0
    - Validated partial population: MOVE:ALL with only some motors connected shows no jitter on connected motors; DIR/SLEEP update on all outputs

Acceptance Criteria:
- `pio run -e esp32dev` succeeds
- Unit tests from 1–2 pass; bench checklist documented and followed during bring-up
- Bench verifies: (a) 74HC595 outputs (DIR/SLEEP) reflect commands immediately after latch; (b) FastAccelStepper produces STEP pulses for MOVE on selected motors and auto-sleeps after completion

## Execution Order
1. SPI Driver & Bit Packing (Task Group 1)
2. HardwareBackend integration (Task Group 2)
3. Build & Bench Validation (Task Group 3)
