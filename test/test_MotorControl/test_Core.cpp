#ifdef ARDUINO
#include <Arduino.h>
#endif
#include <unity.h>
#include <string>
#include <vector>
#include "MotorControl/MotorCommandProcessor.h"

static MotorCommandProcessor proto;

// Forward declarations for tests implemented in test_HelpStatus.cpp
void test_help_format();
void test_status_format_lines();
// Forward declarations for tests implemented in test_HomeSequencing.cpp
void test_home_parsing_comma_skips();
void test_home_busy_rule_reject_when_moving();
void test_home_all_concurrency_and_post_state();
void test_help_includes_home_line_again();

void setUp() {
  proto = MotorCommandProcessor();
}

void tearDown() {}

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

void test_move_out_of_range() {
  auto r1 = proto.processLine("MOVE:0,2000", 0);
  TEST_ASSERT_EQUAL_STRING("CTRL:ERR E07 POS_OUT_OF_RANGE", r1.c_str());
  auto r2 = proto.processLine("MOVE:0,-1300", 0);
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
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", r1.c_str());
  auto r2 = proto.processLine("MOVE:0,200", 10);
  TEST_ASSERT_EQUAL_STRING("CTRL:ERR E04 BUSY", r2.c_str());
  auto r3 = proto.processLine("MOVE:ALL,50", 20);
  TEST_ASSERT_EQUAL_STRING("CTRL:ERR E04 BUSY", r3.c_str());
  proto.tick(1100);
  auto r4 = proto.processLine("MOVE:0,200", 1101);
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", r4.c_str());
}

void test_home_defaults() {
  auto r1 = proto.processLine("HOME:0", 0);
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", r1.c_str());
  auto r2 = proto.processLine("MOVE:0,10", 10);
  TEST_ASSERT_EQUAL_STRING("CTRL:ERR E04 BUSY", r2.c_str());
  proto.tick(300);
  auto r3 = proto.processLine("MOVE:0,10", 310);
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", r3.c_str());
}

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

void test_sleep_busy_then_ok() {
  auto r1 = proto.processLine("MOVE:0,100,100", 0);
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", r1.c_str());
  auto r2 = proto.processLine("SLEEP:0", 10);
  TEST_ASSERT_EQUAL_STRING("CTRL:ERR E04 BUSY", r2.c_str());
  proto.tick(1100);
  auto r3 = proto.processLine("SLEEP:0", 1200);
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", r3.c_str());
}

void test_move_all_out_of_range() {
  auto r = proto.processLine("MOVE:ALL,1300", 0);
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
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", r.c_str());
}

void test_move_sets_speed_accel_in_status() {
  auto r1 = proto.processLine("MOVE:0,10,5000,12000", 0);
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", r1.c_str());
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

void test_bitpack_dir_sleep_basic();
// Hardware backend tests (declared in test_HardwareBackend.cpp)
void test_backend_latch_before_start();
void test_backend_dir_bits_per_target();
void test_backend_wake_sleep_overrides();
void test_backend_busy_rule_overlapping_move();
void test_backend_dir_latched_once_per_move();
void test_backend_speed_accel_passed_to_adapter();

#ifdef ARDUINO
void setup() {
  delay(200);
  UNITY_BEGIN();
  RUN_TEST(test_bad_cmd);
  RUN_TEST(test_bad_id);
  RUN_TEST(test_bad_param);
  RUN_TEST(test_move_out_of_range);
  RUN_TEST(test_all_addressing_and_status_awake);
  RUN_TEST(test_busy_rule_and_completion);
  RUN_TEST(test_home_defaults);
  RUN_TEST(test_help_format);
  RUN_TEST(test_status_format_lines);
  // HOME sequencing and parsing tests
  RUN_TEST(test_home_parsing_comma_skips);
  RUN_TEST(test_home_busy_rule_reject_when_moving);
  RUN_TEST(test_home_all_concurrency_and_post_state);
  RUN_TEST(test_help_includes_home_line_again);
  RUN_TEST(test_sleep_busy_then_ok);
  RUN_TEST(test_move_all_out_of_range);
  RUN_TEST(test_wake_sleep_single_and_status);
  RUN_TEST(test_home_with_params_acceptance);
  RUN_TEST(test_move_sets_speed_accel_in_status);
  RUN_TEST(test_bitpack_dir_sleep_basic);
  // Hardware backend sequencing tests
  RUN_TEST(test_backend_latch_before_start);
  RUN_TEST(test_backend_dir_bits_per_target);
  RUN_TEST(test_backend_wake_sleep_overrides);
  RUN_TEST(test_backend_busy_rule_overlapping_move);
  RUN_TEST(test_backend_dir_latched_once_per_move);
  RUN_TEST(test_backend_speed_accel_passed_to_adapter);
  UNITY_END();
  // Keep MCU idle to avoid accidental re-runs/reboots during test teardown
  while (true) { delay(1000); }
}

void loop() {}
#else
int main(int, char**) {
  UNITY_BEGIN();
  setUp();
  RUN_TEST(test_bad_cmd);
  setUp();
  RUN_TEST(test_bad_id);
  setUp();
  RUN_TEST(test_bad_param);
  setUp();
  RUN_TEST(test_move_out_of_range);
  setUp();
  RUN_TEST(test_all_addressing_and_status_awake);
  setUp();
  RUN_TEST(test_busy_rule_and_completion);
  setUp();
  RUN_TEST(test_home_defaults);
  setUp();
  RUN_TEST(test_help_format);
  setUp();
  RUN_TEST(test_status_format_lines);
  setUp();
  RUN_TEST(test_home_parsing_comma_skips);
  setUp();
  RUN_TEST(test_home_busy_rule_reject_when_moving);
  setUp();
  RUN_TEST(test_home_all_concurrency_and_post_state);
  setUp();
  RUN_TEST(test_help_includes_home_line_again);
  setUp();
  RUN_TEST(test_sleep_busy_then_ok);
  setUp();
  RUN_TEST(test_move_all_out_of_range);
  setUp();
  RUN_TEST(test_wake_sleep_single_and_status);
  setUp();
  RUN_TEST(test_home_with_params_acceptance);
  setUp();
  RUN_TEST(test_move_sets_speed_accel_in_status);
  RUN_TEST(test_bitpack_dir_sleep_basic);
  // Hardware backend sequencing tests
  RUN_TEST(test_backend_latch_before_start);
  RUN_TEST(test_backend_dir_bits_per_target);
  RUN_TEST(test_backend_wake_sleep_overrides);
  RUN_TEST(test_backend_busy_rule_overlapping_move);
  RUN_TEST(test_backend_dir_latched_once_per_move);
  RUN_TEST(test_backend_speed_accel_passed_to_adapter);
  return UNITY_END();
}
#endif
