# Spec Requirements: ESP32 8‑Motor Bring‑Up (FastAccelStepper + 74HC595)

## Initial Description
move on to next roadmap item

## Requirements Discussion

### First Round Questions

**Q1:** Pins and buses: Do you prefer using hardware SPI for the 74HC595 chain (with `SRCLK`, `RCLK`, `SER`, optional `OE`), or bit-banged GPIOs? Please provide desired GPIO numbers for STEP[0..7], and for 595: DATA, SRCLK, RCLK, and optionally OE (active low).
**Answer:** 1. SPI - please use the SPI for '595 . For Motors: pick reasonable STEP output GPIOs (hardware wiring can follow)

**Q2:** DIR + ENABLE via 74HC595: Confirm 2 chained 595s → 16 outputs, mapped as 8x DIR and 8x ENABLE. Any preferred output order/bit mapping (e.g., [M0..M7] DIR first then ENABLE)? ENABLE polarity is active-low on DRV8825 (`nENBL`); confirm we’ll drive low to enable.
**Answer:** 2. yes 2 chained 595s. no preference on DIR/SLEEP order.  Important clarification: we don't disable the driver but we put to sleep!  nEnable is pulled low by wires. we want to drive the SLEEP pin. Clarify requirements too if it's misleading there

**Q3:** FastAccelStepper integration: Use library’s auto-enable/auto-sleep per motor, mapping to our WAKE/SLEEP semantics (i.e., WAKE/SLEEP act as overrides/diagnostics, not required in normal flow)? OK to keep protocol WAKE/SLEEP unchanged?
**Answer:** 3. yes, use fastaccelstepper built in API support for auto sleep. WAKE/SLEEP is diag only override (mostly for driver vref tuning) 

**Q4:** DIR latch semantics: Roadmap says latch DIR at motion start only. Confirm that for absolute moves we compute DIR once per move and do not toggle mid-move, even if the target crosses zero (should not for absolute).
**Answer:** 4. yes, no DIR change during move. if new command arrives while a previous move is still executing we can reject it with an Error code

**Q5:** Stepping mode: DRV8825 set to full-step (MS1..MS3 pulled to 000). Should we hard-wire MS pins or control them via extra 595 bits? I propose hard-wire to full-step for v1.
**Answer:** 5. yes, full step hard wired, no need to control it from MCU

**Q6:** Timing and concurrency: Target concurrent motion across all 8 motors. Any constraints on 595 update cadence vs. STEP pulse generation (e.g., min latch-to-step delay)? I’ll ensure SR shifting + latch completes before first step on affected motors.
**Answer:** 6. yes we need to be able to move motors concurently. we don't need to sync their moves but yes, we must be sure SR shift done before we start moves.

**Q7:** Default speeds: Reuse protocol defaults (speed=4000 steps/s, accel=16000 steps/s^2) for FastAccelStepper, unless overridden by command. OK?
**Answer:** 7. yes, those defaults are tested with fastaccelstepper

**Q8:** Fault handling: For v1 bring-up, treat driver faults as opaque (no readback). If a channel stalls or hits thermal, we won’t have feedback. OK to keep STATUS minimal (id, pos, speed, accel, moving, awake) and add diagnostics in roadmap item 4?
**Answer:** 8. yes

**Q9:** Backend abstraction: I’ll implement `HardwareBackend` (FastAccelStepper + 595) under the existing `MotorBackend` interface so we can switch from `StubBackend` cleanly. OK?
**Answer:** 9. ok

### Follow-up Clarifications
- SLEEP vs ENABLE: For DRV8825, keep `nENBL` pulled low (always enabled). Control low-power via `SLEEP` pin instead. WAKE/SLEEP protocol actions remain as overrides (diagnostics); normal motion uses FastAccelStepper auto-sleep.
- SPI usage: Use hardware SPI (VSPI) to shift 2×74HC595 (16 outputs). `OE` tied low, `MR` tied high; we drive `RCLK` (latch) via a GPIO.

## Visual Assets

### Files Provided:
No visual files found.

### Visual Insights:
None.

## Requirements Summary

### Functional Requirements
- Drive eight DRV8825 steppers from one ESP32:
  - `STEP[0..7]` on eight dedicated GPIOs (output-capable; avoid strapping pins).
  - 2×74HC595 daisy-chained via hardware SPI (VSPI) to generate per-motor `DIR` and `SLEEP` outputs (not `ENABLE`).
- Motion backend uses FastAccelStepper with defaults `speed=4000`, `accel=16000` when not overridden.
- Concurrency: Run multiple motors concurrently; no frame sync required.
- Safety/power: Latch `DIR` at motion start only. Ensure 595 shift + latch completes before starting motion.
- Protocol compatibility: Reuse existing protocol semantics; reject `MOVE`/`HOME` with `BUSY` if a prior motion targets the same motor(s).

### Non-Functional Requirements
- Full-step mode hard-wired (MS1..MS3 = 000); no MCU control of microstepping in v1.
- WAKE/SLEEP protocol actions act as diagnostic overrides; use FastAccelStepper’s auto-sleep in normal operation.
- Keep STATUS minimal in v1 (id, pos, speed, accel, moving, awake).

### Proposed Pin Map (subject to wiring)
- VSPI (hardware SPI): `SCK=GPIO18`, `MOSI=GPIO23`, `MISO=GPIO19` (unused), `RCLK/LATCH=GPIO5`.
- STEP pins (output): `GPIO4, GPIO16, GPIO17, GPIO25, GPIO26, GPIO27, GPIO32, GPIO33` mapped to motors `0..7` respectively.
- 74HC595 outputs (bit mapping):
  - Byte 0 (DIR): bit `i` → `DIR[i]` for motor `i` (0..7), `1=forward`, `0=reverse` (logical convention in firmware).
  - Byte 1 (SLEEP): bit `i` → `SLEEP[i]` for motor `i` (0..7), `1=awake (SLEEP high)`, `0=sleep (SLEEP low)`.
- 74HC595 wiring: `OE`→GND (enabled), `MR`→VCC (not reset by MCU).

### Scope Boundaries
**In Scope:**
- HardwareBackend integrating FastAccelStepper and SPI-driven 74HC595 for DIR/SLEEP.
- Concurrent motion, per-motor DIR at start only, auto-sleep via library.

**Out of Scope:**
- Microstepping control, current limiting, thermal telemetry, detailed fault codes (roadmap item 4+).
- Exact synchronization across motors beyond “concurrent start after latch”.

### Technical Considerations
- Ensure `RCLK` latch occurs before FastAccelStepper start to avoid glitches.
- Handle polarity for `SLEEP` correctly for DRV8825: drive HIGH to wake, LOW to sleep.
- Keep `nENBL` hard-wired LOW (always enabled) per directive.
- Maintain `MotorBackend` abstraction to swap between `StubBackend` and `HardwareBackend` without changing protocol layer.

