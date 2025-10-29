# Kinetic Mirror Matrix (ESP32)

Firmware + host tools to drive up to 8 stepper‑driven mirrors from a single ESP32 using FastAccelStepper for motion and two 74HC595 shift registers for per‑motor DIR and SLEEP. A simple human‑readable serial protocol provides immediate control from any laptop; diagnostics and thermal run‑time limits keep demos stable and safe.

- [Roadmap](./agent-os/product/roadmap.md)
- [Tech stack](./agent-os/product/tech-stack.md)
- [Wiring guide](./docs/esp32-74hc595-wiring.md)

## What It Does

- Exposes a USB serial protocol (v1) with commands: HELP, STATUS, MOVE, HOME, WAKE, SLEEP
- Drives 8 DRV8825 steppers concurrently (full‑step for v1). DIR and SLEEP are via 74HC595 shift registers to reduce GPIO use.
- Auto-sleeps motors by default to avoid overheating and to reduce power consumption.
- Implements bump‑stop homing, zeroing at midpoint
- Reports per‑motor status including homed, steps since home, and thermal budget metrics
- Enforces runtime/cooldown budgets; toggleable at runtime for lab work
- Ships a Python CLI for quick tests and an interactive TUI view

## Features (High‑Level)

- Serial protocol v1 with stable grammar and shortcuts
  - See: [MotorCommandProcessor.cpp](./lib/MotorControl/src/MotorCommandProcessor.cpp), [Serial command protocol v1 spec](./agent-os/specs/2025-10-15-serial-command-protocol-v1/spec.md)
- Wi‑Fi onboarding library with serial/MQTT-ready commands, SoftAP portal, and device status feedback
  - See: [NetOnboarding.cpp](./lib/net_onboarding/src/NetOnboarding.cpp), portal assets in [data_src/net](./data_src/net), spec: [Wi‑Fi Onboarding via NVS](./agent-os/specs/2025-10-28-wifi-onboarding-via-nvs/spec.md)
- 8‑motor hardware bring‑up via FastAccelStepper + 2×74HC595 (DIR/SLEEP)
  - See: [FasAdapterEsp32.cpp](./src/drivers/Esp32/FasAdapterEsp32.cpp), [Shift595Vspi.cpp](./src/drivers/Esp32/Shift595Vspi.cpp), [Esp32Dev.hpp](./include/boards/Esp32Dev.hpp)
- Homing & baseline calibration (bump‑stop)
  - See: [HardwareMotorController.cpp](./lib/MotorControl/src/HardwareMotorController.cpp), spec: [Homing & baseline calibration spec](./agent-os/specs/2025-10-17-homing-baseline-calibration-bump-stop/spec.md)
- Status & diagnostics (homed, steps_since_home, budget_s, ttfc_s)
  - See: [MotorCommandProcessor.cpp](./lib/MotorControl/src/MotorCommandProcessor.cpp), spec: [Status & diagnostics spec](./agent-os/specs/2025-10-17-status-diagnostics/spec.md)
- Thermal limits enforcement (preflight checks, WARN/ERR, last‑op timing)
  - See: [MotorCommandProcessor.cpp](./lib/MotorControl/src/MotorCommandProcessor.cpp), [MotorControlConstants.h](./lib/MotorControl/include/MotorControl/MotorControlConstants.h), spec: [Thermal limits enforcement spec](./agent-os/specs/2025-10-17-thermal-limits-enforcement/spec.md)
- Host CLI (Python) incl. interactive TUI
  - See: [tools/serial_cli](./tools/serial_cli/)

## Architecture

```mermaid
flowchart LR
  subgraph "Host PC"
    CLI["serial_cli (Component)"]
  end

  subgraph "ESP32 Firmware"
    Console["SerialConsole"]
    MCP["MotorCommandProcessor"]
    MC["MotorController"]
    HWC["HardwareMotorController"]
    STUB["StubMotorController"]
    FAS["FasAdapterEsp32"]
    FASLib["FastAccelStepper (Library)"]
    SR["Shift595Vspi"]
  end

  subgraph "Hardware"
    DRV["DRV8825 Drivers"]
    SRIC["74HC595 Shift Registers"]
    Motors["Stepper Motors 0..7"]
  end

  CLI -->|USB Serial| Console
  Console --> MCP
  MCP --> MC
  MC --> HWC
  MC --> STUB
  HWC --> FAS
  HWC --> SR
  FAS -->|uses| FASLib
  FAS -->|STEP GPIO| DRV
  SR -->|VSPI| SRIC
  SRIC -->|DIR/SLEEP lines| DRV
  DRV --> Motors
```

### MOVE/HOME Command Flow

```mermaid
sequenceDiagram
  participant CLI as Host CLI
  participant Dev as SerialConsole ESP32
  participant MCP as MotorCommandProcessor
  participant CTR as MotorController HW/Stub
  participant FAS as FastAccelStepper
  participant SR as Shift595 DIR-SLEEP
  CLI->>Dev: MOVE:0,1000,4000,16000
  Dev->>MCP: parse line
  MCP->>CTR: tick(now) and preflight checks
  alt OK or WARN (limits OFF)
    MCP-->>CLI: CTRL:OK est_ms=...
    MCP->>CTR: moveAbsMask(mask,...)
    CTR->>SR: set DIR/SLEEP and latch
    CTR->>FAS: startMoveAbs(id,target,...)
    FAS-->>CTR: running
    CTR-->>MCP: update last-op timing
    Note over CTR: On completion — auto-sleep, update homed/steps
  else ERR (limits ON)
    MCP-->>CLI: CTRL:ERR E10/E11
  end
```

## Quickstart

Prereqs

- PlatformIO Core (CLI)
- Python 3.9+ (for host CLI)

Build and Upload (ESP32)

- Configure your board pins/wiring: [Esp32Dev.hpp](./include/boards/Esp32Dev.hpp), [wiring guide](./docs/esp32-74hc595-wiring.md)
- Build firmware: `pio run -e esp32dev`
- Upload firmware: `pio run -e esp32dev -t upload`
- Build portal filesystem image: `pio run -e esp32dev -t buildfs`
- Upload portal filesystem: `pio run -e esp32dev -t uploadfs`
- Monitor: `pio device monitor -b 115200`
  - On boot you should see: `CTRL:READY Serial v1 — send HELP`

Host CLI

- From repo root: `python -m serial_cli --help`

- Examples:
  - `python -m serial_cli interactive --port /dev/ttyUSB0`  #  polls and displays STATUS at ~2 Hz, displays device responses to commands
  - `python -m serial_cli help --port /dev/ttyUSB0`
  - `python -m serial_cli status --port /dev/ttyUSB0`
  - `python -m serial_cli move --port /dev/ttyUSB0 0 800 --speed 4000 --accel 16000`
  - `python -m serial_cli home --port /dev/ttyUSB0 0 --overshoot 800 --backoff 150`
- CLI module: [tools/serial_cli](./tools/serial_cli/)

Tests

- Native: `pio test -e native`
- On‑Device: `pio test -e esp32dev`

## Wi‑Fi Onboarding

### SoftAP portal

1. Power the device with no credentials saved (hold the BOOT button for 5 seconds if you need to clear existing credentials; see **Long‑press reset** below).
2. Join the broadcast SSID `SOFT_AP_SSID_PREFIX + MAC suffix` using the password from [`include/secrets.h`](./include/secrets.h) (`SOFT_AP_PASS`).
3. Browse to `http://192.168.4.1/` from any phone or laptop connected to the SoftAP.
4. Use the portal to refresh nearby networks, tap one to prefill the form, or manually enter SSID/PSK. Submitting triggers an immediate connect attempt and streams status updates.

The UI is built with Pico.css + Alpine.js and lives under [`data_src/net`](./data_src/net). Assets are gzipped into LittleFS at build time via [`tools/gzip_fs.py`](./tools/gzip_fs.py).

### Serial fallback commands

When a USB cable is convenient, the existing `NET:*` verbs remain available:

- `NET:STATUS` — prints current state, IP, and RSSI (when connected).
- `NET:LIST` — scans nearby SSIDs (top 12) ordered by RSSI; available only while the device SoftAP is active.
- `NET:SET,"ssid","pass"` — quoted CSV payload with validation/error codes identical to the portal.
- `NET:RESET` — clears saved credentials; the device re-enters SoftAP mode automatically.

Use `tools/serial_cli` or the interactive TUI to issue these commands (`python -m serial_cli interactive --port <PORT>`).

### Status indicators

- **LED (GPIO2, active-low):**
  - Fast blink ≈125 ms — SoftAP active and waiting for credentials.
  - Slow blink ≈400 ms — Connecting to the configured network.
  - Solid on — STA connected successfully.
- **Serial events:** each transition emits `CTRL: NET:...` lines with correlation IDs so host tools can pair async updates with user commands.

### Long-press reset

Hold the ESP32 BOOT button (GPIO0) for ≥5 seconds to clear NVS credentials and drop back into SoftAP mode. Short taps are ignored to avoid accidental wipes.

### HTTP API quick reference

| Endpoint | Method | Description |
| --- | --- | --- |
| `/api/status` | GET | Current onboarding state, SSID, IP, RSSI (if connected), and active SoftAP password. |
| `/api/scan` | GET | Top nearby networks ordered by RSSI (`ssid`, `rssi`, `secure`, `channel`). |
| `/api/wifi` | POST | Accepts JSON `{"ssid":"...","pass":"..."}` to persist credentials and trigger a connect attempt. Returns `409` while a connection is already in progress. |

All endpoints respond with compact JSON (`Content-Type: application/json`) and set `Cache-Control: no-store` to avoid stale data inside captive portal flows.

## Setup & Tuning Knobs

 - Build flags and test routing: [platformio.ini](./platformio.ini)
  - `-DUSE_STUB_BACKEND` selects the stub backend (hardware is default)
  - Default test filter limits on‑device tests to driver layer
- Motion/limits constants: [MotorControlConstants.h](./lib/MotorControl/include/MotorControl/MotorControlConstants.h)
  - Defaults: `DEFAULT_SPEED_SPS`, `DEFAULT_ACCEL_SPS2`
  - Limits: `MIN_POS_STEPS`, `MAX_POS_STEPS`
  - Thermal: `MAX_RUNNING_TIME_S`, `MAX_COOL_DOWN_TIME_S`, refill rate (derived)
- FastAccelStepper integration: [FasAdapterEsp32.cpp](./src/drivers/Esp32/FasAdapterEsp32.cpp)
  - Auto‑enable, `setDelayToEnable(2000)` µs, external‑pin callbacks
- Shift‑register I/O and OE gating: [Shift595Vspi.cpp](./src/drivers/Esp32/Shift595Vspi.cpp)
- Board pin map: [Esp32Dev.hpp](./include/boards/Esp32Dev.hpp)
- Filesystem for static assets/presets (optional):
  - [platformio.ini](./platformio.ini) → littlefs, prebuild gzip: [tools/gzip_fs.py](./tools/gzip_fs.py), sources in `data_src/`

Runtime Controls (device)

- `GET THERMAL_LIMITING` → `CTRL:OK THERMAL_LIMITING=ON|OFF max_budget_s=N`
- `GET` or `GET ALL` → `CTRL:OK SPEED=<N> ACCEL=<N> DECEL=<N> THERMAL_LIMITING=ON|OFF max_budget_s=<N>`
- `SET THERMAL_LIMITING=OFF|ON`
- `GET LAST_OP_TIMING[:<id|ALL>]` to validate estimates (`est_ms`) and actual durations

## Protocol Cheatsheet (links)

- Highlights (grammar):
  - `MOVE:<id|ALL>,<abs_steps>[,<speed>][,<accel>]`
  - `HOME:<id|ALL>[,<overshoot>][,<backoff>][,<speed>][,<accel>][,<full_range>]`
  - `STATUS`, `WAKE:<id|ALL>`, `SLEEP:<id|ALL>`
  - `GET` (all settings), `GET ALL`
  - `GET LAST_OP_TIMING[:<id|ALL>]`, `GET THERMAL_LIMITING`, `SET THERMAL_LIMITING=OFF|ON`
  - Responses: `CTRL:OK` (MOVE/HOME include `est_ms`), `CTRL:ERR E..`, and `CTRL:WARN ...` when enforcement is OFF
- Full spec: [Serial command protocol v1 spec](./agent-os/specs/2025-10-15-serial-command-protocol-v1/spec.md)
- HELP source: [MotorCommandProcessor.cpp](./lib/MotorControl/src/MotorCommandProcessor.cpp)

## Repo Map (quick links)

- Firmware console: [SerialConsole.cpp](./src/console/SerialConsole.cpp)
- Command processor: [MotorCommandProcessor.cpp](./lib/MotorControl/src/MotorCommandProcessor.cpp)
- Hardware controller: [HardwareMotorController.cpp](./lib/MotorControl/src/HardwareMotorController.cpp)
- FastAccelStepper adapter: [FasAdapterEsp32.cpp](./src/drivers/Esp32/FasAdapterEsp32.cpp)
- 74HC595 driver: [Shift595Vspi.cpp](./src/drivers/Esp32/Shift595Vspi.cpp)
- Constants: [MotorControlConstants.h](./lib/MotorControl/include/MotorControl/MotorControlConstants.h)
- Tests (C++): [test_MotorControl](./test/test_MotorControl/), [test_Drivers](./test/test_Drivers/)
- Host CLI: [tools/serial_cli](./tools/serial_cli/)
