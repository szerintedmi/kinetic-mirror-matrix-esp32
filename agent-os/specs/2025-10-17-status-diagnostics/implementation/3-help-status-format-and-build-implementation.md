# 3. HELP/STATUS format and build — Implementation

Owner: api-engineer

## Summary
- Extended existing formatting test to assert new STATUS keys appear on the first line: `homed`, `steps_since_home`, `budget_s`, `ttfc_s`.
- Validated HELP remains unchanged and readable (existing assertions still pass).
- Performed build validation for `esp32dev` via PlatformIO.
- Reviewed STATUS line length to ensure it remains skimmable (< 120–140 chars per line).

## Changes
- test/test_MotorControl/test_HelpStatus.cpp
  - Added 4 assertions in `test_status_format_lines()` to check for the new keys.

## Rationale
The spec requires STATUS to include homing and thermal budget diagnostics without breaking readability. Tests now cover the presence of these fields in the first line, complementing behavioral diagnostics tests added in TG1.

## Build Validation
- Platform: `esp32dev` (Arduino/ESP32)
- Command: `pio run -e esp32dev`
- Result: Build succeeds locally (no changes to PlatformIO config were required).

## Output Length Check
- Typical first STATUS line example (stub backend):
  `id=0 pos=0 moving=0 awake=0 homed=0 steps_since_home=0 budget_s=90.0 ttfc_s=0.0 speed=500 accel=800`
- Approximate length: ~105 characters. With 8 lines, total payload fits comfortably within standard terminal widths and does not flood the monitor.

## Notes
- No HELP text changes needed for this task group; existing HELP remains valid and concise.
- Further diagnostics formatting changes should remain additive and keep one-line-per-motor output.

## CLI Validation (serial_cli)
- Verified CLI mapping and interactive parsing/render tests pass via PlatformIO wrapper:
  - Command: `pio test -e native -f test_CLI_Host --without-uploading`
  - Result: Passed (mapping and render table helpers aligned with STATUS keys)

## Final Verification
- Firmware unit tests (MotorControl) also green alongside formatting updates:
  - Command: `pio test -e native -f test_MotorControl --without-uploading`
  - Result: Passed (including updated HELP/STATUS checks)
