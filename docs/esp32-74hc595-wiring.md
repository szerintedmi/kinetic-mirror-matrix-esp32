# ESP32 + 74HC595 + DRV8825 Pinout and Wiring Guide

This project drives eight DRV8825 stepper drivers using FastAccelStepper for STEP generation and two daisy‑chained 74HC595 shift registers for per‑motor DIR and SLEEP. This guide consolidates wiring details and the GPIO constants used in the codebase.

## GPIO Constants (board map)

- File: `include/boards/Esp32Dev.hpp`
  - VSPI hardware pins
    - `VSPI_SCK = 18`
    - `VSPI_MOSI = 23`
    - `VSPI_MISO = 19` (unused)
    - `VSPI_SS   = -1` (unused)
  - Latch and Output Enable
    - `SHIFT595_RCLK = 5` (74HC595 RCLK / ST_CP)
    - `SHIFT595_OE   = 22` (OE, active‑low, shared by both 595s)
  - STEP pins for motors `0..7`
    - `STEP_PINS[8] = {32, 25, 27, 13, 4, 17, 19, 21}`

## 74HC595 Integration (SPI + Latch)

- Driver: `src/drivers/Esp32/Shift595Vspi.cpp` / `include/drivers/Esp32/Shift595Vspi.h`
  - Uses VSPI with SCK=`GPIO18`, MOSI=`GPIO23`
  - Latch (RCLK) on `GPIO5`
  - OE on `GPIO22` (active‑low). Firmware keeps OE high at boot, latches a known state (all sleep), then enables outputs (OE low).
  - Transfer order: DIR byte first, then SLEEP byte, then RCLK rising edge to latch outputs.
    - In a standard cascade: the first byte ends up on the “far” chip; the second on the “near” chip.
    - Project assumption: Far chip = DIR outputs; Near chip = SLEEP outputs.

### Daisy‑Chain Wiring

- 595 #1 (near MCU)
  - SER (pin 14) ← ESP32 MOSI (GPIO23)
  - SRCLK (pin 11) ← ESP32 SCK (GPIO18)
  - RCLK (pin 12) ← ESP32 RCLK (GPIO5)
- 595 #2 (far)
  - SER (pin 14) ← QH' (pin 9) of 595 #1
  - SRCLK (pin 11) ← ESP32 SCK (GPIO18)
  - RCLK (pin 12) ← ESP32 RCLK (GPIO5)
- Both 595s
  - OE (pin 13) ← ESP32 (GPIO22), shared and tied high via ~10k pull‑up
  - MR (pin 10) → VCC (HIGH)
  - VCC → 3.3 V (recommended) and GND (each with local 100 nF decoupling)

## FastAccelStepper External Pins (DIR/SLEEP)

- Adapter: `src/drivers/Esp32/FasAdapterEsp32.cpp` / `src/drivers/Esp32/FasAdapterEsp32.h`
  - Registers `engine.setExternalCallForPin(...)` to receive pin changes.
  - Virtual pin mapping:
    - DIR virtual pins: `DIR_BASE + id | PIN_EXTERNAL_FLAG` (base 0..7)
    - SLEEP virtual pins: `SLEEP_BASE + id | PIN_EXTERNAL_FLAG` (base 32..39)
  - Callback sets two bytes (DIR, SLEEP) and calls `IShift595::setDirSleep(dir_bits, sleep_bits)` which shifts both bytes then latches.
  - Direction sense: `setDirectionPin(..., /*dirHighCountsUp=*/true)` by default (flip if wiring requires).
  - Enable delay: `setDelayToEnable(2000)` µs (typical DRV8825 wake 1–1.5 ms; 1500–2000 µs is conservative).

## STEP Generation

- STEP pins are connected directly from ESP32 (`STEP_PINS[]`) to each DRV8825 `STEP` input.
- Configuration is in the adapter via `engine.stepperConnectToPin(step_gpio)`; speed/accel are cached and applied on change.

## Bring‑Up Checklist (No Scope)

1. Power & control (both 595s)
   - VCC = 3.3 V (recommended), GND common, MR HIGH, OE LOW (enabled) after first latch.
2. Chain signals
   - SRCLK (SCK GPIO18) and RCLK (GPIO5) wired to BOTH 595s.
   - QH' (pin 9) of near 595 wired to SER (pin 14) of far 595.
3. Sanity tests over serial
   - `WAKE:<id>`: SLEEP Q for that motor goes HIGH immediately.
   - `SLEEP:<id>`: SLEEP Q goes LOW immediately.
   - `MOVE:<id>,P+200` then `MOVE:<id>,P-200`: DIR line flips between moves; STEP pulses appear on that motor’s STEP pin.
4. Polarity & mapping
   - If motor moves only one way but DIR toggles: flip `dirHighCountsUp` in the adapter.
   - If DIR/SLEEP bits reach the wrong Q: review cascade order or adjust mapping.

## Relevant Files (quick links)

- Board map and pins: `include/boards/Esp32Dev.hpp`
- 74HC595 VSPI driver: `src/drivers/Esp32/Shift595Vspi.cpp`, `include/drivers/Esp32/Shift595Vspi.h`
- FAS adapter (external pins): `src/drivers/Esp32/FasAdapterEsp32.cpp`, `src/drivers/Esp32/FasAdapterEsp32.h`
- Controller (backend): `lib/MotorControl/src/HardwareMotorController.cpp`

---

Tip: 74HC595 at 5 V requires higher logic thresholds; with a 3.3 V ESP32, prefer 3.3 V VCC on the shift registers or use 74HCT595.

## Driver Compatibility and Current Limit (Vref)

- All drivers' Vref must be tuned to the motor's safe current limit. Set and verify Vref for every driver channel before running moves; follow each driver's datasheet/board guide for the correct formula and sense-resistor value.
- Compatibility: the wiring works with A4988 drivers (tested) and likely with TMC2209 (not tested). However, for our small motor (<100 mA peak), the A4988's current limiting was not accurate enough to reliably enforce such a low current; we could not tune it to stay within our low limits. Prefer DRV8825 (or a driver with finer current control) for very low-current motors.
