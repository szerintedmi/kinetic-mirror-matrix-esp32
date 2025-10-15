## PlatformIO project setup

- **One env per hardware target**: Create dedicated `[env:...]` entries so compiler flags, upload speeds, and frameworks stay isolated.
- **Pin frameworks and libraries**: Lock exact versions in `platformio.ini`; reproducible builds are essential when debugging timing issues.
- **Stick to the standard layout**: Keep modules under `src/<module>/`, tasks in `src/tasks/`, drivers in `src/drivers/`, public headers in `include/<module>/`, tests in `test/`, tooling in `tools/`, and static assets split between `data_src/` (plain) and `data/` (gzipped).
- **Document helper scripts**: List pre/post build helpers (e.g., asset compression) directly in `platformio.ini` and keep them in `tools/` so new contributors discover them.
- **Separate prototype configs**: Maintain a prototype-specific `platformio.ini` (or `project_conf = prototype.ini`) that disables release checks but keeps lightweight tests enabled.
- **Note upload routes**: Comment serial vs. OTA vs. JTAG upload instructions inside each environment to save ramp-up time.
