# 4. Test Review & Gap Analysis — Implementation

Owner: testing-engineer

## Scope
Reviewed and augmented tests for Status & Diagnostics across firmware and CLI to cover critical boundary cases and mapping.

## Changes
- Firmware (Unity/C++): test/test_MotorControl/
  - test_StatusBudget.cpp
    - Added `test_homed_resets_on_reboot` to verify homed flag clears on reboot.
    - Added `test_steps_since_home_resets_after_second_home` to verify steps_since_home returns to 0 when a HOME completes after prior motion.
  - test_Core.cpp
    - Wired new test into the runner for both native and on-device builds (on-device runs only lightweight tests).
- CLI (Python): tools/tests/python/
  - test_cli_mapping.py
    - Added alias coverage: `st` → `STATUS`, `m` for MOVE, `h` for HOME.

## Rationale
- Boundary focus per spec: negative `budget_s` visibility (no clamp at 0), `ttfc_s` calculation, homed/steps_since_home behavior.
- Ensure CLI mapping and render helpers align with new STATUS columns and common aliases.

## Results
- Native firmware tests (MotorControl): PASSED
  - Command: `pio test -e native -f test_MotorControl --without-uploading`
  - 30/30 passed.
- Python CLI wrapper tests: PASSED
  - Command: `pio test -e native -f test_CLI_Host --without-uploading`
  - Aliases and render/parse helpers green.

## Notes
- Steps since last home resets on HOME completion (now covered by two tests: initial HOME and second HOME after motion). Reboot reset of `steps_since_home` is not required by acceptance; we retained a light homed‑flag reboot test only.
