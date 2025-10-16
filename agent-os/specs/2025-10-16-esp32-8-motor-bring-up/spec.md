# Specification: ESP32 8‑Motor Bring‑Up (FastAccelStepper + 74HC595)

## Goal
Drive eight DRV8825 stepper drivers from a single ESP32 using FastAccelStepper for motion and two daisy‑chained 74HC595 shift registers (via hardware SPI) for per‑motor DIR and SLEEP control, fully behind the existing serial protocol.

## User Stories
- As a builder, I can wire eight steppers and run concurrent moves from the same serial commands used in v1.
- As a developer, I can rely on auto‑sleep and reject overlapping commands, avoiding overheating while preserving simple control.
- As an operator, I can verify motion with STATUS and the Python CLI without changing any commands.

## Core Requirements
### Functional Requirements
- Hardware control:
  - STEP: eight dedicated ESP32 GPIO outputs mapped to motors 0–7.
  - DIR/SLEEP: two chained 74HC595s (16 outputs) driven by hardware SPI (VSPI) to provide DIR[i] and SLEEP[i].
  - SLEEP vs ENABLE: keep `nENBL` tied LOW (always enabled). Control low‑power via the DRV8825 SLEEP pin.
- Motion:
  - Use FastAccelStepper for each motor; defaults: speed 4000 steps/s, accel 16000 steps/s^2.
  - Concurrency: support concurrent moves; ensure shift+latch completes before starting motion.
  - DIR latch at motion start only; do not toggle mid‑move.
  - Auto‑sleep: use FastAccelStepper’s built‑in auto enable/sleep; protocol WAKE/SLEEP treated as diagnostic overrides.
- Protocol compatibility:
  - Reuse the existing grammar and BUSY rules from item 1; reject new MOVE/HOME targeting motors already moving.
  - STATUS remains minimal (id, pos, speed, accel, moving, awake) and reflects hardware state.

### Non-Functional Requirements
- Full‑step only (MS1..MS3 hard‑wired to 000).
- Robust startup: latch initial DIR/SLEEP states before enabling motion.
- Avoid strapping‑sensitive pins for STEP; pick stable output‑capable GPIOs.

## Visual Design
- No visuals provided for this spec.

## Reusable Components
### Existing Code to Leverage
- Protocol layer and command handlers (v1).
- `MotorBackend` abstraction and `StubBackend` implementation.
- Serial console layer (echo, line parsing) and Python CLI for E2E.

### New Components Required
- `HardwareBackend` implementing `MotorBackend` with:
  - FastAccelStepper instances for 8 motors (STEP pins).
  - SPI driver for 2×74HC595 with RCLK latch GPIO; byte 0 → DIR bits, byte 1 → SLEEP bits.
  - Mapping layer for WAKE/SLEEP overrides while leaving auto‑sleep active by default.
  - Safe sequencing: update 595 buffers, latch, then start motion.

## Technical Approach
- Firmware (ESP32/Arduino):
  - Configure VSPI (`SCK=GPIO18`, `MOSI=GPIO23`, `MISO=GPIO19` unused) and `RCLK/LATCH=GPIO5`.
  - STEP pins suggested: `GPIO4, 16, 17, 25, 26, 27, 32, 33` → motors 0..7.
  - 74HC595 bit layout:
    - Byte 0 (DIR): bit i = DIR[i] (1=forward), i∈[0..7]
    - Byte 1 (SLEEP): bit i = SLEEP[i] (1=awake/high, 0=sleep/low)
  - Initialize 595 (OE→GND, MR→VCC) and set initial SLEEP=1 for all; latch before creating motion tasks.
  - On MOVE for a motor set:
    1) Compute DIR for each target (once), set DIR bits; set SLEEP bits = awake for targets
    2) Shift+Latch 2 bytes (LSB/MSB ordering fixed in driver), then start FastAccelStepper moves
    3) On completion, rely on auto‑sleep; ensure STATUS reflects awake=0 after motion
  - BUSY handling: check any targeted motor’s stepper is active; if so, return `CTRL:ERR E04 BUSY`.
- Testing:
  - Build validation via PlatformIO (`esp32dev`).
  - Unit tests for sequencing (simulated) remain under native; hardware timing validated on bench.

## Out of Scope
- Microstepping control, current limiting, telemetry/fault detail (future items).
- Precise cross‑motor synchronization beyond concurrent start after latch.

## Success Criteria
- Builds and runs on ESP32 DevKit with 8 motors wired.
- Concurrent MOVE operations execute reliably with correct DIR latching and no mid‑move toggles.
- STATUS reports live position, speed, accel, moving, awake and responds to WAKE/SLEEP overrides.
- Python CLI (help/status/wake/sleep/move/home) works unchanged for bring‑up.
