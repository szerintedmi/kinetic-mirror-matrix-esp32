#ifdef ARDUINO
#include <Arduino.h>
#endif
#include <unity.h>
#include <string>
#include <vector>
#include "protocol/Protocol.h"

static Protocol proto2;

static std::vector<std::string> split_lines(const std::string &s) {
  std::vector<std::string> out;
  size_t start = 0;
  while (start <= s.size()) {
    size_t pos = s.find('\n', start);
    if (pos == std::string::npos) {
      out.push_back(s.substr(start));
      break;
    }
    out.push_back(s.substr(start, pos - start));
    start = pos + 1;
  }
  return out;
}

// Reset state per test at the beginning of each test function

void test_help_format() {
  proto2 = Protocol();
  std::string help = proto2.processLine("HELP", 0);
  // Should contain each command usage line
  TEST_ASSERT_TRUE(help.find("HELP") != std::string::npos);
  TEST_ASSERT_TRUE(help.find("STATUS") != std::string::npos);
  TEST_ASSERT_TRUE(help.find("MOVE:<id|ALL>,<abs_steps>[,<speed>][,<accel>]") != std::string::npos);
  TEST_ASSERT_TRUE(help.find("HOME:<id|ALL>[,<overshoot>][,<backoff>][,<speed>][,<accel>][,<full_range>]") != std::string::npos);
  TEST_ASSERT_TRUE(help.find("WAKE:<id|ALL>") != std::string::npos);
  TEST_ASSERT_TRUE(help.find("SLEEP:<id|ALL>") != std::string::npos);
  // Should not include error codes or ERR tokens
  TEST_ASSERT_TRUE(help.find("E0") == std::string::npos);
  TEST_ASSERT_TRUE(help.find("ERR") == std::string::npos);
}

void test_status_format_lines() {
  proto2 = Protocol();
  std::string st = proto2.processLine("STATUS", 0);
  auto lines = split_lines(st);
  // Expect 8 motors => 8 lines
  TEST_ASSERT_EQUAL_INT(8, (int)lines.size());
  // Check first line contains all keys
  const std::string &L0 = lines[0];
  TEST_ASSERT_TRUE(L0.find("id=") != std::string::npos);
  TEST_ASSERT_TRUE(L0.find(" pos=") != std::string::npos);
  TEST_ASSERT_TRUE(L0.find(" speed=") != std::string::npos);
  TEST_ASSERT_TRUE(L0.find(" accel=") != std::string::npos);
  TEST_ASSERT_TRUE(L0.find(" moving=") != std::string::npos);
  TEST_ASSERT_TRUE(L0.find(" awake=") != std::string::npos);
}

// No standalone runner here; tests are executed from test_core.cpp runner.
