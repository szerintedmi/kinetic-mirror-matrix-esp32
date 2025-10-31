#ifdef ARDUINO
#include <Arduino.h>
#endif
#include <unity.h>
#include <cstdio>
#include <string>
#include "MotorControl/MotorCommandProcessor.h"
#include "MotorControl/MotorControlConstants.h"

void test_help_includes_thermal_get_set() {
  MotorCommandProcessor p;
  std::string help = p.processLine("HELP", 0);
  TEST_ASSERT_TRUE(help.find("GET THERMAL_LIMITING") != std::string::npos);
  TEST_ASSERT_TRUE(help.find("SET THERMAL_LIMITING=OFF|ON") != std::string::npos);
}

void test_get_thermal_runtime_limiting_default_on_and_max_budget() {
  MotorCommandProcessor p;
  std::string r1 = p.processLine("GET THERMAL_LIMITING", 0);
  TEST_ASSERT_TRUE(r1.rfind("CTRL:ACK ", 0) == 0);
  TEST_ASSERT_TRUE(r1.find(" THERMAL_LIMITING=ON") != std::string::npos);
  std::string exp = std::string("max_budget_s=") + std::to_string((int)MotorControlConstants::MAX_RUNNING_TIME_S);
  TEST_ASSERT_TRUE(r1.find(exp) != std::string::npos);
  std::string s = p.processLine("SET THERMAL_LIMITING=OFF", 0);
  TEST_ASSERT_TRUE(s.rfind("CTRL:DONE", 0) == 0);
  std::string r2 = p.processLine("GET THERMAL_LIMITING", 0);
  TEST_ASSERT_TRUE(r2.find("THERMAL_LIMITING=OFF") != std::string::npos);
}

void test_preflight_e10_move_enabled_err() {
  MotorCommandProcessor p;
  // Very slow long move via global SPEED -> required runtime exceeds MAX_RUNNING_TIME_S
  TEST_ASSERT_TRUE(p.processLine("SET SPEED=1", 0).rfind("CTRL:DONE", 0) == 0);
  TEST_ASSERT_TRUE(p.processLine("SET ACCEL=1000", 0).rfind("CTRL:DONE", 0) == 0);
  std::string r = p.processLine("MOVE:0,1200", 0);
  TEST_ASSERT_TRUE(r.rfind("CTRL:ERR ", 0) == 0);
  TEST_ASSERT_TRUE(r.find(" E10 THERMAL_REQ_GT_MAX") != std::string::npos);
  TEST_ASSERT_TRUE(r.find(" id=0 ") != std::string::npos || r.find(" id=0") != std::string::npos);
}

void test_preflight_e11_move_enabled_err() {
  MotorCommandProcessor p;
  // Drain budget by staying awake for a long time
  TEST_ASSERT_TRUE(p.processLine("WAKE:0", 0).rfind("CTRL:ACK", 0) == 0);
  // After 100s, budget will be <= 0
  (void)p.processLine("STATUS", 100000);
  TEST_ASSERT_TRUE(p.processLine("SET SPEED=4000", 0).rfind("CTRL:DONE", 0) == 0);
  TEST_ASSERT_TRUE(p.processLine("SET ACCEL=16000", 0).rfind("CTRL:DONE", 0) == 0);
  std::string r = p.processLine("MOVE:0,10", 100000);
  TEST_ASSERT_TRUE(r.rfind("CTRL:ERR ", 0) == 0);
  TEST_ASSERT_TRUE(r.find(" E11 THERMAL_NO_BUDGET") != std::string::npos);
}

void test_preflight_warn_when_disabled_then_ok() {
  MotorCommandProcessor p;
  // Disable limits
  TEST_ASSERT_TRUE(p.processLine("SET THERMAL_LIMITING=OFF", 0).rfind("CTRL:DONE", 0) == 0);
  // This would exceed max -> expect WARN then OK
  TEST_ASSERT_TRUE(p.processLine("SET SPEED=1", 0).rfind("CTRL:DONE", 0) == 0);
  TEST_ASSERT_TRUE(p.processLine("SET ACCEL=1000", 0).rfind("CTRL:DONE", 0) == 0);
  std::string r1 = p.processLine("MOVE:0,1200", 0);
  bool r1_warn = (r1.rfind("CTRL:WARN ", 0) == 0) && (r1.find(" THERMAL_REQ_GT_MAX") != std::string::npos);
  if (!r1_warn) { std::printf("[DEBUG] r1 unexpected response:\n%s\n", r1.c_str()); std::fflush(stdout); }
  TEST_ASSERT_TRUE_MESSAGE(r1_warn, "Expected r1 to start with CTRL:WARN THERMAL_REQ_GT_MAX");
  bool r1_ok = (r1.find("\nCTRL:ACK msg_id=") != std::string::npos) && (r1.find(" est_ms=") != std::string::npos);
  if (!r1_ok) { std::printf("[DEBUG] r1 missing final ACK est_ms:\n%s\n", r1.c_str()); std::fflush(stdout); }
  TEST_ASSERT_TRUE_MESSAGE(r1_ok, "Expected r1 to include final CTRL:ACK est_ms");

  // Drain budget and try a small move -> expect WARN then OK
  // Use a different motor id (1) to avoid BUSY from the long-running move on id=0
  TEST_ASSERT_TRUE(p.processLine("WAKE:1", 0).rfind("CTRL:ACK", 0) == 0);
  (void)p.processLine("STATUS", 100000);
  std::string r2 = p.processLine("MOVE:1,10", 100000);
  bool r2_warn = (r2.rfind("CTRL:WARN ", 0) == 0) && (r2.find(" THERMAL_NO_BUDGET") != std::string::npos);
  if (!r2_warn) { std::printf("[DEBUG] r2 unexpected response:\n%s\n", r2.c_str()); std::fflush(stdout); }
  TEST_ASSERT_TRUE_MESSAGE(r2_warn, "Expected r2 to start with CTRL:WARN THERMAL_NO_BUDGET");
  bool r2_ok = (r2.find("\nCTRL:ACK msg_id=") != std::string::npos) && (r2.find(" est_ms=") != std::string::npos);
  if (!r2_ok) { std::printf("[DEBUG] r2 missing final ACK est_ms:\n%s\n", r2.c_str()); std::fflush(stdout); }
  TEST_ASSERT_TRUE_MESSAGE(r2_ok, "Expected r2 to include final CTRL:ACK est_ms");
}

void test_preflight_e10_home_enabled_err() {
  MotorCommandProcessor p;
  // HOME with global speed=1 => ~850s required > MAX_RUNNING_TIME_S
  TEST_ASSERT_TRUE(p.processLine("SET SPEED=1", 0).rfind("CTRL:DONE", 0) == 0);
  std::string r = p.processLine("HOME:0", 0);
  TEST_ASSERT_TRUE(r.rfind("CTRL:ERR ", 0) == 0);
  TEST_ASSERT_TRUE(r.find(" E10 THERMAL_REQ_GT_MAX") != std::string::npos);
}

void test_move_ok_returns_estimate() {
  MotorCommandProcessor p;
  TEST_ASSERT_TRUE(p.processLine("SET SPEED=4000", 0).rfind("CTRL:DONE", 0) == 0);
  TEST_ASSERT_TRUE(p.processLine("SET ACCEL=16000", 0).rfind("CTRL:DONE", 0) == 0);
  std::string r = p.processLine("MOVE:0,10", 0);
  TEST_ASSERT_TRUE(r.rfind("CTRL:ACK msg_id=", 0) == 0);
  TEST_ASSERT_TRUE(r.find(" est_ms=") != std::string::npos);
}

void test_home_ok_returns_estimate() {
  MotorCommandProcessor p;
  std::string r = p.processLine("HOME:0", 0);
  TEST_ASSERT_TRUE(r.rfind("CTRL:ACK msg_id=", 0) == 0);
  TEST_ASSERT_TRUE(r.find(" est_ms=") != std::string::npos);
}

void test_last_op_timing_move() {
  MotorCommandProcessor p;
  TEST_ASSERT_TRUE(p.processLine("SET SPEED=1000", 0).rfind("CTRL:DONE", 0) == 0);
  TEST_ASSERT_TRUE(p.processLine("SET ACCEL=1000", 0).rfind("CTRL:DONE", 0) == 0);
  std::string r = p.processLine("MOVE:0,50", 0);
  TEST_ASSERT_TRUE(r.rfind("CTRL:ACK msg_id=", 0) == 0);
  TEST_ASSERT_TRUE(r.find(" est_ms=") != std::string::npos);
  // During operation, device may report either pre-start or moving state; skip strict check
  // After completion
  // Ensure controller advanced beyond completion
  p.tick(1000);
  std::string t1 = p.processLine("GET:LAST_OP_TIMING:0", 1000);
  TEST_ASSERT_TRUE(t1.find("LAST_OP_TIMING") != std::string::npos);
  TEST_ASSERT_TRUE(t1.find(" actual_ms=") != std::string::npos);
}

void test_last_op_timing_home() {
  MotorCommandProcessor p;
  std::string r = p.processLine("HOME:0", 0);
  TEST_ASSERT_TRUE(r.rfind("CTRL:ACK msg_id=", 0) == 0);
  TEST_ASSERT_TRUE(r.find(" est_ms=") != std::string::npos);
  // During operation, behavior may vary by timing; validate post-completion only
  p.tick(2000);
  std::string t1 = p.processLine("GET:LAST_OP_TIMING:0", 2000);
  TEST_ASSERT_TRUE(t1.find("LAST_OP_TIMING") != std::string::npos);
  TEST_ASSERT_TRUE(t1.find(" actual_ms=") != std::string::npos);
}

void test_wake_reject_enabled_no_budget() {
  MotorCommandProcessor p;
  // Drain budget by being awake
  TEST_ASSERT_TRUE(p.processLine("WAKE:0", 0).rfind("CTRL:ACK", 0) == 0);
  (void)p.processLine("STATUS", 100000); // 100s later -> no budget
  // Go to sleep to allow WAKE command
  TEST_ASSERT_TRUE(p.processLine("SLEEP:0", 100000).rfind("CTRL:ACK", 0) == 0);
  std::string r = p.processLine("WAKE:0", 100001);
  TEST_ASSERT_TRUE(r.rfind("CTRL:ERR ", 0) == 0);
  TEST_ASSERT_TRUE(r.find(" E12 THERMAL_NO_BUDGET_WAKE") != std::string::npos);
}

void test_wake_warn_disabled_no_budget_then_ok() {
  MotorCommandProcessor p;
  TEST_ASSERT_TRUE(p.processLine("SET THERMAL_LIMITING=OFF", 0).rfind("CTRL:DONE", 0) == 0);
  TEST_ASSERT_TRUE(p.processLine("WAKE:0", 0).rfind("CTRL:ACK", 0) == 0);
  (void)p.processLine("STATUS", 100000);
  TEST_ASSERT_TRUE(p.processLine("SLEEP:0", 100000).rfind("CTRL:ACK", 0) == 0);
  std::string r = p.processLine("WAKE:0", 100001);
  TEST_ASSERT_TRUE(r.rfind("CTRL:WARN ", 0) == 0);
  TEST_ASSERT_TRUE(r.find(" THERMAL_NO_BUDGET_WAKE") != std::string::npos);
  TEST_ASSERT_TRUE(r.find("\nCTRL:ACK") != std::string::npos);
}

void test_auto_sleep_overrun_cancels_move_and_awake() {
  MotorCommandProcessor p;
  // Disable to allow starting a very long move
  TEST_ASSERT_TRUE(p.processLine("SET THERMAL_LIMITING=OFF", 0).rfind("CTRL:DONE", 0) == 0);
  // Long move: target at limit with very low speed to exceed budget
  TEST_ASSERT_TRUE(p.processLine("SET SPEED=1", 0).rfind("CTRL:DONE", 0) == 0);
  TEST_ASSERT_TRUE(p.processLine("SET ACCEL=1000", 0).rfind("CTRL:DONE", 0) == 0);
  std::string r = p.processLine("MOVE:0,1200", 0);
  TEST_ASSERT_TRUE(r.find("CTRL:ACK ") != std::string::npos && r.find(" est_ms=") != std::string::npos);
  // Re-enable enforcement
  TEST_ASSERT_TRUE(p.processLine("SET THERMAL_LIMITING=ON", 0).rfind("CTRL:DONE", 0) == 0);
  // Advance time past zero budget + grace period
  const uint32_t t_ms = (MotorControlConstants::MAX_RUNNING_TIME_S + MotorControlConstants::AUTO_SLEEP_IF_OVER_BUDGET_S + 1) * 1000;
  (void)p.processLine("STATUS", t_ms);
  // Verify auto-slept and move canceled
  std::string st = p.processLine("STATUS", t_ms + 1);
  // Extract line for id=0
  size_t start = 0;
  bool found_line = false;
  std::string line0;
  while (start <= st.size()) {
    size_t pos = st.find('\n', start);
    std::string line = (pos == std::string::npos) ? st.substr(start) : st.substr(start, pos - start);
    if (line.rfind("id=0", 0) == 0 || line.find("id=0 ") == 0) { line0 = line; found_line = true; break; }
    if (pos == std::string::npos) break;
    start = pos + 1;
  }
  TEST_ASSERT_TRUE(found_line);
  TEST_ASSERT_TRUE(line0.find(" moving=0") != std::string::npos);
  TEST_ASSERT_TRUE(line0.find(" awake=0") != std::string::npos);
}
