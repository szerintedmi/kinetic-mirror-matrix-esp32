# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Build firmware (choose one environment)
pio run -e esp32DedicatedStep      # FastAccelStepper-based, 8 motors, dedicated STEP pins
pio run -e esp32SharedStep         # Shared-step (RMT) variant for 16+ motors
pio run -e native                  # Native build for host-side tests (stub backend)

# Upload to device
pio run -e esp32DedicatedStep -t upload

# Build and upload LittleFS (Wi-Fi portal assets)
pio run -e esp32DedicatedStep -t buildfs
pio run -e esp32DedicatedStep -t uploadfs

# Monitor serial output
pio device monitor -b 115200
```

## Testing

```bash
# Run all native tests (fast, no hardware) - includes Python tests via shim
pio test -e native

# Run a single native test file
pio test -e native -f test_MotorControl/test_CommandPipeline

# Run on-device tests
pio test -e esp32DedicatedStep

# Run Python CLI tests (any of these methods)
poetry run pytest tools/serial_cli/tests/    # Direct pytest
pio run -t test_python                       # Via PlatformIO target
```

## Linting and Formatting

```bash
# C++ lint/check (clang-tidy + cppcheck)
pio check -e esp32DedicatedStep -e native

# C++ format (auto-fix)
CLANG_FORMAT_FIX=1 pio check -e esp32DedicatedStep

# Generate compile_commands.json for clangd
pio run -t compiledb -e esp32DedicatedStep

# Python lint/format
./scripts/python_lint.sh           # Ruff lint
./scripts/python_format.sh         # Ruff format
pio run -t lint_python             # Via PlatformIO
```

## Architecture Overview

### Firmware Layers

```text
SerialConsole (src/console/)
    │
    ▼
MotorCommandProcessor (lib/MotorControl/src/)
    │ - Parses serial protocol v1 (MOVE, HOME, STATUS, etc.)
    │ - Command pipeline: Parser → Router → Handlers → BatchExecutor
    ▼
MotorController interface (HardwareMotorController or StubMotorController)
    │
    ├─► FasAdapterEsp32 (src/drivers/Esp32/) → FastAccelStepper library
    │       Manages STEP pulses via ESP32 MCPWM/PCNT
    │
    └─► Shift595Vspi (src/drivers/Esp32/)
            Controls DIR/SLEEP lines via 2×74HC595 shift registers over VSPI
```

### Key Directories

- `lib/MotorControl/` - Core motor control library (platform-independent logic)
  - `src/command/` - Modular command pipeline (parser, router, handlers)
  - `include/MotorControl/MotorControlConstants.h` - Speed/accel defaults, thermal limits
- `src/drivers/Esp32/` - ESP32-specific hardware drivers
- `lib/net_onboarding/` - Wi-Fi provisioning (SoftAP portal + NVS storage)
- `lib/transport/` - MQTT command/response envelope handling
- `tools/serial_cli/` - Python host CLI with interactive TUI

### Build Configurations

- `-DUSE_STUB_BACKEND` - Uses StubMotorController (no hardware), default for `native` env
- `-DUSE_SHARED_STEP=1` - Enables shared-step RMT pulse generation for 16+ motors

### Serial Protocol v1

Commands: `HELP`, `STATUS`, `MOVE:<id>,<steps>[,speed,accel]`, `HOME:<id>[,overshoot,backoff]`, `WAKE:<id>`, `SLEEP:<id>`

Responses: `CTRL:ACK`, `CTRL:ERR E<code>`, `CTRL:WARN`, `CTRL:READY`

Full spec: `agent-os/specs/2025-10-15-serial-command-protocol-v1/spec.md`

### Python CLI

```bash
# Interactive TUI (serial)
python -m serial_cli interactive --port /dev/ttyUSB0

# Interactive TUI (MQTT)
python -m serial_cli interactive --transport mqtt

# Single commands
python -m serial_cli move --port /dev/ttyUSB0 0 800 --speed 4000
python -m serial_cli home --port /dev/ttyUSB0 0 --overshoot 800
```

## Tech Stack Summary

- **MCU**: ESP32 DevKit, Arduino framework, PlatformIO build
- **Motion**: DRV8825 drivers (full-step), FastAccelStepper library
- **Expansion**: 2×74HC595 shift registers for DIR/SLEEP lines
- **Networking**: AsyncMqttClient for MQTT; Wi-Fi via SoftAP portal + NVS
- **Storage**: LittleFS for presets and gzipped web assets
- **Host tools**: Python 3.13+ CLI (`pyserial`, `paho-mqtt`, `textual`), managed via Poetry

## Hardware Pin Configuration

Pin assignments in `include/boards/Esp32Dev.hpp`:

- STEP pins: GPIO 32, 25, 27, 13, 21, 19, 17, 4 (motors 0-7)
- Shift register: VSPI (SCK=18, MOSI=23), RCLK=5, OE=22
- Status LED: GPIO 2 (active-low)
