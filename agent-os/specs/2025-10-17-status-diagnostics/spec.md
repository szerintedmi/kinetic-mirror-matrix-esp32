# Specification: Status & Diagnostics

## Goal
Extend STATUS to include homing state and thermal runtime budget metrics per motor. Extend the existing host-side Python CLI (`tools/serial_cli.py`) with an interactive mode that polls STATUS at ~2 Hz and renders a compact table for quick health checks (polling from the host; firmware remains pull-only).

## User Stories
- As an operator, I want to see whether each motor is homed so I can trust absolute moves.
- As an operator, I want a simple runtime budget view so I can decide when to pause for cooldown during demos.
- As a developer, I want per-motor step counts since home so I can later trigger auto‑rehoming when drift risk increases.

## Core Requirements
### Functional Requirements
- Keep a single `STATUS` verb; return all motors, one line each.
- Append new fields (existing keys unchanged):
  - `homed=<0|1>`: 1 after successful HOME; resets to 0 on reboot.
  - `steps_since_home=<steps>`: absolute steps accumulated since last successful HOME; resets on HOME and reboot.
  - Thermal budget metrics (session-only):
    - `budget_s=<sec.dec>`: remaining runtime budget (seconds), can be negative when over budget; displayed with 1 decimal.
    - `ttfc_s=<sec.dec>`: time-to-full-cooldown seconds if turned OFF now.
- Definitions for ON/OFF:
  - ON = motor driver awake/enabled; OFF = driver asleep/disabled. Auto‑WAKE/SLEEP transitions count.
- Host diagnostics CLI (serial_cli.py) provides an interactive mode that continuously refreshes a compact fixed‑width table at ~2 Hz by polling `STATUS`, while also accepting commands (`HELP`, `MOVE`, `HOME`, `WAKE`, `SLEEP`) concurrently. Command responses are shown without disrupting the table (e.g., in a small log/output pane). A non‑interactive one‑shot print remains available via existing `serial_cli.py status`.

### Non-Functional Requirements
- No persistence; all metrics reset on reboot.
- Negligible runtime overhead; updates must not perturb step timing.
- Works with both stub and hardware controllers.

## Visual Design
No mockups provided. CLI table should be readable in a standard terminal (80–120 columns) and avoid excessive wrapping.

## Reusable Components
### Existing Code to Leverage
- Firmware:
  - `lib/MotorControl/include/MotorControl/MotorController.h` (`MotorState`, controller interface)
  - `lib/MotorControl/src/MotorCommandProcessor.cpp` (`handleSTATUS` formatting, command routing)
  - `lib/MotorControl/src/StubMotorController.cpp/.h` and `lib/MotorControl/src/HardwareMotorController.cpp` (per-motor state ownership)
- CLI:
  - `tools/serial_cli.py` (command building and response reading)
  - `tools/e2e_demo.py` (pyserial usage and simple loop structure)
- Tests:
  - `test/test_MotorControl/test_HelpStatus.cpp` (STATUS key presence expectations)

### New Components Required
- Firmware: lightweight per‑motor budget bookkeeping in controller(s) (fields for `homed`, `steps_since_home`, `budget_balance_s`, `last_update_ms`).
- Host tool: extend `tools/serial_cli.py` with an interactive mode (e.g., `serial_cli.py interactive --port <...>`) that connects over serial, polls `STATUS` at ~2 Hz, parses lines, renders a compact table, and accepts commands interactively.

## Technical Approach
- Constants:
  - `MAX_RUNNING_TIME_S = 90`
  - `FULL_COOL_DOWN_TIME_S = 60`
  - `REFILL_RATE = MAX_RUNNING_TIME_S / FULL_COOL_DOWN_TIME_S = 1.5`
- Budget accounting per motor (event‑driven + just‑in‑time):
  - Track a single internal budget in tenths (`budget_tenths`, can be negative) and `last_update_ms`.
  - On MOVE/HOME/WAKE/SLEEP and before responding to STATUS, compute elapsed seconds and update:
    - If ON: decrement by `10 * elapsed_s`.
    - If OFF: increment by `REFILL_TENTHS_PER_SEC * elapsed_s` and cap at `BUDGET_TENTHS_MAX`.
  - Display `budget_s` with one decimal place directly from `budget_tenths / 10` (no clamping at 0; negative values indicate over‑budget).
  - Compute `ttfc_s`:
    - If `budget_tenths >= BUDGET_TENTHS_MAX`: `0` else `ceil((BUDGET_TENTHS_MAX - budget_tenths) / REFILL_TENTHS_PER_SEC)` with a display cap at `MAX_COOL_DOWN_TIME_S`.
- Steps since home:
  - Reset to 0 when HOME completes successfully; increment by absolute step deltas from position updates.
- Homed flag:
  - Set to 1 on successful HOME; cleared on reboot. MOVE commands do not change this flag; subsequent HOME leaves it at 1.
- Formatting:
  - Order keys for readability (e.g., `id pos moving awake homed steps_since_home budget_s ttfc_s`) and keep consistent across lines; reordering is allowed.
  - No high‑precision floats. Steps remain integers; time‑based values (`budget_s`, `ttfc_s`) are formatted with up to 1 decimal place using fixed‑point math (no hardware FP required).
- CLI table (host-side):
  - Parse STATUS lines; render columns: `id pos moving awake homed steps_since_home budget_s ttfc_s`.
  - Default refresh ~2 Hz; one‑shot remains available via `serial_cli.py status`.
  - Interactive mode: use a Textual-based TUI with a status pane (auto‑refresh) and an input line for commands. Ensure serial I/O is synchronized; do not block refresh while waiting for responses.

## Out of Scope
- Enforcing runtime/thermal limits (covered by new roadmap item “Thermal Limits Enforcement”).
- Fault counters and node‑level metrics.
- Persistence or JSON output.
- Degrees/unit conversion.

## Success Criteria
- STATUS includes `homed`, `steps_since_home`, `budget_s`, and `ttfc_s` per motor with correct semantics.
- Minimal CPU and memory overhead; no regression in motion accuracy or timing.
- CLI displays a readable table at ~2 Hz and supports one‑shot output.
- Unit tests updated to validate presence of new keys and basic invariants (e.g., `budget_s` may be negative when over budget; `ttfc_s` >= 0).

## Output Format Update (Proposal)
To simplify host-side parsing while keeping output human-readable, adopt a CSV variant for `STATUS` responses:

- First line is a fixed header: `id,pos,moving,awake,homed,steps_since_home,budget_s,ttfc_s`
- Each subsequent line is a motor row with the same field order.
- Example:
  - `id,pos,moving,awake,homed,steps_since_home,budget_s,ttfc_s`
  - `0,0,0,1,1,0,90.0,0.0`
  - `1,10,1,1,1,10,89.5,0.5`

Notes:
- Human readability is preserved; values remain concise and skimmable.
- No backward compatibility requirement; host CLI supports both the original `key=value` lines and the CSV header+values format.
