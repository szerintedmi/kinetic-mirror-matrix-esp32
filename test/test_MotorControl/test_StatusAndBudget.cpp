#ifdef ARDUINO
#include <Arduino.h>
#endif
#include <unity.h>
#include <string>
#include <vector>
#include "MotorControl/MotorCommandProcessor.h"
#include "MotorControl/MotorControlConstants.h"
#include "MotorControl/MotionKinematics.h"

static std::vector<std::string> split_lines_sb(const std::string &s) {
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

static bool find_line_for_id(const std::vector<std::string>& lines, int id, std::string &out) {
  std::string prefix = std::string("id=") + std::to_string(id);
  for (const auto &L : lines) {
    if (L.rfind(prefix, 0) == 0) { out = L; return true; }
  }
  return false;
}

void test_status_includes_new_keys() {
  MotorCommandProcessor p;
  auto st = p.processLine("STATUS", 0);
  auto lines = split_lines_sb(st);
  // STATUS now starts with an ACK; find first data line
  TEST_ASSERT_TRUE((int)lines.size() >= 1);
  size_t idx = 0;
  while (idx < lines.size() && lines[idx].rfind("CTRL:", 0) == 0) ++idx;
  TEST_ASSERT_TRUE(idx < lines.size());
  const std::string &L0 = lines[idx];
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
  auto st1 = p.processLine("STATUS", 30000);
  auto lines1 = split_lines_sb(st1);
  std::string L0;
  TEST_ASSERT_TRUE(find_line_for_id(lines1, 0, L0));
  int exp_after_30 = (int)MotorControlConstants::MAX_RUNNING_TIME_S - 30;
  std::string exp1 = std::string("budget_s=") + std::to_string(exp_after_30);
  TEST_ASSERT_TRUE(L0.find(exp1) != std::string::npos);
  // Sleep and allow refill beyond cap; expect clamp at 90
  auto r2 = p.processLine("SLEEP:0", 30000);
  TEST_ASSERT_TRUE(r2.rfind("CTRL:DONE", 0) == 0);
  auto st2 = p.processLine("STATUS", 70000);
  auto lines2 = split_lines_sb(st2);
  TEST_ASSERT_TRUE(find_line_for_id(lines2, 0, L0));
  std::string exp_full = std::string("budget_s=") + std::to_string((int)MotorControlConstants::MAX_RUNNING_TIME_S);
  TEST_ASSERT_TRUE(L0.find(exp_full) != std::string::npos);
  TEST_ASSERT_TRUE(L0.find("ttfc_s=0") != std::string::npos);
}

void test_home_and_steps_since_home() {
  MotorCommandProcessor p;
  auto r1 = p.processLine("HOME:0", 0);
  TEST_ASSERT_TRUE(r1.rfind("CTRL:ACK", 0) == 0);
  uint32_t th = MotionKinematics::estimateHomeTimeMs(
      MotorControlConstants::DEFAULT_OVERSHOOT,
      MotorControlConstants::DEFAULT_BACKOFF,
      MotorControlConstants::DEFAULT_SPEED_SPS,
      MotorControlConstants::DEFAULT_ACCEL_SPS2);
  p.tick(th);
  auto st0 = p.processLine("STATUS", th);
  auto lines0 = split_lines_sb(st0);
  std::string L0;
  TEST_ASSERT_TRUE(find_line_for_id(lines0, 0, L0));
  TEST_ASSERT_TRUE(L0.find("homed=1") != std::string::npos);
  TEST_ASSERT_TRUE(L0.find("steps_since_home=0") != std::string::npos);

  // Use default/global speed; ensure command accepted without per-move params
  auto r2 = p.processLine("MOVE:0,10", th);
  TEST_ASSERT_TRUE(r2.rfind("CTRL:ACK", 0) == 0);
  p.tick(th + 200);
  auto st1 = p.processLine("STATUS", th + 200);
  auto lines1 = split_lines_sb(st1);
  TEST_ASSERT_TRUE(find_line_for_id(lines1, 0, L0));
  TEST_ASSERT_TRUE(L0.find("steps_since_home=10") != std::string::npos);
}

void test_budget_clamps_and_ttfc_non_negative() {
  MotorCommandProcessor p;
  auto r1 = p.processLine("WAKE:0", 0);
  TEST_ASSERT_TRUE(r1.rfind("CTRL:DONE", 0) == 0);
  // After 100s awake, budget should be negative (no clamp)
  auto st = p.processLine("STATUS", 100000);
  auto lines = split_lines_sb(st);
  std::string L0;
  TEST_ASSERT_TRUE(find_line_for_id(lines, 0, L0));
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
  auto st0 = p.processLine("STATUS", (uint32_t)(big_s * 1000));
  auto lines0 = split_lines_sb(st0);
  std::string L0;
  TEST_ASSERT_TRUE(find_line_for_id(lines0, 0, L0));
  // Expect ttfc is clamped to MAX_COOL_DOWN_TIME_S while budget is negative
  TEST_ASSERT_TRUE(L0.find("budget_s=-") != std::string::npos);
  TEST_ASSERT_TRUE(L0.find(std::string("ttfc_s=") + std::to_string((int)MotorControlConstants::MAX_COOL_DOWN_TIME_S)) != std::string::npos);

  // Go to sleep now and wait exactly MAX_COOL_DOWN_TIME_S seconds
  auto r2 = p.processLine("SLEEP:0", (uint32_t)(big_s * 1000));
  TEST_ASSERT_TRUE(r2.rfind("CTRL:DONE", 0) == 0);
  auto st1 = p.processLine("STATUS", (uint32_t)((big_s + MotorControlConstants::MAX_COOL_DOWN_TIME_S) * 1000));
  auto lines1 = split_lines_sb(st1);
  TEST_ASSERT_TRUE(find_line_for_id(lines1, 0, L0));
  TEST_ASSERT_TRUE(L0.find(std::string("budget_s=") + std::to_string((int)MotorControlConstants::MAX_RUNNING_TIME_S)) != std::string::npos);
  TEST_ASSERT_TRUE(L0.find("ttfc_s=0") != std::string::npos);
}

void test_homed_resets_on_reboot() {
  MotorCommandProcessor p;
  auto r1 = p.processLine("HOME:0", 0);
  TEST_ASSERT_TRUE(r1.rfind("CTRL:ACK", 0) == 0);
  uint32_t th = MotionKinematics::estimateHomeTimeMs(
      MotorControlConstants::DEFAULT_OVERSHOOT,
      MotorControlConstants::DEFAULT_BACKOFF,
      MotorControlConstants::DEFAULT_SPEED_SPS,
      MotorControlConstants::DEFAULT_ACCEL_SPS2);
  p.tick(th);
  auto st0 = p.processLine("STATUS", th);
  auto lines0 = split_lines_sb(st0);
  std::string L0;
  TEST_ASSERT_TRUE(find_line_for_id(lines0, 0, L0));
  TEST_ASSERT_TRUE(L0.find("homed=1") != std::string::npos);
  // Reboot simulation: re-instantiate the processor
  p = MotorCommandProcessor();
  auto st1 = p.processLine("STATUS", 0);
  auto lines1 = split_lines_sb(st1);
  TEST_ASSERT_TRUE(find_line_for_id(lines1, 0, L0));
  TEST_ASSERT_TRUE(L0.find("homed=0") != std::string::npos);
}

void test_steps_since_home_resets_after_second_home() {
  MotorCommandProcessor p;
  // First HOME completes -> steps_since_home should be 0
  auto r_home1 = p.processLine("HOME:0", 0);
  TEST_ASSERT_TRUE(r_home1.rfind("CTRL:ACK", 0) == 0);
  uint32_t th2 = MotionKinematics::estimateHomeTimeMs(
      MotorControlConstants::DEFAULT_OVERSHOOT,
      MotorControlConstants::DEFAULT_BACKOFF,
      MotorControlConstants::DEFAULT_SPEED_SPS,
      MotorControlConstants::DEFAULT_ACCEL_SPS2);
  p.tick(th2);
  auto st0 = p.processLine("STATUS", th2);
  auto lines0 = split_lines_sb(st0);
  std::string L0;
  TEST_ASSERT_TRUE(find_line_for_id(lines0, 0, L0));
  TEST_ASSERT_TRUE(L0.find("homed=1") != std::string::npos);
  TEST_ASSERT_TRUE(L0.find("steps_since_home=0") != std::string::npos);

  // Move to accumulate steps_since_home
  auto r_move = p.processLine("MOVE:0,100", th2);
  TEST_ASSERT_TRUE(r_move.rfind("CTRL:ACK", 0) == 0);
  p.tick(th2 + 1100);
  auto st1 = p.processLine("STATUS", th2 + 1100);
  auto lines1 = split_lines_sb(st1);
  TEST_ASSERT_TRUE(find_line_for_id(lines1, 0, L0));
  TEST_ASSERT_TRUE(L0.find("steps_since_home=100") != std::string::npos);

  // Second HOME resets steps_since_home to 0 after completion
  auto r_home2 = p.processLine("HOME:0", th2 + 1100);
  TEST_ASSERT_TRUE(r_home2.rfind("CTRL:ACK", 0) == 0);
  // Wait for another HOME completion
  uint32_t th3 = MotionKinematics::estimateHomeTimeMs(
      MotorControlConstants::DEFAULT_OVERSHOOT,
      MotorControlConstants::DEFAULT_BACKOFF,
      MotorControlConstants::DEFAULT_SPEED_SPS,
      MotorControlConstants::DEFAULT_ACCEL_SPS2);
  p.tick(th2 + 1100 + th3);
  auto st2 = p.processLine("STATUS", th2 + 1100 + th3);
  auto lines2 = split_lines_sb(st2);
  TEST_ASSERT_TRUE(find_line_for_id(lines2, 0, L0));
  TEST_ASSERT_TRUE(L0.find("steps_since_home=0") != std::string::npos);
}
