## Hardware validation

- **Define smoke scripts**: Keep a `scripts/smoke_test.py` (or similar) that sends serial commands and checks key responses; ensures lab operators repeat the same steps.
- **Exercise peripherals in cycles**: For each board, toggle actuators, sample sensors, and log errors within a single scripted run; catches wiring faults fast.
- **Record build metadata**: Print firmware git hash and config version at test start; makes lab issues traceable without hunting logs.
- **Document manual checks**: Maintain a short checklist (`docs/hw_validation.md`) covering power-up, connectivity, and watchdog behavior for techs without IDE access.
- **Automate reboot margin**: After flashing, reset the board twice and confirm the system returns to READY state—verifies persistence logic survived.
