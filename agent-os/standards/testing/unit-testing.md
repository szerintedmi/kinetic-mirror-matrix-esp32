## Unit testing

- **Framework**: Unity (`<unity.h>`). Used for both host and device tests. No CppUTest.
- **Test on host first**: The `[env:native]` build uses `-DUSE_STUB_BACKEND` and `-fsanitize=address` to surface logic and memory bugs before flashing hardware.
- **Isolate hardware**: Stub HAL interfaces for host tests. Device-only tests live under `test/test_OnDevice/`; ESP32 envs filter to these with `test_filter = test_OnDevice`, native skips them with `test_ignore = test_OnDevice`.
- **Keep suites fast**: Target sub-2s native runtimes. Use `TEST_TIMEOUT_GUARD(ms)` from `test_common/TestTimeout.h` to abort hung tests (no-op on Arduino).
- **Assertion messages**: Use `_MESSAGE` variants (e.g., `TEST_ASSERT_TRUE_MESSAGE`) when the macro doesn't already print expected/actual values. Typed assertions like `TEST_ASSERT_EQUAL_STRING` self-report and don't need extra messages.
- **Shared helpers**: Common utilities in `test_common/TestHelpers.h` (`SplitLines`, `FindStatusLineForId`, `ParseEstMs`, `ParseMsgId`). Add here, don't duplicate across test files.
- **Python tests via shim**: `test_CLI/test_CLI_Python.cpp` runs `poetry run pytest tools/mirror_cli/tests/` through `std::system()`, so `pio test -e native` covers both C++ and Python.
- **File organization**: Small test suites use a single self-contained `.cpp` with its own `main()`. Large suites (e.g., `test_MotorControl/`) split tests across multiple `.cpp` files grouped by feature, with a central `test_main.cpp` that declares and runs all tests.
