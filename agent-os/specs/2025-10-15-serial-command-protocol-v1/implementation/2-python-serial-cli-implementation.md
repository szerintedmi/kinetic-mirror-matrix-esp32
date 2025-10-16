# Task 2: Python Serial CLI

## Overview
Task Reference: Task Group 2 from `agent-os/specs/2025-10-15-serial-command-protocol-v1/tasks.md`

### Task Description
Provide a cross-platform Python CLI for issuing protocol commands over serial, plus focused tests for argument→command mapping and an example E2E script.

## Implementation Summary
- Added `tools/serial_cli.py` implementing subcommands: `help`, `status`, `wake`, `sleep`, `move`, `home`.
- Added mapping tests `tools/tests/test_cli_mapping.py` (no hardware needed) which validate that CLI args map to the correct protocol lines.
- Added `tools/e2e_demo.py` to exercise typical flows against a device or print commands in dry-run mode.

## Files Changed/Created
### New Files
- `tools/serial_cli.py`
- `tools/tests/test_cli_mapping.py`
- `tools/e2e_demo.py`

### Modified Files
- None

## Key Implementation Details
### CLI Grammar Mapping
- HELP → `HELP`\n
- STATUS → `STATUS`\n
- WAKE → `WAKE:<id>`\n
- SLEEP → `SLEEP:<id>`\n
- MOVE → `MOVE:<id|ALL>,<abs_steps>[,<speed>][,<accel>]`\n
- HOME → `HOME:<id|ALL>[,<overshoot>][,<backoff>][,<speed>][,<accel>][,<full_range>]`\n
### Options
- Common: `--port`, `--baud` (default 115200), `--timeout`, `--dry-run`
- HOME optional flags: `--overshoot`, `--backoff`, `--speed`, `--accel`, `--full-range`

### Testing
- Mapping tests: run `python3 tools/tests/test_cli_mapping.py` — validates 9 cases, prints pass/fail.
- E2E demo: `python3 tools/e2e_demo.py --port /dev/ttyUSB0` or without `--port` for dry-run.

## User Standards & Preferences Compliance

### Serial Interface
File Reference: `agent-os/standards/frontend/serial-interface.md`
- Maps verbs 1:1 to the protocol, avoids payloads beyond grammar, and treats multi-line responses as pass-through.

### Build Validation
File Reference: `agent-os/standards/testing/build-validation.md`
- Provides a minimal E2E script for manual validation on hardware; automated tests are focused and do not require device access.

## Known Issues & Limitations
- The CLI does not parse STATUS output; it prints through as-is (sufficient for v1).
- Requires `pyserial` for device usage; not required for `--dry-run` or mapping tests.

## Notes
- The CLI shares the grammar builder used by the demo, ensuring consistency between examples and real usage.

