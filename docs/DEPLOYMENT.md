# Build & Deployment Guide

This guide covers building firmware, uploading to devices, and managing deployments.

## Quick Reference

```bash
# Build
pio run -e esp32DedicatedStep              # Build firmware
pio run -e esp32DedicatedStep -t buildfs   # Build filesystem

# Upload via USB
pio run -e esp32DedicatedStep -t upload    # Upload firmware
pio run -e esp32DedicatedStep -t uploadfs  # Upload filesystem

# Upload via OTA (single device)
poetry run python -m tools.deploy.ota_deploy --device <IP>

# Upload via OTA (all devices in config)
poetry run python -m tools.deploy.ota_deploy

# Upload filesystem via OTA
poetry run python -m tools.deploy.ota_deploy --filesystem --device <IP>

# Upload firmware + filesystem via OTA
poetry run python -m tools.deploy.ota_deploy --with-filesystem --device <IP>
```

## Build Environments

| Environment | Description | Use Case |
|-------------|-------------|----------|
| `esp32DedicatedStep` | FastAccelStepper, 8 motors | Production hardware |
| `esp32SharedStep` | Shared-step RMT, 16+ motors | High motor count |
| `native` | StubMotorController, no hardware | Host-side testing |

### Build Commands

```bash
# Full build
pio run -e esp32DedicatedStep

# Build filesystem (Wi-Fi portal assets)
pio run -e esp32DedicatedStep -t buildfs

# Clean and rebuild
pio run -e esp32DedicatedStep -t clean && pio run -e esp32DedicatedStep

# Generate compile_commands.json for IDE
pio run -t compiledb -e esp32DedicatedStep
```

### Firmware Versioning

Version is automatically injected from git at build time:

- `GIT_COMMIT_HASH`: Short commit hash (e.g., `3837274` or `3837274-dirty`)
- `GIT_COMMIT_DATE`: ISO 8601 timestamp

Dirty builds (uncommitted changes) use the build timestamp instead of commit timestamp.

Check version:
- Serial at boot: `Firmware: 3837274`
- Command: `GET ALL` returns `firmware_version` and `firmware_date`
- API: `GET /api/status` returns `firmwareVersion` and `firmwareDate`
- Web portal: Shows version in status card

## USB Upload

Connect via USB and upload:

```bash
pio run -e esp32DedicatedStep -t upload
pio device monitor -b 115200  # Verify: CTRL:READY Serial v1
```

## OTA Updates

Update firmware over the network using ArduinoOTA. Device must be on WiFi.

### OTA Password

Configure in `include/secrets.h` (firmware side):

```cpp
#define OTA_PASSWORD "your-secure-password"
```

Configure in `tools/deploy/ota_devices.toml` (deploy script side):

```toml
[ota]
password = "your-secure-password"
```

Both must match for OTA uploads to succeed.

### Single Device OTA

Get device IP from serial monitor, MQTT, or web portal, then:

```bash
# Build and deploy firmware to single device
poetry run python -m tools.deploy.ota_deploy --device <IP>

# Deploy existing build (skip rebuild)
poetry run python -m tools.deploy.ota_deploy --device <IP> --skip-build

# Skip POST-deploy verification
poetry run python -m tools.deploy.ota_deploy --device <IP> --no-verify

# Deploy filesystem only
poetry run python -m tools.deploy.ota_deploy --filesystem --device <IP>

# Deploy both firmware and filesystem
poetry run python -m tools.deploy.ota_deploy --with-filesystem --device <IP>
```

### OTA Deploy Script

Deploy to one or more devices with progress tracking and verification.

#### Setup

1. Copy the example config:
   ```bash
   cp tools/deploy/ota_devices.toml.example tools/deploy/ota_devices.toml
   ```

2. Edit `tools/deploy/ota_devices.toml`:
   ```toml
   [devices]
   ips = [
       "192.168.1.100",
       "192.168.1.101",
   ]

   [ota]
   # Must match OTA_PASSWORD in secrets.h
   password = "kinetic-mirror-ota"
   ```

#### Usage

```bash
# Deploy firmware to single device
poetry run python -m tools.deploy.ota_deploy --device <IP>

# Build and deploy firmware to all devices in config
poetry run python -m tools.deploy.ota_deploy

# Build only, no deploy
poetry run python -m tools.deploy.ota_deploy --build-only

# Deploy existing build (skip build step)
poetry run python -m tools.deploy.ota_deploy --skip-build

# Skip POST-deploy verification
poetry run python -m tools.deploy.ota_deploy --no-verify

# Retry failed devices from previous run
poetry run python -m tools.deploy.ota_deploy --retry tools/deploy/logs/<timestamp>_summary.json

# Deploy filesystem only (littlefs.bin with web assets)
poetry run python -m tools.deploy.ota_deploy --filesystem --device <IP>

# Deploy both firmware and filesystem
poetry run python -m tools.deploy.ota_deploy --with-filesystem --device <IP>
```

#### What It Does

**Firmware deploy (default):**
1. **Build**: Runs `pio run -e esp32DedicatedStep` (unless `--skip-build`)
2. **Upload**: Uses `espota.py` directly to upload to all devices in parallel (no rebuild per device)
3. **Verify**: Checks `/api/status` on each device to confirm firmware version
4. **Report**: Shows summary table with success/failure status

**Filesystem deploy (`--filesystem`):**
1. **Build**: Runs `pio run -e esp32DedicatedStep -t buildfs`
2. **Upload**: Uses `espota.py -s` to upload littlefs.bin to all devices in parallel
3. **Report**: Shows summary table (no version verification for filesystem-only)

**Combined deploy (`--with-filesystem`):**
Performs firmware deploy first, then filesystem deploy if all firmware uploads succeed.

#### Output Example

```
Building firmware...
Build complete: 3837274 (2025-11-30T14:23:45+0000)
Log: tools/deploy/logs/2025-11-30_14-23-45_build.log

Deploying to 2 device(s)...

⠋ 192.168.1.100     ████████████████████████████████ Uploading 100%
⠋ 192.168.1.101     ████████████████████████████████ Verifying...

┏━━━━━━━━━━━━━━━━┳━━━━━━━━┳━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━┳━━━━━━━┓
┃ IP             ┃ Status ┃ Version  ┃ Date                  ┃ Error ┃
┡━━━━━━━━━━━━━━━━╇━━━━━━━━╇━━━━━━━━━━╇━━━━━━━━━━━━━━━━━━━━━━━╇━━━━━━━┩
│ 192.168.1.100  │   ✔    │ 3837274  │ 2025-11-30T14:23:45+0 │       │
│ 192.168.1.101  │   ✔    │ 3837274  │ 2025-11-30T14:23:45+0 │       │
└────────────────┴────────┴──────────┴───────────────────────┴───────┘

All 2 device(s) deployed successfully!

Logs saved to: tools/deploy/logs
```

#### Logs

All output is logged to `tools/deploy/logs/`:
- `<timestamp>_build.log` - Firmware build output
- `<timestamp>_buildfs.log` - Filesystem build output (when using `--filesystem` or `--with-filesystem`)
- `<timestamp>_<ip>.log` - Per-device firmware upload output
- `<timestamp>_<ip>_fs.log` - Per-device filesystem upload output
- `<timestamp>_summary.json` - Deployment results (for retry)

### Auto-Rollback

ESP32 bootloader automatically reverts to previous firmware if new firmware fails to boot after a few attempts.

## Wi-Fi Onboarding

### Dual-Network Support

Each device stores **primary** and **secondary** Wi-Fi credentials with automatic failover:

- **Primary**: Home/studio network for normal operation
- **Secondary**: Mobile hotspot for field deployments or fallback

On boot or disconnect, the device tries the last successful network first, then falls back to the other. This enables safe credential updates across multiple controllers via MQTT (`NET:SET_PRIMARY` or `NET:SET_SECONDARY`) without risking loss of connectivity from misconfiguration.

### SoftAP Portal

1. Power device with no credentials (hold BOOT 5s to clear existing)
2. Join `SOFT_AP_SSID_PREFIX + MAC` using password from `include/secrets.h`
3. Browse to `http://192.168.4.1/`
4. Select network, choose slot (primary/secondary), enter credentials

### Serial/MQTT Commands

```
NET:STATUS                                 # Current state, IP, RSSI, connected slot
NET:GET_CONFIG                             # Show configured primary/secondary SSIDs
NET:LIST                                   # Scan nearby SSIDs (SoftAP only)
NET:SET,"ssid","pass"                      # Set primary and connect (legacy)
NET:SET_PRIMARY,"ssid","pass"              # Set primary credentials
NET:SET_SECONDARY,"ssid","pass"            # Set secondary credentials
NET:CLEAR_SECONDARY                        # Remove secondary credentials
NET:RESET                                  # Clear all credentials, enter SoftAP
```

### Status LED (GPIO2, active-low)

- Fast blink ~125ms: SoftAP active, waiting for credentials
- Slow blink ~400ms: Connecting to configured network
- Solid on: STA connected

### HTTP API

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/status` | GET | Onboarding state, SSID, IP, RSSI, firmware version |
| `/api/config` | GET | Configured networks (`primary`, `secondary`, `connected_to`) |
| `/api/scan` | GET | Nearby networks (ssid, rssi, secure, channel) |
| `/api/wifi` | POST | `{"ssid":"...","pass":"..."}` set primary and connect |
| `/api/wifi/secondary` | POST | `{"ssid":"...","pass":"..."}` set secondary |
| `/api/wifi/secondary/clear` | POST | Clear secondary credentials |
| `/api/reset` | POST | Clear all credentials, enter SoftAP |

## Testing

### C++ Tests (PlatformIO)

```bash
pio test -e native                         # All native tests (fast, no hardware)
pio test -e native -f test_MotorControl/test_CommandPipeline  # Single test
pio test -e esp32DedicatedStep             # On-device tests
```

### Python Tests

```bash
poetry run pytest tools/mirror_cli/tests/  # Direct pytest
pio run -t test_python                     # Via PlatformIO target
pio test -e native                         # Also runs Python tests via shim
```

## Linting & Formatting

### C++ (clang-tidy + cppcheck)

```bash
pio check -e esp32DedicatedStep -e native  # Lint all environments
CLANG_FORMAT_FIX=1 pio check ...           # Auto-fix formatting
```

### Python (Ruff)

```bash
pio run -t lint_python                     # Lint
pio run -t format_python                   # Format
./scripts/python_lint.sh                   # Direct Ruff lint
./scripts/python_format.sh                 # Direct Ruff format
```

## Configuration

### Build Flags

- `-DUSE_STUB_BACKEND`: Use StubMotorController (no hardware), default for `native`
- `-DUSE_SHARED_STEP=1`: Shared-step RMT pulse generation for 16+ motors

### Motion Constants

See [MotorControlConstants.h](../lib/MotorControl/include/MotorControl/MotorControlConstants.h):

- `DEFAULT_SPEED_SPS`, `DEFAULT_ACCEL_SPS2`: Motion defaults
- `MIN_POS_STEPS`, `MAX_POS_STEPS`: Position limits
- `MAX_RUNNING_TIME_S`, `MAX_COOL_DOWN_TIME_S`: Thermal budget
