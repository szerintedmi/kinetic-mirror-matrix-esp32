#include <unity.h>
#include <cstdlib>
#include <string>

static int run_with(const char *exe)
{
    std::string cmd = std::string(exe) + " tools/tests/python/run_cli_tests.py";
    return std::system(cmd.c_str());
}

void test_python_cli_suite()
{
    int rc = run_with("python3");
    if (rc != 0)
    {
        rc = run_with("python");
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, rc, "Python CLI tests failed");
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_python_cli_suite);
    return UNITY_END();
}
