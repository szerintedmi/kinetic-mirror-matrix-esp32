# Tech Stack

High-level choices for firmware, hardware control, host tooling, and validation for the Mirror Array product. Follow the referenced standards for configuration details and conventions.

## Firmware Platform
- MCU/Board: ESP32 DevKit (`esp32dev`)
- Framework: Arduino core for ESP32 (C++)
- Build System: PlatformIO (`platform = espressif32`, `board = esp32dev`)
- RTOS: FreeRTOS (via Arduino-ESP32)
- Filesystem: LittleFS for on-device assets and presets
- Pre-build assets: `tools/gzip_fs.py` to gzip files from `data_src/` into `data/`
- Serial console: 115200 baud (see `platformio.ini`)
- Reference: `@agent-os/standards/global/platformio-project-setup.md`, `@agent-os/standards/global/conventions.md`, `@agent-os/standards/global/resource-management.md`

## Hardware & Motion Control
- Drivers: DRV8825 (full-step for v1)
- Expanders: 2× 74HC595 shift registers for per-motor `DIR` and `SLEEP` (ENABLE semantics)
- Target: 8 steppers per node (initial cluster)
- Motion Library: FastAccelStepper (in use)
- DIR/SLEEP timing: FastAccelStepper external-pin callback drives 74HC595; controller delegates timing
- Wake/Sleep: Auto-enable before motion via FAS; disable on completion; OE gated at boot to ensure safe startup
- Reference: `agent-os/product/roadmap.md` (items 2–3), `@agent-os/standards/backend/hardware-abstraction.md`, `@agent-os/standards/backend/task-structure.md`, `docs/esp32-74hc595-wiring.md`

## Command Protocols & Networking
- USB Serial v1: Human-readable commands using `<VERB>[:payload]` grammar
  - Core: `HELP`, `STATUS`, `WAKE:<id|ALL>`, `SLEEP:<id|ALL>`,
    `MOVE:<id|ALL>,<abs_steps>[,<speed>][,<accel>]`,
    `HOME:<id|ALL>[,<overshoot>][,<backoff>][,<speed>][,<accel>][,<full_range>]`
  - Diagnostics: `GET LAST_OP_TIMING[:<id|ALL>]`
  - Thermal toggles: `GET THERMAL_LIMITING`, `SET THERMAL_LIMITING=OFF|ON`
  - Shortcuts supported: `ST` (STATUS), `M` (MOVE), `H` (HOME)
  - Responses prefixed with `CTRL:`; success carries `CTRL:OK` (MOVE/HOME include `est_ms=<ms>`), errors use `CTRL:ERR <code> ...`; warnings may precede `CTRL:OK` when enforcement is OFF
- Wireless (planned): ESP‑Now broadcast for master→nodes (no acks/retries initially)
- Node addressing and grouping introduced with wireless phase
- Reference: `agent-os/product/roadmap.md` (items 1, 4–5, 7–8)

## Storage & Presets
- On-device preset storage in LittleFS (JSON or compact text)
- Static web assets (if used) served from LittleFS as `.gz`
- Reference: `@agent-os/standards/backend/data-persistence.md`, `@agent-os/standards/frontend/embedded-web-ui.md`

## Host Tooling
- Language: Python 3 (CLI for serial tests, diagnostics, and later presets)
- Packaging: `python -m serial_cli` (module in `tools/serial_cli/`)
- Libraries: `pyserial`, `argparse`
- Deliverables: cross‑platform CLI with one‑shot commands and an interactive TUI (`interactive`) that polls STATUS and surfaces warnings alongside `CTRL:OK`
- Reference: `agent-os/product/roadmap.md` (items 1, 4–5), `@agent-os/standards/testing/build-validation.md`

## Testing & Validation
- Unit tests via PlatformIO Unity where applicable
- Hardware validation on bench (bring-up, homing, thermal/current sanity)
- Build validation across environments in `platformio.ini`
- Reference: `@agent-os/standards/testing/unit-testing.md`, `@agent-os/standards/testing/hardware-validation.md`, `@agent-os/standards/testing/build-validation.md`

## Frontend Surfaces (Optional)
- Embedded Web UI (if enabled): minimal static HTML/CSS/JS, gzipped and served from LittleFS
- Serial interface remains the primary control surface in v1
- Reference: `@agent-os/standards/frontend/serial-interface.md`, `@agent-os/standards/frontend/embedded-web-ui.md`

## Versioning & Pins
- Pin framework, libraries, and tool versions in `platformio.ini` for reproducible builds
- Track library adds (e.g., FastAccelStepper) in `platformio.ini` with exact versions
- Reference: `@agent-os/standards/global/platformio-project-setup.md`
