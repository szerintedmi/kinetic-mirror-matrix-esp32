# Tech Stack

High-level choices for firmware, hardware control, host tooling, and validation for the Mirror Array product. Follow the referenced standards for configuration details and conventions.

## Firmware Platform
- MCU/Board: ESP32 DevKit (`esp32dev`)
- Framework: Arduino core for ESP32 (C++)
- Build System: PlatformIO (`platform = espressif32`, `board = esp32dev`)
- RTOS: FreeRTOS (via Arduino-ESP32)
- Filesystem: LittleFS (`board_build.filesystem = littlefs`, partition: `min_spiffs.csv`)
- Wi‑Fi onboarding: SoftAP portal (ESPAsyncWebServer) with credentials persisted in NVS
- OTA updates: ArduinoOTA with custom `OtaManager` wrapper; version injection via `tools/inject_version.py`
- Pre-build assets: `tools/gzip_fs.py` to gzip files from `data_src/` into `data/`
- Serial console: 115200 baud monitor, 460800 baud upload (see `platformio.ini`)
- Build environments:
  - `esp32DedicatedStep` — FastAccelStepper, 8 motors, dedicated STEP pins
  - `esp32SharedStep` — Shared-step RMT variant for 16+ motors (`-DUSE_SHARED_STEP=1`)
  - `native` — Host-side tests with `StubMotorController` (`-DUSE_STUB_BACKEND`)
- Reference: `@agent-os/standards/global/platformio-project-setup.md`, `@agent-os/standards/global/conventions.md`, `@agent-os/standards/global/resource-management.md`

## Hardware & Motion Control
- Drivers: DRV8825 (full-step for v1)
- Expanders: 2× 74HC595 shift registers for per-motor `DIR` and `SLEEP` via VSPI (5 MHz, MSBFIRST)
- Target: 8 steppers per node (dedicated-step); 16+ per node (shared-step RMT)
- Motion Library: FastAccelStepper 0.33.9 (MCPWM/PCNT-based STEP pulses)
- Alternate mode: SharedStepRmt for RMT-based pulse generation (16+ motors)
- DIR/SLEEP timing: FastAccelStepper external-pin callback drives 74HC595; controller delegates timing
- Wake/Sleep: Auto-enable before motion via FAS; disable on completion; OE gated at boot to ensure safe startup
- Driver selection: `AdapterFactory` selects between FasAdapterEsp32 and SharedStepAdapterEsp32 at build time
- Reference: `@agent-os/standards/backend/motion-control.md`, `@agent-os/standards/backend/shared-step-rmt.md`, `docs/esp32-74hc595-wiring.md`

## Command Protocols & Networking
- USB Serial v1: Human-readable commands using `<ACTION>[:payload]` grammar
  - Core: `HELP`, `STATUS`, `WAKE:<id|ALL>`, `SLEEP:<id|ALL>`,
    `MOVE:<id|ALL>,<abs_steps>[,<speed>][,<accel>]`,
    `HOME:<id|ALL>[,<overshoot>][,<backoff>][,<speed>][,<accel>][,<full_range>]`
  - Diagnostics: `GET LAST_OP_TIMING[:<id|ALL>]`
  - Thermal toggles: `GET THERMAL_LIMITING`, `SET THERMAL_LIMITING=OFF|ON`
  - Shortcuts supported: `ST` (STATUS), `M` (MOVE), `H` (HOME)
  - Responses prefixed with `CTRL:`; success carries `CTRL:OK` (MOVE/HOME include `est_ms=<ms>`), errors use `CTRL:ERR <code> ...`; warnings may precede `CTRL:OK` when enforcement is OFF
- MQTT (primary transport):
  - Transport: MQTT over TCP; username/password on trusted LAN for MVP; optional TLS later
  - Client (firmware): AsyncMqttClient-esphome 2.1.0
  - Client abstraction: thin `IMqttClient` adapter so we can swap to ESP‑IDF `esp-mqtt` later if TLS/mTLS or deeper tuning is needed
  - Topics/QoS (summary):
    - Presence: `devices/<id>/state` retained QoS1 with LWT
    - Status: `devices/<id>/status/<motor_id>` QoS0 on change + periodic (2–5 Hz)
    - Commands: `devices/<id>/cmd` QoS1 with strict `cmd_id` correlation; responses on `devices/<id>/cmd/resp` QoS1
  - Node policy: no command queuing; reject with `BUSY` while executing; master schedules
  - Broker: Mosquitto on developer machine for MVP; packaging for site gateway later
- HTTP: ESPAsyncWebServer 3.6.0 on port 80 (Wi-Fi portal API)
- JSON: ArduinoJson 7.4.2 for MQTT payloads and config API
- Reference: `@agent-os/standards/backend/command-pipeline.md`, `@agent-os/standards/backend/mqtt-protocol.md`, `@agent-os/standards/backend/transport-abstraction.md`, `@agent-os/standards/backend/error-codes.md`

## Storage & Presets
- On-device preset storage in LittleFS (JSON or compact text)
- Static web assets served from LittleFS as `.gz` (deterministic gzip via `tools/gzip_fs.py`, mtime=0)
- Reference: `@agent-os/standards/backend/data-persistence.md`, `@agent-os/standards/frontend/embedded-web-ui.md`

## Host Tooling
- Language: Python 3.13+, managed via Poetry
- Packaging: CLI module in `tools/mirror_cli/` with entry point `mirror-cli`
- Libraries:
  - `paho-mqtt` 2.1.0 (MQTT)
  - `pyserial` 3.5 (serial)
  - `textual` 6.6.0 (interactive TUI)
  - `rich` 14.2.0 (terminal rendering)
- Transport default: MQTT; serial selectable as debug/backdoor
- Deliverables: cross‑platform CLI with one‑shot actions and an interactive TUI that subscribes to MQTT status/events and mirrors serial behavior
- Deployment: `tools/deploy/ota_deploy.py` for multi-device parallel OTA with post-deploy verification
- Reference: `@agent-os/standards/frontend/python-cli.md`, `@agent-os/standards/testing/build-validation.md`

## Testing & Validation
- C++ unit tests: PlatformIO Unity framework, 16 test suites in `test/`
- Python tests: pytest 9.0.1, 23 test files in `tools/mirror_cli/tests/`
- Native cross-compilation: `pio test -e native` runs C++ tests on host via stub backend
- On-device tests: `pio test -e esp32DedicatedStep`
- Hardware validation on bench (bring-up, homing, thermal/current sanity)
- Build validation across all three environments in `platformio.ini`
- Reference: `@agent-os/standards/testing/unit-testing.md`, `@agent-os/standards/testing/hardware-validation.md`, `@agent-os/standards/testing/build-validation.md`

## Linting & Code Quality
- C++: clang-tidy + cppcheck via `pio check`; clang-format via `tools/clang_format_check.py`
- Python: Ruff 0.14.6 for linting and formatting (`scripts/python_lint.sh`, `scripts/python_format.sh`)
- Reference: `@agent-os/standards/global/coding-style.md`

## Frontend Surfaces
- Embedded Web UI: minimal static HTML/CSS/JS, gzipped and served from LittleFS via ESPAsyncWebServer
- Primary control surface: MQTT via Python TUI; serial remains a debug path
- Reference: `@agent-os/standards/frontend/embedded-web-ui.md`

## Firmware Library Versions
- FastAccelStepper: 0.33.9
- ESPAsyncWebServer: 3.6.0
- AsyncTCP: 3.4.9
- ArduinoJson: 7.4.2
- AsyncMqttClient-esphome: 2.1.0
- Pin all versions in `platformio.ini` for reproducible builds
- Reference: `@agent-os/standards/global/platformio-project-setup.md`
