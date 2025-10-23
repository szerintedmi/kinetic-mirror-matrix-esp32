# Task 5: Host CLI/TUI Adjustments

## Overview
**Task Reference:** Task #5 from `agent-os/specs/2025-10-21-16-plus-motors-per-esp32-rd-spike/tasks.md`
**Implemented By:** ui-designer
**Date:** 2025-10-23
**Status:** ✅ Complete

### Task Description
Update the Python host CLI and TUI to match the simplified protocol that removes per-MOVE and per-HOME speed/accel parameters, keep STATUS polling behavior, and reflect the new grammar in help and hints. Add a few focused tests for the CLI command builders/parsing.

## Implementation Summary
The CLI parser and command builder were updated to remove `--speed`/`--accel` from the `move`/`home` paths and to emit commands that match firmware HELP: `MOVE:<id|ALL>,<abs_steps>` and `HOME:<id|ALL>[,<overshoot>][,<backoff>][,<full_range>]`. The interactive TUI Help overlay now includes a concise “Common commands” section mirroring the simplified grammar and global settings.

Existing Python tests that validate CLI mapping were updated accordingly to assert the simplified command forms and placeholders for optional HOME fields. STATUS polling and overall interactive behavior remain unchanged.

## Files Changed/Created

### New Files
- `agent-os/specs/2025-10-21-16-plus-motors-per-esp32-rd-spike/implementation/5-host-cli-tui-adjustments-implementation.md` - Implementation report for Task Group 5.

### Modified Files
- `tools/serial_cli/__init__.py` - Removed per-MOVE/HOME speed/accel args; updated builders to simplified grammar; adjusted check-* writers.
- `tools/serial_cli/tui/textual_ui.py` - Added grammar-focused “Common commands” to Help overlay left panel.
- `test/test_Serial_CLI/test_cli_mapping.py` - Updated expected command mappings to reflect the simplified grammar.
- `agent-os/specs/2025-10-21-16-plus-motors-per-esp32-rd-spike/tasks.md` - Marked Task Group 5 and sub-tasks complete.

## Key Implementation Details

### CLI grammar updates
**Location:** `tools/serial_cli/__init__.py`

- Removed `--speed` and `--accel` options from `move`/`m` and `home`/`h` subparsers.
- `build_command()` now returns `MOVE:<id>,<abs_steps>\n` and builds `HOME` via `_join_home_with_placeholders(id, overshoot, backoff, full_range)` which supports optional fields and preserves placeholder positions when skipping `backoff`.
- `check-move`/`check-home` continue to accept speed/accel for local estimation parameters, but device commands are sent without per-command speed/accel to align with firmware.

**Rationale:** Matches firmware parser and HELP output introduced by protocol simplification (global SPEED/ACCEL/DECEL).

### TUI help improvements
**Location:** `tools/serial_cli/tui/textual_ui.py`

- Help overlay left panel now shows a compact “Common commands” section:
  - `MOVE:<id|ALL>,<abs_steps>`
  - `HOME:<id|ALL>[,<overshoot>][,<backoff>][,<full_range>]`
  - `GET/SET SPEED, ACCEL, DECEL`
  - `GET LAST_OP_TIMING[:<id|ALL>]`

**Rationale:** Makes the simplified grammar and global controls readily discoverable during interactive sessions.

## Dependencies (if applicable)
None added.

## Testing

### Test Files Created/Updated
- `test/test_Serial_CLI/test_cli_mapping.py` - Updated to assert simplified `MOVE`/`HOME` forms and alias behavior; placeholder behavior for `HOME` with `--full-range` but no `--backoff`.

### Test Coverage
- Unit tests: ✅ Complete (focused CLI mapping cases)
- Integration tests: ❌ None
- Edge cases covered: optional HOME placeholders; alias `m`/`h`; GET LAST_OP_TIMING with/without id.

### Manual Testing Performed
- Verified `python -m serial_cli --dry-run move 0 200` prints `MOVE:0,200`.
- Verified interactive TUI opens and shows updated Help overlay; STATUS table refresh remains unchanged.

## User Standards & Preferences Compliance

### Coding Style
**File Reference:** `agent-os/standards/global/coding-style.md`

**How Your Implementation Complies:**
Followed clear function naming, short helpers for command joins, and minimal diffs. Avoided inline comments and preserved existing code style.

### Conventions
**File Reference:** `agent-os/standards/global/conventions.md`

**How Your Implementation Complies:**
Aligned host CLI grammar exactly with firmware HELP and spec. Kept aliases and dry-run behavior consistent. Did not introduce new commands beyond scope.

### Serial Interface
**File Reference:** `agent-os/standards/frontend/serial-interface.md`

**How Your Implementation Complies:**
Respected device protocol source of truth, surfaced HELP verbatim, and avoided legacy parameters in host-generated commands.

### Build & Testing
**File Reference:** `agent-os/standards/testing/build-validation.md`

**How Your Implementation Complies:**
Kept tests focused and fast; validated command builders only and avoided running unrelated suites.

## Integration Points (if applicable)
- Device protocol: `MOVE`, `HOME`, `GET/SET SPEED|ACCEL|DECEL`, `GET LAST_OP_TIMING` as documented by firmware HELP.

