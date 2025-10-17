## Summary
- Added `interactive` subcommand with a curses-based TUI that polls STATUS at a configurable rate (default 2 Hz), renders a compact fixed-width table, and accepts commands concurrently. Implemented pure helpers for parsing STATUS output and rendering the table, plus a serial worker thread to synchronize I/O without blocking UI refresh.
- Refactored the CLI into a proper Python package under `tools/serial_cli/` with a top-level shim (`serial_cli/`) so the tool runs as `python3 -m serial_cli`. Moved tests to `tools/tests/python/` and updated the PlatformIO test wrapper to execute the Python test runner.

## Changes
- Argument parsing additions (package: `tools/serial_cli/`)
  - New subcommand: `interactive`
  - Options: `--port`, `--baud`, `--timeout`, `--rate` (Hz; default 2.0)

- Rendering helpers
  - `parse_status_lines(text) -> List[Dict[str,str]]`: supports current key=value token lines and optional CSV header+rows
  - `render_table(rows) -> str`: renders columns `id pos moving awake homed steps_since_home budget_s ttfc_s`

- TUI loop structure and non-blocking input
  - `_SerialWorker` thread handles all serial I/O, interleaving command processing with STATUS polling to avoid stalling refresh
  - Curses UI draws: table (top), log pad (middle, scrollable), and command prompt (bottom)
  - Help overlay (`/h`) as a full-screen, two-column, wrapped view (left: keys + status columns; right: device HELP output)
  - Pads reserve one column for an ASCII scrollbar; scroll with PgUp/PgDn; Up/Down navigates command history
  - Host meta-commands prefixed with `/` (`/q`, `/h`) to avoid collisions with device commands

- Robust serial I/O
  - Asynchronous response capture; only first response line is prefixed with `> ` in the log
  - Reconnect logging: first error retained; subsequent retries update a single `Reconnecting to <port> ...` line with growing dots; `[reconnected]` noted on success

- Packaging and test harness alignment
  - CLI packaged under `tools/serial_cli/`; top-level shim `serial_cli/` enables `python -m serial_cli`
  - Tests moved to `tools/tests/python/`; PlatformIO Unity wrapper updated to call `tools/tests/python/run_cli_tests.py`

## Testing
- Unit tests for parsing and table formatting
  - `tools/tests/python/test_cli_interactive.py`
    - Verifies `interactive` arg parsing defaults and overrides
    - Validates parsing of key=value and CSV STATUS formats and table rendering

- PlatformIO native wrapper
  - `pio test -e native -f test_CLI_Host` → runs `tools/tests/python/run_cli_tests.py`

- Manual test steps and expected behavior
  - Run: `python3 -m serial_cli interactive --port <PORT>`
    - Expected: table refreshes ~2 Hz; log pad scrolls with PgUp/PgDn; help overlay via `/h`; `/q` to quit
    - Disconnects: first error retained; subsequent reconnect progress shown on one line with growing dots
