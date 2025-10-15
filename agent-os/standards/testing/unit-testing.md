## Unit testing

- **Lean frameworks**: Start with PlatformIO `unity` or `CppUTest`; they run on host and device, keeping coverage realistic without heavy setup.
- **Test on host first**: Add a `[env:native]` entry using the same code paths so logic bugs surface before flashing hardware.
- **Isolate hardware**: Stub HAL interfaces when testing logic; keep actual peripheral tests under `test_integration/` so CI can skip them.
- **Keep suites fast**: Target sub-2s runtimes
