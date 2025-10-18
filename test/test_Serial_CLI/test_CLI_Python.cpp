// Unity runner that discovers and runs all test_*.py scripts under
// the current suite folder as individual Unity tests (rename-safe).

#include <unity.h>
#include <dirent.h>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>

static std::string g_script;
static std::string g_name;

static int run_python_file(const std::string &path)
{
    // Try python3 first, then python
    std::string cmd = std::string("python3 ") + path;
    int rc = std::system(cmd.c_str());
    if (rc != 0)
    {
        cmd = std::string("python ") + path;
        rc = std::system(cmd.c_str());
    }
    return rc;
}

static std::vector<std::string> discover_py_tests(const std::string &dir)
{
    std::vector<std::string> files;
    DIR *dp = opendir(dir.c_str());
    if (!dp)
        return files;
    struct dirent *ent;
    while ((ent = readdir(dp)) != nullptr)
    {
        std::string name = ent->d_name;
        if (name.size() >= 3 && name[0] == '.')
            continue;
        if (name.rfind("test_", 0) == 0 && name.size() > 3)
        {
            if (name.size() >= 3 && name.substr(name.size() - 3) == std::string(".py"))
            {
                files.push_back(dir + "/" + name);
            }
        }
    }
    closedir(dp);
    std::sort(files.begin(), files.end());
    return files;
}

static std::string current_source_dir()
{
    std::string p = __FILE__;
    size_t pos = p.find_last_of("/\\");
    if (pos == std::string::npos) return std::string(".");
    return p.substr(0, pos);
}

static void run_current_python_test()
{
    int rc = run_python_file(g_script);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, rc, g_script.c_str());
}

int main()
{
    UNITY_BEGIN();
    auto files = discover_py_tests(current_source_dir());
    for (const auto &f : files)
    {
        g_script = f;
        // Use just the filename as the Unity test name
        size_t pos = f.find_last_of("/\\");
        g_name = (pos == std::string::npos) ? f : f.substr(pos + 1);
        UnityDefaultTestRun(run_current_python_test, g_name.c_str(), __LINE__);
    }
    return UNITY_END();
}
