#include <unity.h>
#include <string>
#include "MotorControl/MotorCommandProcessor.h"
#include "MotorControl/MotionKinematics.h"
#include "MotorControl/MotorControlConstants.h"

void test_get_set_speed_ok() {
  MotorCommandProcessor proto;
  auto r1 = proto.processLine("GET SPEED", 0);
  TEST_ASSERT_TRUE(r1.rfind("CTRL:ACK ", 0) == 0);
  TEST_ASSERT_TRUE(r1.find(" SPEED=") != std::string::npos);
  auto r2 = proto.processLine("SET SPEED=4500", 0);
  TEST_ASSERT_TRUE(r2.rfind("CTRL:ACK", 0) == 0);
  auto r3 = proto.processLine("GET SPEED", 0);
  TEST_ASSERT_TRUE(r3.find("SPEED=4500") != std::string::npos);
}

void test_get_set_accel_ok() {
  MotorCommandProcessor proto;
  auto r1 = proto.processLine("GET ACCEL", 0);
  TEST_ASSERT_TRUE(r1.rfind("CTRL:ACK ", 0) == 0);
  TEST_ASSERT_TRUE(r1.find(" ACCEL=") != std::string::npos);
  auto r2 = proto.processLine("SET ACCEL=12345", 0);
  TEST_ASSERT_TRUE(r2.rfind("CTRL:ACK", 0) == 0);
  auto r3 = proto.processLine("GET ACCEL", 0);
  TEST_ASSERT_TRUE(r3.find("ACCEL=12345") != std::string::npos);
}

void test_get_set_decel_ok() {
  MotorCommandProcessor proto;
  auto r1 = proto.processLine("GET DECEL", 0);
  TEST_ASSERT_TRUE(r1.rfind("CTRL:ACK ", 0) == 0);
  TEST_ASSERT_TRUE(r1.find(" DECEL=") != std::string::npos);
  auto r2 = proto.processLine("SET DECEL=9000", 0);
  TEST_ASSERT_TRUE(r2.rfind("CTRL:ACK", 0) == 0);
  auto r3 = proto.processLine("GET DECEL", 0);
  TEST_ASSERT_TRUE(r3.find("DECEL=9000") != std::string::npos);
}

void test_set_decel_busy_reject() {
  MotorCommandProcessor proto;
  TEST_ASSERT_TRUE(proto.processLine("SET SPEED=4000", 0).rfind("CTRL:ACK", 0) == 0);
  TEST_ASSERT_TRUE(proto.processLine("SET ACCEL=16000", 0).rfind("CTRL:ACK", 0) == 0);
  auto r1 = proto.processLine("MOVE:0,1000", 0);
  TEST_ASSERT_TRUE(r1.rfind("CTRL:ACK", 0) == 0);
  auto r2 = proto.processLine("SET DECEL=5000", 10);
  TEST_ASSERT_TRUE(r2.rfind("CTRL:ERR ", 0) == 0);
  TEST_ASSERT_TRUE(r2.find(" E04 BUSY") != std::string::npos);
}

void test_set_speed_busy_reject() {
  // Start a long move, then attempt to change SPEED
  MotorCommandProcessor proto;
  TEST_ASSERT_TRUE(proto.processLine("SET SPEED=4000", 0).rfind("CTRL:ACK", 0) == 0);
  TEST_ASSERT_TRUE(proto.processLine("SET ACCEL=16000", 0).rfind("CTRL:ACK", 0) == 0);
  auto r1 = proto.processLine("MOVE:0,1000", 0);
  TEST_ASSERT_TRUE(r1.rfind("CTRL:ACK", 0) == 0);
  auto r2 = proto.processLine("SET SPEED=5000", 10);
  TEST_ASSERT_TRUE(r2.rfind("CTRL:ERR ", 0) == 0);
  TEST_ASSERT_TRUE(r2.find(" E04 BUSY") != std::string::npos);
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
  TEST_ASSERT_TRUE(proto.processLine("SET SPEED=2000", 0).rfind("CTRL:ACK", 0) == 0);
  TEST_ASSERT_TRUE(proto.processLine("SET ACCEL=8000", 0).rfind("CTRL:ACK", 0) == 0);
  // Issue HOME with no explicit speed/accel params
  auto r = proto.processLine("HOME:0", 0);
  TEST_ASSERT_TRUE(r.rfind("CTRL:ACK", 0) == 0);
  uint32_t est = parse_est_ms(r);
  // Compute expected using defaults for overshoot/backoff/full_range
  using namespace MotorControlConstants;
  uint32_t expected = MotionKinematics::estimateHomeTimeMsWithFullRange(
    DEFAULT_OVERSHOOT, DEFAULT_BACKOFF,
    (MAX_POS_STEPS - MIN_POS_STEPS), 2000, 8000);
  TEST_ASSERT_EQUAL_UINT32(expected, est);
}

// No main(); tests are registered in test_main.cpp
