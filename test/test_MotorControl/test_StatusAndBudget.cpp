#ifdef ARDUINO
#include <Arduino.h>
#endif
#include "MotorControl/MotionKinematics.h"
#include "MotorControl/MotorCommandProcessor.h"
#include "MotorControl/MotorControlConstants.h"
#include "test_common/TestHelpers.h"

#include <string>
#include <unity.h>
#include <vector>

using test_helpers::FindStatusLineForId;
using test_helpers::SkipCtrlLines;
using test_helpers::SplitLines;

void test_status_includes_new_keys() {
  MotorCommandProcessor p;
  auto st = p.processLine("STATUS", 0);
  auto lines = SplitLines(st);
  // STATUS now starts with an ACK; find first data line
  TEST_ASSERT_TRUE((int)lines.size() >= 1);
  size_t idx = SkipCtrlLines(lines);
  TEST_ASSERT_TRUE(idx < lines.size());
  const std::string& L0 = lines[idx];
  TEST_ASSERT_TRUE(L0.find("homed=") != std::string::npos);
  TEST_ASSERT_TRUE(L0.find("steps_since_home=") != std::string::npos);
  TEST_ASSERT_TRUE(L0.find("budget_s=") != std::string::npos);
  TEST_ASSERT_TRUE(L0.find("ttfc_s=") != std::string::npos);
}

void test_budget_spend_and_refill_clamp() {
  MotorCommandProcessor p;
  auto r1 = p.processLine("WAKE:0", 0);
  TEST_ASSERT_TRUE(r1.rfind("CTRL:DONE", 0) == 0);
  // After 30s awake, expect ~60s remaining
  p.tick(30000);  // Update budget state
  auto st1 = p.processLine("STATUS", 30000);
  auto lines1 = SplitLines(st1);
  std::string L0 = FindStatusLineForId(lines1, 0);
  TEST_ASSERT_FALSE(L0.empty());
  int exp_after_30 = (int)MotorControlConstants::MAX_RUNNING_TIME_S - 30;
  std::string exp1 = std::string("budget_s=") + std::to_string(exp_after_30);
  TEST_ASSERT_TRUE(L0.find(exp1) != std::string::npos);
  // Sleep and allow refill beyond cap; expect clamp at 90
  auto r2 = p.processLine("SLEEP:0", 30000);
  TEST_ASSERT_TRUE(r2.rfind("CTRL:DONE", 0) == 0);
  p.tick(70000);  // Update budget state
  auto st2 = p.processLine("STATUS", 70000);
  auto lines2 = SplitLines(st2);
  L0 = FindStatusLineForId(lines2, 0);
  TEST_ASSERT_FALSE(L0.empty());
  std::string exp_full =
      std::string("budget_s=") + std::to_string((int)MotorControlConstants::MAX_RUNNING_TIME_S);
  TEST_ASSERT_TRUE(L0.find(exp_full) != std::string::npos);
  TEST_ASSERT_TRUE(L0.find("ttfc_s=0") != std::string::npos);
}

void test_home_and_steps_since_home() {
  MotorCommandProcessor p;
  auto r1 = p.processLine("HOME:0", 0);
  TEST_ASSERT_TRUE(r1.rfind("CTRL:ACK", 0) == 0);
  uint32_t th = MotionKinematics::estimateHomeTimeMs(MotorControlConstants::DEFAULT_OVERSHOOT,
                                                     MotorControlConstants::DEFAULT_BACKOFF,
                                                     MotorControlConstants::DEFAULT_SPEED_SPS,
                                                     MotorControlConstants::DEFAULT_ACCEL_SPS2);
  p.tick(th);
  auto st0 = p.processLine("STATUS", th);
  auto lines0 = SplitLines(st0);
  std::string L0 = FindStatusLineForId(lines0, 0);
  TEST_ASSERT_FALSE(L0.empty());
  TEST_ASSERT_TRUE(L0.find("homed=1") != std::string::npos);
  TEST_ASSERT_TRUE(L0.find("steps_since_home=0") != std::string::npos);

  // Use default/global speed; ensure command accepted without per-move params
  auto r2 = p.processLine("MOVE:0,10", th);
  TEST_ASSERT_TRUE(r2.rfind("CTRL:ACK", 0) == 0);
  p.tick(th + 200);
  auto st1 = p.processLine("STATUS", th + 200);
  auto lines1 = SplitLines(st1);
  L0 = FindStatusLineForId(lines1, 0);
  TEST_ASSERT_FALSE(L0.empty());
  TEST_ASSERT_TRUE(L0.find("steps_since_home=10") != std::string::npos);
}

void test_budget_clamps_and_ttfc_non_negative() {
  MotorCommandProcessor p;
  auto r1 = p.processLine("WAKE:0", 0);
  TEST_ASSERT_TRUE(r1.rfind("CTRL:DONE", 0) == 0);
  // After 100s awake, budget should be negative (no clamp)
  p.tick(100000);  // Update budget state
  auto st = p.processLine("STATUS", 100000);
  auto lines = SplitLines(st);
  std::string L0 = FindStatusLineForId(lines, 0);
  TEST_ASSERT_FALSE(L0.empty());
  // Expect a negative budget_s (leading '-')
  TEST_ASSERT_TRUE(L0.find("budget_s=-") != std::string::npos);
  // ttfc should not be negative
  TEST_ASSERT_TRUE(L0.find("ttfc_s=-") == std::string::npos);
}

void test_ttfc_clamp_and_recovery() {
  MotorCommandProcessor p;
  // Run long enough awake to exceed any reasonable deficit
  auto r1 = p.processLine("WAKE:0", 0);
  TEST_ASSERT_TRUE(r1.rfind("CTRL:DONE", 0) == 0);
  const int64_t big_s = (int64_t)MotorControlConstants::MAX_COOL_DOWN_TIME_S * 5 + 123;
  uint32_t big_ms = (uint32_t)(big_s * 1000);
  p.tick(big_ms);  // Update budget state
  auto st0 = p.processLine("STATUS", big_ms);
  auto lines0 = SplitLines(st0);
  std::string L0 = FindStatusLineForId(lines0, 0);
  TEST_ASSERT_FALSE(L0.empty());
  // Expect ttfc is clamped to MAX_COOL_DOWN_TIME_S while budget is negative
  TEST_ASSERT_TRUE(L0.find("budget_s=-") != std::string::npos);
  TEST_ASSERT_TRUE(L0.find(std::string("ttfc_s=") +
                           std::to_string((int)MotorControlConstants::MAX_COOL_DOWN_TIME_S)) !=
                   std::string::npos);

  // Go to sleep now and wait exactly MAX_COOL_DOWN_TIME_S seconds
  auto r2 = p.processLine("SLEEP:0", big_ms);
  TEST_ASSERT_TRUE(r2.rfind("CTRL:DONE", 0) == 0);
  uint32_t recovery_ms = (uint32_t)((big_s + MotorControlConstants::MAX_COOL_DOWN_TIME_S) * 1000);
  p.tick(recovery_ms);  // Update budget state
  auto st1 = p.processLine("STATUS", recovery_ms);
  auto lines1 = SplitLines(st1);
  L0 = FindStatusLineForId(lines1, 0);
  TEST_ASSERT_FALSE(L0.empty());
  TEST_ASSERT_TRUE(L0.find(std::string("budget_s=") +
                           std::to_string((int)MotorControlConstants::MAX_RUNNING_TIME_S)) !=
                   std::string::npos);
  TEST_ASSERT_TRUE(L0.find("ttfc_s=0") != std::string::npos);
}

void test_homed_resets_on_reboot() {
  MotorCommandProcessor p;
  auto r1 = p.processLine("HOME:0", 0);
  TEST_ASSERT_TRUE(r1.rfind("CTRL:ACK", 0) == 0);
  uint32_t th = MotionKinematics::estimateHomeTimeMs(MotorControlConstants::DEFAULT_OVERSHOOT,
                                                     MotorControlConstants::DEFAULT_BACKOFF,
                                                     MotorControlConstants::DEFAULT_SPEED_SPS,
                                                     MotorControlConstants::DEFAULT_ACCEL_SPS2);
  p.tick(th);
  auto st0 = p.processLine("STATUS", th);
  auto lines0 = SplitLines(st0);
  std::string L0 = FindStatusLineForId(lines0, 0);
  TEST_ASSERT_FALSE(L0.empty());
  TEST_ASSERT_TRUE(L0.find("homed=1") != std::string::npos);
  // Reboot simulation: re-instantiate the processor
  p = MotorCommandProcessor();
  auto st1 = p.processLine("STATUS", 0);
  auto lines1 = SplitLines(st1);
  L0 = FindStatusLineForId(lines1, 0);
  TEST_ASSERT_FALSE(L0.empty());
  TEST_ASSERT_TRUE(L0.find("homed=0") != std::string::npos);
}

void test_steps_since_home_resets_after_second_home() {
  MotorCommandProcessor p;
  // First HOME completes -> steps_since_home should be 0
  auto r_home1 = p.processLine("HOME:0", 0);
  TEST_ASSERT_TRUE(r_home1.rfind("CTRL:ACK", 0) == 0);
  uint32_t th2 = MotionKinematics::estimateHomeTimeMs(MotorControlConstants::DEFAULT_OVERSHOOT,
                                                      MotorControlConstants::DEFAULT_BACKOFF,
                                                      MotorControlConstants::DEFAULT_SPEED_SPS,
                                                      MotorControlConstants::DEFAULT_ACCEL_SPS2);
  p.tick(th2);
  auto st0 = p.processLine("STATUS", th2);
  auto lines0 = SplitLines(st0);
  std::string L0 = FindStatusLineForId(lines0, 0);
  TEST_ASSERT_FALSE(L0.empty());
  TEST_ASSERT_TRUE(L0.find("homed=1") != std::string::npos);
  TEST_ASSERT_TRUE(L0.find("steps_since_home=0") != std::string::npos);

  // Move to accumulate steps_since_home
  auto r_move = p.processLine("MOVE:0,100", th2);
  TEST_ASSERT_TRUE(r_move.rfind("CTRL:ACK", 0) == 0);
  p.tick(th2 + 1100);
  auto st1 = p.processLine("STATUS", th2 + 1100);
  auto lines1 = SplitLines(st1);
  L0 = FindStatusLineForId(lines1, 0);
  TEST_ASSERT_FALSE(L0.empty());
  TEST_ASSERT_TRUE(L0.find("steps_since_home=100") != std::string::npos);

  // Second HOME resets steps_since_home to 0 after completion
  auto r_home2 = p.processLine("HOME:0", th2 + 1100);
  TEST_ASSERT_TRUE(r_home2.rfind("CTRL:ACK", 0) == 0);
  // Wait for another HOME completion
  uint32_t th3 = MotionKinematics::estimateHomeTimeMs(MotorControlConstants::DEFAULT_OVERSHOOT,
                                                      MotorControlConstants::DEFAULT_BACKOFF,
                                                      MotorControlConstants::DEFAULT_SPEED_SPS,
                                                      MotorControlConstants::DEFAULT_ACCEL_SPS2);
  p.tick(th2 + 1100 + th3);
  auto st2 = p.processLine("STATUS", th2 + 1100 + th3);
  auto lines2 = SplitLines(st2);
  L0 = FindStatusLineForId(lines2, 0);
  TEST_ASSERT_FALSE(L0.empty());
  TEST_ASSERT_TRUE(L0.find("steps_since_home=0") != std::string::npos);
}

// Test that multiple small tick intervals accumulate correctly (sub-second granularity)
void test_budget_subsecond_accumulation() {
  MotorCommandProcessor p;
  auto r1 = p.processLine("WAKE:0", 0);
  TEST_ASSERT_TRUE(r1.rfind("CTRL:DONE", 0) == 0);

  // Tick 10 times at 100ms intervals (= 1 second total)
  uint32_t time_ms = 0;
  for (int i = 0; i < 10; ++i) {
    time_ms += 100;
    p.tick(time_ms);
  }

  // After 1 second awake, budget should decrease by 1 second
  auto st = p.processLine("STATUS", time_ms);
  auto lines = SplitLines(st);
  std::string L0 = FindStatusLineForId(lines, 0);
  TEST_ASSERT_FALSE(L0.empty());
  int expected = (int)MotorControlConstants::MAX_RUNNING_TIME_S - 1;
  std::string expected_str = std::string("budget_s=") + std::to_string(expected);
  TEST_ASSERT_TRUE(L0.find(expected_str) != std::string::npos);
}

// Test that budget updates at 100ms granularity, not 1000ms
void test_budget_100ms_granularity() {
  MotorCommandProcessor p;
  auto r1 = p.processLine("WAKE:0", 0);
  TEST_ASSERT_TRUE(r1.rfind("CTRL:DONE", 0) == 0);

  // Tick at 500ms - should show partial budget decrease
  p.tick(500);
  auto st1 = p.processLine("STATUS", 500);
  auto lines1 = SplitLines(st1);
  std::string L0 = FindStatusLineForId(lines1, 0);
  TEST_ASSERT_FALSE(L0.empty());
  // After 500ms awake, budget should be 89 (decreased by 0.5s worth of tenths)
  // The budget decreases by 1 tenth per 100ms when awake (10 tenths/sec)
  // 500ms = 5 tenths = 0.5 seconds decrease, so 90 - 0 = still 89 in seconds
  // (90s * 10 = 900 tenths - 5 tenths = 895 tenths = 89.5s -> shows as 89)
  TEST_ASSERT_TRUE(L0.find("budget_s=89") != std::string::npos);
}

// Test that many small intervals don't cause drift compared to one large interval
void test_budget_no_drift_over_small_intervals() {
  // Test with many small ticks
  MotorCommandProcessor p1;
  p1.processLine("WAKE:0", 0);
  uint32_t time_ms = 0;
  for (int i = 0; i < 100; ++i) {
    time_ms += 10;  // 10ms intervals
    p1.tick(time_ms);
  }
  auto st1 = p1.processLine("STATUS", time_ms);
  auto lines1 = SplitLines(st1);
  std::string L1 = FindStatusLineForId(lines1, 0);
  TEST_ASSERT_FALSE(L1.empty());

  // Test with one large tick
  MotorCommandProcessor p2;
  p2.processLine("WAKE:0", 0);
  p2.tick(1000);  // Single 1000ms tick
  auto st2 = p2.processLine("STATUS", 1000);
  auto lines2 = SplitLines(st2);
  std::string L2 = FindStatusLineForId(lines2, 0);
  TEST_ASSERT_FALSE(L2.empty());

  // Both should show same budget (89s after 1 second awake)
  TEST_ASSERT_TRUE(L1.find("budget_s=89") != std::string::npos);
  TEST_ASSERT_TRUE(L2.find("budget_s=89") != std::string::npos);
}
