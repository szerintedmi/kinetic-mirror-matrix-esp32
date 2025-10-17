#ifdef ARDUINO
#include <Arduino.h>
#endif
#include <unity.h>
#include <string>
#include <vector>
#include "MotorControl/MotorCommandProcessor.h"
#include "MotorControl/MotorControlConstants.h"

static MotorCommandProcessor proto_home;

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

static bool line_for_id_has(const std::vector<std::string>& lines, int id, const std::string& needle) {
  for (const auto &L : lines) {
    if (L.find("id=" + std::to_string(id)) == 0 || L.find("id=" + std::to_string(id) + " ") == 0) {
      return L.find(needle) != std::string::npos;
    }
  }
  return false;
}

void test_home_parsing_comma_skips() {
  proto_home = MotorCommandProcessor();
  // Comma-skip overshoot, set backoff to DEFAULT_BACKOFF
  std::string cmd = std::string("HOME:2,,") + std::to_string(MotorControlConstants::DEFAULT_BACKOFF);
  auto r = proto_home.processLine(cmd, 0);
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", r.c_str());
  // Should be moving immediately
  auto st0 = proto_home.processLine("STATUS", 1);
  auto lines0 = split_lines(st0);
  TEST_ASSERT_TRUE(line_for_id_has(lines0, 2, " moving=1"));
  // After sufficient time, post-state should be zeroed and asleep
  proto_home.tick(1000);
  auto st1 = proto_home.processLine("STATUS", 1001);
  auto lines1 = split_lines(st1);
  TEST_ASSERT_TRUE(line_for_id_has(lines1, 2, " pos=0"));
  TEST_ASSERT_TRUE(line_for_id_has(lines1, 2, " moving=0"));
  TEST_ASSERT_TRUE(line_for_id_has(lines1, 2, " awake=0"));
}

void test_home_busy_rule_reject_when_moving() {
  proto_home = MotorCommandProcessor();
  auto r1 = proto_home.processLine("MOVE:2,100,100", 0);
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", r1.c_str());
  auto r2 = proto_home.processLine("HOME:2", 10);
  TEST_ASSERT_EQUAL_STRING("CTRL:ERR E04 BUSY", r2.c_str());
  // After completion, HOME should be accepted
  proto_home.tick(1200);
  auto r3 = proto_home.processLine("HOME:2", 1201);
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", r3.c_str());
}

void test_home_all_concurrency_and_post_state() {
  proto_home = MotorCommandProcessor();
  auto r = proto_home.processLine("HOME:ALL", 0);
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", r.c_str());
  auto st0 = proto_home.processLine("STATUS", 1);
  auto lines0 = split_lines(st0);
  int moving_cnt = 0;
  for (int i = 0; i < 8; ++i) {
    if (line_for_id_has(lines0, i, " moving=1")) moving_cnt++;
  }
  TEST_ASSERT_EQUAL_INT(8, moving_cnt);

  // Advance time past default homing duration (~(800+150)/4000 s => < 300ms)
  proto_home.tick(1000);
  auto st1 = proto_home.processLine("STATUS", 1001);
  auto lines1 = split_lines(st1);
  for (int i = 0; i < 8; ++i) {
    TEST_ASSERT_TRUE(line_for_id_has(lines1, i, " pos=0"));
    TEST_ASSERT_TRUE(line_for_id_has(lines1, i, " moving=0"));
    TEST_ASSERT_TRUE(line_for_id_has(lines1, i, " awake=0"));
  }
}

void test_help_includes_home_line_again() {
  proto_home = MotorCommandProcessor();
  auto help = proto_home.processLine("HELP", 0);
  TEST_ASSERT_TRUE(help.find("HOME:<id|ALL>[,<overshoot>][,<backoff>][,<speed>][,<accel>][,<full_range>]") != std::string::npos);
}

// No main() here; runner is in test_Core.cpp
