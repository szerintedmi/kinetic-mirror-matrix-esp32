#include "MotorControl/MotionKinematics.h"
#include "MotorControl/MotorCommandProcessor.h"
#include "MotorControl/MotorControlConstants.h"
#include "test_common/TestHelpers.h"

#include <string>
#include <unity.h>

using test_helpers::ParseEstMs;

void test_get_microstep_default_1_32() {
  MotorCommandProcessor proto;
  auto r = proto.processLine("GET MICROSTEP", 0);
  TEST_ASSERT_TRUE(r.rfind("CTRL:DONE", 0) == 0);
  TEST_ASSERT_TRUE(r.find("MICROSTEP=1/32") != std::string::npos);
}

void test_set_microstep_valid_modes() {
  MotorCommandProcessor proto;
  // All motors must be asleep to change microstep mode
  const char* modes[] = {"FULL", "HALF", "1/4", "1/8", "1/16", "1/32"};
  const int multipliers[] = {1, 2, 4, 8, 16, 32};

  for (size_t i = 0; i < 6; ++i) {
    std::string cmd = std::string("SET MICROSTEP=") + modes[i];
    auto r = proto.processLine(cmd, 0);
    TEST_ASSERT_TRUE_MESSAGE(r.rfind("CTRL:DONE", 0) == 0, cmd.c_str());
    TEST_ASSERT_TRUE(r.find(std::string("multiplier=") + std::to_string(multipliers[i])) !=
                     std::string::npos);
  }
}

void test_set_microstep_requires_all_asleep() {
  MotorCommandProcessor proto;
  // Wake motor 0
  proto.processLine("WAKE:0", 0);
  // Attempt to change microstep mode - should fail
  auto r = proto.processLine("SET MICROSTEP=1/4", 0);
  TEST_ASSERT_TRUE(r.rfind("CTRL:ERR", 0) == 0);
  TEST_ASSERT_TRUE(r.find("E04 BUSY") != std::string::npos);
  // Sleep motor 0
  proto.processLine("SLEEP:0", 0);
  // Now should succeed
  r = proto.processLine("SET MICROSTEP=1/4", 0);
  TEST_ASSERT_TRUE(r.rfind("CTRL:DONE", 0) == 0);
}

void test_move_scales_by_multiplier() {
  MotorCommandProcessor proto;
  // Set to 1/4 mode (multiplier = 4)
  proto.processLine("SET MICROSTEP=1/4", 0);
  // Move motor 0 to user position 100 (internally 400 hw steps)
  auto r = proto.processLine("MOVE:0,100", 0);
  TEST_ASSERT_TRUE(r.rfind("CTRL:ACK", 0) == 0);
  // Complete the move
  proto.processLine("", 10000);
  // STATUS reports user-space position (not hw steps)
  auto status = proto.processLine("STATUS", 10001);
  // Position should match user input: 100 (not 400 hw steps)
  TEST_ASSERT_TRUE(status.find("pos=100") != std::string::npos);
}

void test_timing_estimate_scales_with_microstep() {
  // Test that timing estimates are consistent between modes
  MotorCommandProcessor proto;
  proto.processLine("SET SPEED=1000", 0);
  proto.processLine("SET ACCEL=4000", 0);

  // Move in FULL mode
  auto r1 = proto.processLine("MOVE:0,1000", 0);
  TEST_ASSERT_TRUE(r1.rfind("CTRL:ACK", 0) == 0);
  uint32_t est_full = ParseEstMs(r1);
  proto.processLine("", 20000);  // Complete move

  // Switch to 1/4 mode and move same user distance
  proto.processLine("SET MICROSTEP=1/4", 20001);
  // Position was rescaled from 1000 to 4000 hw steps
  // Move back to 0 (same user distance of 1000)
  auto r2 = proto.processLine("MOVE:0,0", 20002);
  TEST_ASSERT_TRUE(r2.rfind("CTRL:ACK", 0) == 0);
  uint32_t est_quarter = ParseEstMs(r2);

  // Both estimates should be the same (same physical distance, same physical speed)
  // Allow small tolerance for rounding
  TEST_ASSERT_INT_WITHIN(100, est_full, est_quarter);
}

void test_position_rescaled_on_mode_change() {
  MotorCommandProcessor proto;
  // Move to position 1000 in FULL mode
  proto.processLine("MOVE:0,1000", 0);
  proto.processLine("", 10000);  // Complete move

  // Verify user position is 1000
  auto status1 = proto.processLine("STATUS", 10001);
  TEST_ASSERT_TRUE(status1.find("pos=1000") != std::string::npos);

  // Switch to 1/4 mode (multiplier 4)
  // Internal hw position rescales: 1000 * 4 = 4000 hw steps
  // But user-space position stays the same: 4000 / 4 = 1000
  proto.processLine("SET MICROSTEP=1/4", 10002);
  auto status2 = proto.processLine("STATUS", 10003);
  // User position should still be 1000 (unchanged from user perspective)
  TEST_ASSERT_TRUE(status2.find("pos=1000") != std::string::npos);

  // Move to user position 1000 - should not move since already there
  auto r = proto.processLine("MOVE:0,1000", 10004);
  TEST_ASSERT_TRUE(r.rfind("CTRL:ACK", 0) == 0);
  uint32_t est = ParseEstMs(r);
  // Should be 0 or near 0 since no movement needed
  TEST_ASSERT_EQUAL_UINT32(0, est);
}

void test_status_shows_microstep() {
  MotorCommandProcessor proto;
  auto status = proto.processLine("STATUS", 0);
  // STATUS outputs lowercase field names: microstep=1/32 microstep_mult=32 (default)
  TEST_ASSERT_TRUE(status.find("microstep=1/32") != std::string::npos);
  TEST_ASSERT_TRUE(status.find("microstep_mult=32") != std::string::npos);
  // STATUS also includes thermal_limiting and max_budget_s
  TEST_ASSERT_TRUE(status.find("thermal_limiting=ON") != std::string::npos);
  TEST_ASSERT_TRUE(status.find("max_budget_s=") != std::string::npos);

  proto.processLine("SET MICROSTEP=1/32", 0);
  status = proto.processLine("STATUS", 1);
  TEST_ASSERT_TRUE(status.find("microstep=1/32") != std::string::npos);
  TEST_ASSERT_TRUE(status.find("microstep_mult=32") != std::string::npos);

  // Verify thermal state can be changed and appears in STATUS
  proto.processLine("SET THERMAL_LIMITING=OFF", 0);
  status = proto.processLine("STATUS", 2);
  TEST_ASSERT_TRUE(status.find("thermal_limiting=OFF") != std::string::npos);
}

void test_get_all_includes_microstep() {
  MotorCommandProcessor proto;
  auto r = proto.processLine("GET ALL", 0);
  TEST_ASSERT_TRUE(r.rfind("CTRL:DONE", 0) == 0);
  TEST_ASSERT_TRUE(r.find("MICROSTEP=1/32") != std::string::npos);

  proto.processLine("SET MICROSTEP=HALF", 0);
  r = proto.processLine("GET ALL", 1);
  TEST_ASSERT_TRUE(r.find("MICROSTEP=HALF") != std::string::npos);
}

void test_status_reports_user_space_position() {
  MotorCommandProcessor proto;
  // Set 1/8 microstepping (multiplier = 8)
  proto.processLine("SET MICROSTEP=1/8", 0);
  proto.processLine("WAKE:0", 1);

  // Home first to reset position and enable steps_since_home tracking
  proto.processLine("HOME:0", 2);
  proto.processLine("", 5000);  // Complete home

  // Move to user position 125 (internally 1000 hw steps)
  auto r = proto.processLine("MOVE:0,125", 5001);
  TEST_ASSERT_TRUE(r.rfind("CTRL:ACK", 0) == 0);

  // Complete the move
  proto.processLine("", 15000);

  // STATUS should report user position 125 (not hw 1000)
  auto status = proto.processLine("STATUS:0", 15001);
  TEST_ASSERT_TRUE(status.find("pos=125") != std::string::npos);
  // steps_since_home should also be in user-space (125 steps moved since home)
  TEST_ASSERT_TRUE(status.find("steps_since_home=125") != std::string::npos);
}

// No main(); tests are registered in test_main.cpp
