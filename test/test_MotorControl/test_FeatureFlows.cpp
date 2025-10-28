#ifdef ARDUINO
#include <Arduino.h>
#endif
#include <unity.h>
#include <string>
#include "MotorControl/MotorCommandProcessor.h"

static bool starts_with(const std::string &s, const char *p)
{
  return s.rfind(p, 0) == 0;
}

void test_flow_set_off_move_exceeds_max_warn_ok()
{
  MotorCommandProcessor p;
  TEST_ASSERT_TRUE(p.processLine("SET THERMAL_LIMITING=OFF", 0).rfind("CTRL:ACK", 0) == 0);
  // This request exceeds MAX_RUNNING_TIME_S by design (speed=1)
  TEST_ASSERT_TRUE(p.processLine("SET SPEED=1", 0).rfind("CTRL:ACK", 0) == 0);
  TEST_ASSERT_TRUE(p.processLine("SET ACCEL=1000", 0).rfind("CTRL:ACK", 0) == 0);
  std::string r = p.processLine("MOVE:0,1200", 0);
  // WARN line then final OK with estimate
  size_t nl = r.find('\n');
  std::string first = (nl == std::string::npos) ? r : r.substr(0, nl);
  std::string last = (nl == std::string::npos) ? r : r.substr(r.find_last_of('\n') + 1);
  TEST_ASSERT_TRUE(starts_with(first, "CTRL:WARN "));
  TEST_ASSERT_TRUE(first.find(" THERMAL_REQ_GT_MAX") != std::string::npos);
  TEST_ASSERT_TRUE(starts_with(last, "CTRL:ACK CID="));
  TEST_ASSERT_TRUE(last.find(" est_ms=") != std::string::npos);
}

void test_flow_set_on_move_exceeds_max_err()
{
  MotorCommandProcessor p;
  TEST_ASSERT_TRUE(p.processLine("SET THERMAL_LIMITING=ON", 0).rfind("CTRL:ACK", 0) == 0);
  TEST_ASSERT_TRUE(p.processLine("SET SPEED=1", 0).rfind("CTRL:ACK", 0) == 0);
  TEST_ASSERT_TRUE(p.processLine("SET ACCEL=1000", 0).rfind("CTRL:ACK", 0) == 0);
  std::string r = p.processLine("MOVE:0,1200", 0);
  TEST_ASSERT_TRUE(starts_with(r, "CTRL:ERR "));
  TEST_ASSERT_TRUE(r.find(" E10 THERMAL_REQ_GT_MAX") != std::string::npos);
}
