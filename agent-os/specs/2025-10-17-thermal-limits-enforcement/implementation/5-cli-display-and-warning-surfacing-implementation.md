Implementation Notes: Task Group 5 — CLI display and warning surfacing

Summary
- The serial CLI/TUI now surfaces the global thermal flag and any WARN lines alongside CTRL:OK, without changing STATUS parsing.
- The Python CLI test runner for the suite auto-discovers and runs all test_*.py files in its folder.

Changes
- tools/serial_cli/__init__.py
  - Added helpers: parse_thermal_get_response, extract_est_ms_from_ctrl_ok.
  - Worker fetches GET THERMAL_LIMITING on startup and after SET, stores state, exposes text for UI.
  - Tolerates WARN+OK responses when extracting est_ms.
- tools/serial_cli/tui/curses_ui.py
  - Footer shows “thermal limiting=ON|OFF (max=Ns)”.
  - Colored background: green for ON, red for OFF (fallback-safe).
- tools/tests/python/test_cli_thermal_flag.py
  - Tests for GET parser and WARN+OK extraction.
- test/test_serial_cli/test_CLI_Python.cpp
  - Runner now discovers all test_*.py in its own folder and runs them as individual Unity tests with script filenames as names.

Acceptance
- GET THERMAL_LIMITING fetch and display verified.
- WARN+OK responses are preserved and displayed; commands remain successful.

