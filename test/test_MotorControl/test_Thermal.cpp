#ifdef ARDUINO
#include <Arduino.h>
#endif
#include <unity.h>
#include <string>
#include "MotorControl/MotorCommandProcessor.h"
#include "MotorControl/MotorControlConstants.h"

void test_help_includes_thermal_get_set() {
  MotorCommandProcessor p;
  std::string help = p.processLine("HELP", 0);
  TEST_ASSERT_TRUE(help.find("GET THERMAL_RUNTIME_LIMITING") != std::string::npos);
  TEST_ASSERT_TRUE(help.find("SET THERMAL_RUNTIME_LIMITING=OFF|ON") != std::string::npos);
}

void test_get_thermal_runtime_limiting_default_on_and_max_budget() {
  MotorCommandProcessor p;
  std::string r1 = p.processLine("GET THERMAL_RUNTIME_LIMITING", 0);
  TEST_ASSERT_TRUE(r1.find("CTRL:OK THERMAL_RUNTIME_LIMITING=ON") != std::string::npos);
  std::string exp = std::string("max_budget_s=") + std::to_string((int)MotorControlConstants::MAX_RUNNING_TIME_S);
  TEST_ASSERT_TRUE(r1.find(exp) != std::string::npos);
  std::string s = p.processLine("SET THERMAL_RUNTIME_LIMITING=OFF", 0);
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", s.c_str());
  std::string r2 = p.processLine("GET THERMAL_RUNTIME_LIMITING", 0);
  TEST_ASSERT_TRUE(r2.find("THERMAL_RUNTIME_LIMITING=OFF") != std::string::npos);
}

void test_preflight_e10_move_enabled_err() {
  MotorCommandProcessor p;
  // Very slow long move -> required runtime exceeds MAX_RUNNING_TIME_S
  std::string r = p.processLine("MOVE:0,1200,1,1000", 0);
  TEST_ASSERT_TRUE(r.find("CTRL:ERR E10 THERMAL_REQ_GT_MAX") == 0);
  TEST_ASSERT_TRUE(r.find(" id=0 ") != std::string::npos || r.find(" id=0") != std::string::npos);
}

void test_preflight_e11_move_enabled_err() {
  MotorCommandProcessor p;
  // Drain budget by staying awake for a long time
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", p.processLine("WAKE:0", 0).c_str());
  // After 100s, budget will be <= 0
  (void)p.processLine("STATUS", 100000);
  std::string r = p.processLine("MOVE:0,10,4000,16000", 100000);
  TEST_ASSERT_TRUE(r.find("CTRL:ERR E11 THERMAL_NO_BUDGET") == 0);
}

void test_preflight_warn_when_disabled_then_ok() {
  MotorCommandProcessor p;
  // Disable limits
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", p.processLine("SET THERMAL_RUNTIME_LIMITING=OFF", 0).c_str());
  // This would exceed max -> expect WARN then OK
  std::string r1 = p.processLine("MOVE:0,1200,1,1000", 0);
  TEST_ASSERT_TRUE(r1.find("CTRL:WARN THERMAL_REQ_GT_MAX") == 0);
  TEST_ASSERT_TRUE(r1.find("\nCTRL:OK est_ms=") != std::string::npos);

  // Drain budget and try a small move -> expect WARN then OK
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", p.processLine("WAKE:0", 0).c_str());
  (void)p.processLine("STATUS", 100000);
  std::string r2 = p.processLine("MOVE:0,10,4000,16000", 100000);
  TEST_ASSERT_TRUE(r2.find("CTRL:WARN THERMAL_NO_BUDGET") == 0);
  TEST_ASSERT_TRUE(r2.find("\nCTRL:OK est_ms=") != std::string::npos);
}

void test_preflight_e10_home_enabled_err() {
  MotorCommandProcessor p;
  // HOME with speed=1 => ~850s required > MAX_RUNNING_TIME_S
  std::string r = p.processLine("HOME:0,,,1", 0);
  TEST_ASSERT_TRUE(r.find("CTRL:ERR E10 THERMAL_REQ_GT_MAX") == 0);
}

void test_move_ok_returns_estimate() {
  MotorCommandProcessor p;
  std::string r = p.processLine("MOVE:0,10,4000,16000", 0);
  TEST_ASSERT_TRUE(r.find("CTRL:OK est_ms=") == 0);
}

void test_home_ok_returns_estimate() {
  MotorCommandProcessor p;
  std::string r = p.processLine("HOME:0", 0);
  TEST_ASSERT_TRUE(r.find("CTRL:OK est_ms=") == 0);
}

void test_last_op_timing_move() {
  MotorCommandProcessor p;
  std::string r = p.processLine("MOVE:0,50,1000,1000", 0);
  TEST_ASSERT_TRUE(r.find("CTRL:OK est_ms=") == 0);
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
  TEST_ASSERT_TRUE(r.find("CTRL:OK est_ms=") == 0);
  // During operation, behavior may vary by timing; validate post-completion only
  p.tick(2000);
  std::string t1 = p.processLine("GET:LAST_OP_TIMING:0", 2000);
  TEST_ASSERT_TRUE(t1.find("LAST_OP_TIMING") != std::string::npos);
  TEST_ASSERT_TRUE(t1.find(" actual_ms=") != std::string::npos);
}
