# Tech Stack

High-level choices for firmware, hardware control, host tooling, and validation for the Mirror Array product. Follow the referenced standards for configuration details and conventions.

## Firmware Platform
- MCU/Board: ESP32 DevKit (`esp32dev`)
- Framework: Arduino core for ESP32 (C++)
- Build System: PlatformIO (`platform = espressif32`, `board = esp32dev`)
- RTOS: FreeRTOS (via Arduino-ESP32)
- Filesystem: LittleFS for on-device assets and presets
- Wi‑Fi onboarding: SoftAP portal with credentials persisted in NVS (Preferences)
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
- MQTT (primary from roadmap item 9):
  - Transport: MQTT over TCP; username/password on trusted LAN for MVP; optional TLS later
  - Client (firmware): AsyncMqttClient (Arduino) for MVP
  - Client abstraction: thin `IMqttClient` adapter so we can swap to ESP‑IDF `esp-mqtt` later if TLS/mTLS or deeper tuning is needed
  - Topics/QoS (summary):
    - Presence: `devices/<id>/state` retained QoS1 with LWT
    - Status: `devices/<id>/status/<motor_id>` QoS0 on change + periodic (2–5 Hz)
    - Commands: `devices/<id>/cmd` QoS1 with strict `cmd_id` correlation; responses on `devices/<id>/cmd/resp` QoS1
  - Node policy: no command queuing; reject with `BUSY` while executing; master schedules
  - Broker: Mosquitto on developer machine for MVP; packaging for site gateway later
  - Reference: `agent-os/product/roadmap.md` (items 8–11, 14–15)

## Storage & Presets
- On-device preset storage in LittleFS (JSON or compact text)
- Static web assets (if used) served from LittleFS as `.gz`
- Reference: `@agent-os/standards/backend/data-persistence.md`, `@agent-os/standards/frontend/embedded-web-ui.md`

## Host Tooling
- Language: Python 3 (CLI/TUI for MQTT and serial)
- Packaging: CLI module in repo (keeps serial and MQTT transports)
- Libraries: `paho-mqtt` (MQTT), `pyserial` (serial), `argparse`
- Transport default: MQTT from roadmap item 9; serial selectable as debug/backdoor
- Deliverables: cross‑platform CLI with one‑shot verbs and an interactive TUI that subscribes to MQTT status/events and mirrors serial behavior
- Reference: `agent-os/product/roadmap.md` (items 8–11, 13–15), `@agent-os/standards/testing/build-validation.md`

## Testing & Validation
- Unit tests via PlatformIO Unity where applicable
- Hardware validation on bench (bring-up, homing, thermal/current sanity)
- Build validation across environments in `platformio.ini`
- Reference: `@agent-os/standards/testing/unit-testing.md`, `@agent-os/standards/testing/hardware-validation.md`, `@agent-os/standards/testing/build-validation.md`

## Frontend Surfaces (Optional)
- Embedded Web UI (if enabled): minimal static HTML/CSS/JS, gzipped and served from LittleFS
- Primary control surface becomes MQTT from roadmap item 9; serial remains a low‑priority debug path
- Reference: `@agent-os/standards/frontend/serial-interface.md`, `@agent-os/standards/frontend/embedded-web-ui.md`

## Versioning & Pins
- Pin framework, libraries, and tool versions in `platformio.ini` for reproducible builds
- Track library adds (e.g., FastAccelStepper) in `platformio.ini` with exact versions
- Reference: `@agent-os/standards/global/platformio-project-setup.md`
