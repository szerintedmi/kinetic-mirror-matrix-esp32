#ifdef ARDUINO
#include <Arduino.h>
#endif
#include <unity.h>
#include <string>
#include <vector>
#include "MotorControl/MotorCommandProcessor.h"
#include "MotorControl/MotorControlConstants.h"
#include "MotorControl/MotionKinematics.h"

static MotorCommandProcessor proto;

void setUp() {
  proto = MotorCommandProcessor();
}

void tearDown() {}

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

// Protocol basics
void test_bad_cmd() {
  auto resp = proto.processLine("FOO", 0);
  TEST_ASSERT_EQUAL_STRING("CTRL:ERR E01 BAD_CMD", resp.c_str());
}

void test_bad_id() {
  auto r1 = proto.processLine("WAKE:9", 0);
  TEST_ASSERT_EQUAL_STRING("CTRL:ERR E02 BAD_ID", r1.c_str());
  auto r2 = proto.processLine("WAKE:-1", 0);
  TEST_ASSERT_EQUAL_STRING("CTRL:ERR E02 BAD_ID", r2.c_str());
}

void test_bad_param() {
  auto r1 = proto.processLine("MOVE:0,abc", 0);
  TEST_ASSERT_EQUAL_STRING("CTRL:ERR E03 BAD_PARAM", r1.c_str());
  auto r2 = proto.processLine("HOME:0,foo", 0);
  TEST_ASSERT_EQUAL_STRING("CTRL:ERR E03 BAD_PARAM", r2.c_str());
}

void test_help_format() {
  std::string help = proto.processLine("HELP", 0);
  TEST_ASSERT_TRUE(help.find("HELP") != std::string::npos);
  TEST_ASSERT_TRUE(help.find("STATUS") != std::string::npos);
  TEST_ASSERT_TRUE(help.find("MOVE:<id|ALL>,<abs_steps>[,<speed>][,<accel>]") != std::string::npos);
  TEST_ASSERT_TRUE(help.find("HOME:<id|ALL>[,<overshoot>][,<backoff>][,<speed>][,<accel>][,<full_range>]") != std::string::npos);
  TEST_ASSERT_TRUE(help.find("WAKE:<id|ALL>") != std::string::npos);
  TEST_ASSERT_TRUE(help.find("SLEEP:<id|ALL>") != std::string::npos);
  TEST_ASSERT_TRUE(help.find("E0") == std::string::npos);
  TEST_ASSERT_TRUE(help.find("ERR") == std::string::npos);
}

void test_status_format_lines() {
  std::string st = proto.processLine("STATUS", 0);
  auto lines = split_lines(st);
  TEST_ASSERT_EQUAL_INT(8, (int)lines.size());
  const std::string &L0 = lines[0];
  TEST_ASSERT_TRUE(L0.find("id=") != std::string::npos);
  TEST_ASSERT_TRUE(L0.find(" pos=") != std::string::npos);
  TEST_ASSERT_TRUE(L0.find(" speed=") != std::string::npos);
  TEST_ASSERT_TRUE(L0.find(" accel=") != std::string::npos);
  TEST_ASSERT_TRUE(L0.find(" moving=") != std::string::npos);
  TEST_ASSERT_TRUE(L0.find(" awake=") != std::string::npos);
  TEST_ASSERT_TRUE(L0.find(" homed=") != std::string::npos);
  TEST_ASSERT_TRUE(L0.find(" steps_since_home=") != std::string::npos);
  TEST_ASSERT_TRUE(L0.find(" budget_s=") != std::string::npos);
  TEST_ASSERT_TRUE(L0.find(" ttfc_s=") != std::string::npos);
}

// Motion, wake/sleep, HOME
void test_move_out_of_range() {
  std::string cmd_hi = std::string("MOVE:0,") + std::to_string(MotorControlConstants::MAX_POS_STEPS + 1);
  auto r1 = proto.processLine(cmd_hi, 0);
  TEST_ASSERT_EQUAL_STRING("CTRL:ERR E07 POS_OUT_OF_RANGE", r1.c_str());
  std::string cmd_lo = std::string("MOVE:0,") + std::to_string(MotorControlConstants::MIN_POS_STEPS - 1);
  auto r2 = proto.processLine(cmd_lo, 0);
  TEST_ASSERT_EQUAL_STRING("CTRL:ERR E07 POS_OUT_OF_RANGE", r2.c_str());
}

void test_all_addressing_and_status_awake() {
  auto r1 = proto.processLine("WAKE:ALL", 0);
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", r1.c_str());
  auto st = proto.processLine("STATUS", 0);
  TEST_ASSERT_TRUE_MESSAGE(st.find("awake=1") != std::string::npos, "Expected awake=1 in STATUS");
}

void test_busy_rule_and_completion() {
  auto r1 = proto.processLine("MOVE:0,100,100", 0);
  TEST_ASSERT_TRUE(r1.rfind("CTRL:OK", 0) == 0);
  auto r2 = proto.processLine("MOVE:0,200", 10);
  TEST_ASSERT_EQUAL_STRING("CTRL:ERR E04 BUSY", r2.c_str());
  auto r3 = proto.processLine("MOVE:ALL,50", 20);
  TEST_ASSERT_EQUAL_STRING("CTRL:ERR E04 BUSY", r3.c_str());
  proto.tick(1100);
  auto r4 = proto.processLine("MOVE:0,200", 1101);
  TEST_ASSERT_TRUE(r4.rfind("CTRL:OK", 0) == 0);
}

void test_home_defaults() {
  auto r1 = proto.processLine("HOME:0", 0);
  TEST_ASSERT_TRUE(r1.rfind("CTRL:OK", 0) == 0);
  auto r2 = proto.processLine("MOVE:0,10", 10);
  TEST_ASSERT_EQUAL_STRING("CTRL:ERR E04 BUSY", r2.c_str());
  uint32_t th = MotionKinematics::estimateHomeTimeMs(
      MotorControlConstants::DEFAULT_OVERSHOOT,
      MotorControlConstants::DEFAULT_BACKOFF,
      MotorControlConstants::DEFAULT_SPEED_SPS,
      MotorControlConstants::DEFAULT_ACCEL_SPS2);
  proto.tick(th);
  auto r3 = proto.processLine("MOVE:0,10", th + 10);
  TEST_ASSERT_TRUE(r3.rfind("CTRL:OK", 0) == 0);
}

void test_sleep_busy_then_ok() {
  auto r1 = proto.processLine("MOVE:0,100,100", 0);
  TEST_ASSERT_TRUE(r1.rfind("CTRL:OK", 0) == 0);
  auto r2 = proto.processLine("SLEEP:0", 10);
  TEST_ASSERT_EQUAL_STRING("CTRL:ERR E04 BUSY", r2.c_str());
  proto.tick(1100);
  auto r3 = proto.processLine("SLEEP:0", 1200);
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", r3.c_str());
}

void test_move_all_out_of_range() {
  std::string cmd = std::string("MOVE:ALL,") + std::to_string(MotorControlConstants::MAX_POS_STEPS + 100);
  auto r = proto.processLine(cmd, 0);
  TEST_ASSERT_EQUAL_STRING("CTRL:ERR E07 POS_OUT_OF_RANGE", r.c_str());
}

void test_wake_sleep_single_and_status() {
  auto r1 = proto.processLine("WAKE:1", 0);
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", r1.c_str());
  auto st1 = proto.processLine("STATUS", 1);
  auto lines1 = split_lines(st1);
  bool found_awake = false;
  for (const auto &L : lines1) {
    if (L.find("id=1 ") == 0 || L.find("id=1") == 0) {
      found_awake = (L.find(" awake=1") != std::string::npos || L.find(" awake=1") != std::string::npos);
      break;
    }
  }
  TEST_ASSERT_TRUE(found_awake);
  auto r2 = proto.processLine("SLEEP:1", 2);
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", r2.c_str());
  auto st2 = proto.processLine("STATUS", 3);
  auto lines2 = split_lines(st2);
  bool found_asleep = false;
  for (const auto &L : lines2) {
    if (L.find("id=1 ") == 0 || L.find("id=1") == 0) {
      found_asleep = (L.find(" awake=0") != std::string::npos);
      break;
    }
  }
  TEST_ASSERT_TRUE(found_asleep);
}

void test_home_with_params_acceptance() {
  auto r = proto.processLine("HOME:0,900,150,500,1000,2400", 0);
  TEST_ASSERT_TRUE(r.rfind("CTRL:OK", 0) == 0);
}

void test_move_sets_speed_accel_in_status() {
  auto r1 = proto.processLine("MOVE:0,10,5000,12000", 0);
  TEST_ASSERT_TRUE(r1.rfind("CTRL:OK", 0) == 0);
  auto st = proto.processLine("STATUS", 1);
  auto lines = split_lines(st);
  bool ok = false;
  for (const auto &L : lines) {
    if (L.find("id=0 ") == 0 || L.find("id=0") == 0) {
      ok = (L.find(" speed=5000") != std::string::npos) && (L.find(" accel=12000") != std::string::npos);
      break;
    }
  }
  TEST_ASSERT_TRUE(ok);
}

// HOME sequencing (from previous dedicated file)
static bool line_for_id_has(const std::vector<std::string>& lines, int id, const std::string& needle) {
  for (const auto &L : lines) {
    if (L.find("id=" + std::to_string(id)) == 0 || L.find("id=" + std::to_string(id) + " ") == 0) {
      return L.find(needle) != std::string::npos;
    }
  }
  return false;
}

void test_home_parsing_comma_skips() {
  // Comma-skip overshoot, set backoff to DEFAULT_BACKOFF
  std::string cmd = std::string("HOME:2,,") + std::to_string(MotorControlConstants::DEFAULT_BACKOFF);
  auto r = proto.processLine(cmd, 0);
  TEST_ASSERT_TRUE(r.rfind("CTRL:OK", 0) == 0);
  auto st0 = proto.processLine("STATUS", 1);
  auto lines0 = split_lines(st0);
  TEST_ASSERT_TRUE(line_for_id_has(lines0, 2, " moving=1"));
  // After sufficient time, post-state should be zeroed and asleep
  proto.tick(1000);
  auto st1 = proto.processLine("STATUS", 1001);
  auto lines1 = split_lines(st1);
  TEST_ASSERT_TRUE(line_for_id_has(lines1, 2, " pos=0"));
  TEST_ASSERT_TRUE(line_for_id_has(lines1, 2, " moving=0"));
  TEST_ASSERT_TRUE(line_for_id_has(lines1, 2, " awake=0"));
}

void test_home_busy_rule_reject_when_moving() {
  auto r1 = proto.processLine("MOVE:2,100,100", 0);
  TEST_ASSERT_TRUE(r1.rfind("CTRL:OK", 0) == 0);
  auto r2 = proto.processLine("HOME:2", 10);
  TEST_ASSERT_EQUAL_STRING("CTRL:ERR E04 BUSY", r2.c_str());
  // After completion, HOME should be accepted
  proto.tick(1200);
  auto r3 = proto.processLine("HOME:2", 1201);
  TEST_ASSERT_TRUE(r3.rfind("CTRL:OK", 0) == 0);
}

void test_home_all_concurrency_and_post_state() {
  auto r = proto.processLine("HOME:ALL", 0);
  TEST_ASSERT_TRUE(r.rfind("CTRL:OK", 0) == 0);
  auto st0 = proto.processLine("STATUS", 1);
  auto lines0 = split_lines(st0);
  int moving_cnt = 0;
  for (int i = 0; i < 8; ++i) {
    if (line_for_id_has(lines0, i, " moving=1")) moving_cnt++;
  }
  TEST_ASSERT_EQUAL_INT(8, moving_cnt);

  // Advance time past default homing duration (~(800+150)/4000 s => < 300ms)
  proto.tick(1000);
  auto st1 = proto.processLine("STATUS", 1001);
  auto lines1 = split_lines(st1);
  for (int i = 0; i < 8; ++i) {
    TEST_ASSERT_TRUE(line_for_id_has(lines1, i, " pos=0"));
    TEST_ASSERT_TRUE(line_for_id_has(lines1, i, " moving=0"));
    TEST_ASSERT_TRUE(line_for_id_has(lines1, i, " awake=0"));
  }
}

void test_help_includes_home_line_again() {
  auto help = proto.processLine("HELP", 0);
  TEST_ASSERT_TRUE(help.find("HOME:<id|ALL>[,<overshoot>][,<backoff>][,<speed>][,<accel>][,<full_range>]") != std::string::npos);
}
