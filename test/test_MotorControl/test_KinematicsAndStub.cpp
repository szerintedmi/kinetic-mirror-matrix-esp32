#ifdef ARDUINO
#include <Arduino.h>
#endif
#include <unity.h>
#include <string>
#include <cmath>
#include "MotorControl/MotionKinematics.h"
#include "MotorControl/MotorCommandProcessor.h"
#include "MotorControl/MotorControlConstants.h"

static std::string status_for(MotorCommandProcessor &p, uint32_t t_ms) {
  return p.processLine("STATUS", t_ms);
}

void test_estimator_trapezoidal_matches_simple_formula() {
  int d = 3000, v = 1000, a = 1000;
  uint32_t est = MotionKinematics::estimateMoveTimeMs(d, v, a);
  uint32_t expected = (uint32_t)((d * 1000 + v - 1) / v) + (uint32_t)((v * 1000 + a - 1) / a);
  TEST_ASSERT_EQUAL_UINT32(expected, est);
}

void test_estimator_triangular_above_naive_bound() {
  int d = 800, v = MotorControlConstants::DEFAULT_SPEED_SPS, a = MotorControlConstants::DEFAULT_ACCEL_SPS2;
  uint32_t est = MotionKinematics::estimateMoveTimeMs(d, v, a);
  uint32_t naive = (uint32_t)((d * 1000 + v - 1) / v);
  TEST_ASSERT_TRUE(est >= naive);
}

void test_stub_move_uses_estimator_duration() {
  MotorCommandProcessor p;
  int d = 500, v = 1200, a = 8000;
  uint32_t t = MotionKinematics::estimateMoveTimeMs(d, v, a);
  std::string cmd = std::string("MOVE:0,") + std::to_string(d) + "," + std::to_string(v) + "," + std::to_string(a);
  auto r1 = p.processLine(cmd, 0);
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", r1.c_str());
  auto st_pre = status_for(p, (t > 0) ? (t - 1) : 0);
  TEST_ASSERT_TRUE(st_pre.find("id=0") != std::string::npos);
  TEST_ASSERT_TRUE(st_pre.find(" moving=1") != std::string::npos);
  auto st_post = status_for(p, t);
  TEST_ASSERT_TRUE(st_post.find(" moving=0") != std::string::npos);
}

void test_stub_home_uses_estimator_duration() {
  MotorCommandProcessor p;
  uint32_t t = MotionKinematics::estimateHomeTimeMs(
      MotorControlConstants::DEFAULT_OVERSHOOT,
      MotorControlConstants::DEFAULT_BACKOFF,
      MotorControlConstants::DEFAULT_SPEED_SPS,
      MotorControlConstants::DEFAULT_ACCEL_SPS2);
  auto r1 = p.processLine("HOME:0", 0);
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", r1.c_str());
  auto st_pre = status_for(p, (t > 0) ? (t - 1) : 0);
  TEST_ASSERT_TRUE(st_pre.find(" moving=1") != std::string::npos);
  auto st_post = status_for(p, t + 1);
  TEST_ASSERT_TRUE(st_post.find(" moving=0") != std::string::npos);
  TEST_ASSERT_TRUE(st_post.find(" awake=0") != std::string::npos);
}

