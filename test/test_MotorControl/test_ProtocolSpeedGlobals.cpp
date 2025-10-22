#include <unity.h>
#include <string>
#include "MotorControl/MotorCommandProcessor.h"
#include "MotorControl/MotionKinematics.h"
#include "MotorControl/MotorControlConstants.h"

void test_get_set_speed_ok() {
  MotorCommandProcessor proto;
  auto r1 = proto.processLine("GET SPEED", 0);
  TEST_ASSERT_TRUE(r1.rfind("CTRL:OK SPEED=", 0) == 0);
  auto r2 = proto.processLine("SET SPEED=4500", 0);
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", r2.c_str());
  auto r3 = proto.processLine("GET SPEED", 0);
  TEST_ASSERT_TRUE(r3.find("SPEED=4500") != std::string::npos);
}

void test_get_set_accel_ok() {
  MotorCommandProcessor proto;
  auto r1 = proto.processLine("GET ACCEL", 0);
  TEST_ASSERT_TRUE(r1.rfind("CTRL:OK ACCEL=", 0) == 0);
  auto r2 = proto.processLine("SET ACCEL=12345", 0);
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", r2.c_str());
  auto r3 = proto.processLine("GET ACCEL", 0);
  TEST_ASSERT_TRUE(r3.find("ACCEL=12345") != std::string::npos);
}

void test_set_speed_busy_reject() {
  // Start a long move, then attempt to change SPEED
  MotorCommandProcessor proto;
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", proto.processLine("SET SPEED=4000", 0).c_str());
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", proto.processLine("SET ACCEL=16000", 0).c_str());
  auto r1 = proto.processLine("MOVE:0,1000", 0);
  TEST_ASSERT_TRUE(r1.rfind("CTRL:OK", 0) == 0);
  auto r2 = proto.processLine("SET SPEED=5000", 10);
  TEST_ASSERT_EQUAL_STRING("CTRL:ERR E04 BUSY", r2.c_str());
}

static uint32_t parse_est_ms(const std::string& s) {
  auto pos = s.find("est_ms=");
  TEST_ASSERT_TRUE(pos != std::string::npos);
  pos += 7;
  size_t end = pos;
  while (end < s.size() && isdigit((unsigned char)s[end])) ++end;
  return (uint32_t)strtoul(s.substr(pos, end - pos).c_str(), nullptr, 10);
}

void test_home_uses_speed_accel_globals() {
  MotorCommandProcessor proto;
  // Set globals to known values
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", proto.processLine("SET SPEED=2000", 0).c_str());
  TEST_ASSERT_EQUAL_STRING("CTRL:OK", proto.processLine("SET ACCEL=8000", 0).c_str());
  // Issue HOME with no explicit speed/accel params
  auto r = proto.processLine("HOME:0", 0);
  TEST_ASSERT_TRUE(r.rfind("CTRL:OK", 0) == 0);
  uint32_t est = parse_est_ms(r);
  // Compute expected using defaults for overshoot/backoff/full_range
  using namespace MotorControlConstants;
  uint32_t expected = MotionKinematics::estimateHomeTimeMsWithFullRange(
    DEFAULT_OVERSHOOT, DEFAULT_BACKOFF,
    (MAX_POS_STEPS - MIN_POS_STEPS), 2000, 8000);
  TEST_ASSERT_EQUAL_UINT32(expected, est);
}

// No main(); tests are registered in test_main.cpp
