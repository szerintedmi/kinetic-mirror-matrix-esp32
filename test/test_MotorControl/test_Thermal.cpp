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

