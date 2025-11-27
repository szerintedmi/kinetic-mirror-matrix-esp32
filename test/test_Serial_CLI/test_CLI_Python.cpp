// Unity shim that runs consolidated Python tests via pytest.
// This allows `pio test -e native` to include Python tests in the test run.

#include <cstdlib>
#include <unity.h>

static void run_python_tests() {
    // Run pytest on consolidated test directory
    // Use --tb=short for concise output in CI
    int rc = std::system("poetry run pytest tools/serial_cli/tests/ -v --tb=short");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, rc, "Python CLI tests failed");
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(run_python_tests);
    return UNITY_END();
}
