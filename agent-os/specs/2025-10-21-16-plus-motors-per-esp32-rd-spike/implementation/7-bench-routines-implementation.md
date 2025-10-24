# Task Group 7 – Manual Bench Routines (No Analyzer)

## Summary
- Added `tools/bench/manual_sequence_runner.py`, a deterministic bench helper that replays a fixed series of MOVE/HOME commands with optional repetitions while echoing every serial response.
- The script hard-codes setup commands (speed/accel/WAKE/HOME) separately from the repeated pattern so operators can run multi-cycle verification without editing files.
- Instead of adding new firmware verbs, the workflow now captures MCU-tracked positions through the existing `STATUS` output after each command and at the end of the run, aligning with the updated scope.
- Added a teardown sequence that parks all motors at `pos=0` and issues `SLEEP:ALL` after the repetitions finish, so the array always lands in a known idle state.
- Documented a lab checklist that ties the script steps to required bench observations (shared-step smoke, physical markers vs. MCU position, and auto-sleep confirmation).

## Implementation Details

### Deterministic bench runner
**Location:** `tools/bench/manual_sequence_runner.py`

- Wraps pyserial to stream the scripted sequence; imports `read_response`, `parse_status_lines`, and `extract_est_ms_from_ctrl_ok` from `tools.serial_cli` to stay consistent with existing parsing logic.
- `SETUP_COMMANDS` executes once (speed/accel/decel, `WAKE:0`, `HOME:0`). `PATTERN_COMMANDS` captures the bench rhythm (short/medium/long moves, HOME, and one multi-command batch) and runs `1 + repetitions` passes.
- `FINALIZE_COMMANDS` runs after all repetitions to return the array to zero (`MOVE:ALL,0`) and explicitly `SLEEP:ALL` before the closing STATUS snapshot.
- After each command the script parses `est_ms` (when available), sleeps for the estimate + grace, then issues a `STATUS` query to verify all motors report `moving=0`. Status checks throttle to a low duty cycle but continue until idle, so multi-command batches are respected.
- Every serial exchange is logged to stdout so operators have a chronological trace. A final `STATUS` snapshot is printed automatically for the logbook.
- Packaged under `tools.bench` (added `__init__.py` scaffolding) so operators can simply run `python -m tools.bench.manual_sequence_runner --port ...`.

**Rationale:** Mirrors the requested “simple sequence executor” while keeping timing logic deterministic (sleep based on device-provided estimate) and avoiding rapid polling that could interfere with shared-step ISR scheduling.

### Tasks list update
**Location:** `agent-os/specs/2025-10-21-16-plus-motors-per-esp32-rd-spike/tasks.md`

- Marked 7.0–7.3 as completed so downstream verification knows the bench assets exist and align with the revised scope (leveraging existing `STATUS` instead of a new GET verb as agreed).

## Lab Checklist (Operator-Facing)

1. **Prep**
   - Shared-step PlatformIO env flashed; connect USB serial and note port.
   - Confirm mirrors are homed and physical zero marks are readable.
2. **Run script**
   - `python tools/bench/manual_sequence_runner.py --port <PORT> --repetitions 9`
   - Observe setup section: verify `SET SPEED=800` applied and initial `HOME:0` completes without warnings.
3. **Pattern validation**
   - Watch each pass header; confirm motors follow the long-short rhythm and that `HOME:0` inside the pattern fully retracts before the next move starts.
   - After each printed `STATUS`, confirm `moving=0` before the script advances (script enforces this, operator visually verifies physical stop).
4. **Physical vs MCU comparison**
   - After every second pass, pause motion (Ctrl+C if needed) and compare physical tape marks to `pos=` readings from the latest `STATUS`. Resume by re-running script if interrupted mid-pattern.
5. **Bench smoke (7.4)**
   - While the script runs, confirm the repeated MOVE/HOME cycles do not freeze and that motors auto-sleep (STATUS → `awake=0`) once the final STATUS is printed.
6. **Logging**
   - Save stdout log plus the final STATUS block. Note any drift or WARN/ERR lines for follow-up.

## Dependencies

### New Dependencies Added
- None (script reuses the existing `pyserial` dependency already required by `serial_cli`).

### Configuration Changes
- None.

## Testing

### Test Files Created/Updated
- `tools/bench/manual_sequence_runner.py` – `python -m compileall tools/bench/manual_sequence_runner.py` (syntax validation).

### Test Coverage
- Unit tests: ❌ None (script relies on serial hardware interaction).
- Integration tests: ❌ None (manual bench only).
- Edge cases covered: parsing absence of `est_ms`, multi-command batches, timeout when STATUS keeps reporting `moving=1`.

### Manual Testing Performed
- Dry-run via `python -m compileall` to ensure syntax correctness.
- Not run against hardware in this environment; operators must execute the lab checklist on bench hardware.

## User Standards & Preferences Compliance

### global/coding-style.md
**File Reference:** `agent-os/standards/global/coding-style.md`

**How Your Implementation Complies:** Script follows the existing CLI patterns (small functions, clear constant names) and avoids unnecessary abstractions.

### global/conventions.md
**File Reference:** `agent-os/standards/global/conventions.md`

**How Your Implementation Complies:** Kept new tooling under `tools/bench`, surfaced a CLI with the expected `--port/--baud` flags, and documented workflow via this implementation note.

### global/platformio-project-setup.md
**File Reference:** `agent-os/standards/global/platformio-project-setup.md`

**How Your Implementation Complies:** Bench helper references the shared-step PlatformIO env implicitly and requires no changes to the build system.

### global/resource-management.md
**File Reference:** `agent-os/standards/global/resource-management.md`

**How Your Implementation Complies:** Serial polling is throttled (estimate+grace, STATUS checks only on demand) to avoid starving the ESP32 or wasting bench time.

### testing/unit-testing.md
**File Reference:** `agent-os/standards/testing/unit-testing.md`

**How Your Implementation Complies:** Captured available automated validation (compileall) and documented the lack of deeper tests because the script interacts with real hardware.

### testing/hardware-validation.md
**File Reference:** `agent-os/standards/testing/hardware-validation.md`

**How Your Implementation Complies:** Checklist maps directly to hardware validation expectations—explicit bench smoke, position comparison, and logging of observed data.

### testing/build-validation.md
**File Reference:** `agent-os/standards/testing/build-validation.md`

**How Your Implementation Complies:** Script requires no firmware rebuild, but the checklist calls out ensuring the shared-step env is flashed before running, staying aligned with the build validation flow.

## Known Issues & Limitations

1. **No dedicated device-side BENCH report**
   - Description: As requested, we rely on STATUS snapshots instead of adding a new GET verb.
   - Impact: Operators must read positions from STATUS lines.
   - Future Consideration: If we later need multi-device aggregation, revisit a firmware-backed bench summary command.

## Performance Considerations
- STATUS polling is limited to one request per command completion (plus retries while `moving=1`), preventing excessive serial chatter even during long repetitions.

## Security Considerations
- Script only talks to a local serial port and introduces no new network surfaces.

## Dependencies for Other Tasks
- Provides the repeatable bench harness required before scaling to Wi-Fi stress testing or analyzer-based validation.

## Notes
- Interrupting the script (Ctrl+C) leaves the MCU running; issue `STATUS` manually afterward if needed to confirm idle before restarting a pass.
