# Build Validation

- Build **all targets** after completing a task: `pio run -e esp32DedicatedStep -e esp32SharedStep -e native`
- Build before running tests to catch compile errors early
- Fix build warnings — treat them as potential issues
