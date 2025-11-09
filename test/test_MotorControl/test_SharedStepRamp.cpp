#include <unity.h>
#include "MotorControl/SharedStepTiming.h"

using namespace SharedStepTiming;

void test_stop_distance_basic() {
  // From 4000 sps with accel 16000 sps^2 should need ~500 steps to stop
  uint32_t d = stop_distance_steps(StopDistanceRequest(4000, 16000));
  TEST_ASSERT_EQUAL_UINT32(500, d);
}

void test_stop_distance_edges() {
  // Zero speed requires zero distance; small accel clamps correctly
  TEST_ASSERT_EQUAL_UINT32(0, stop_distance_steps(StopDistanceRequest(0, 16000)));
  TEST_ASSERT_EQUAL_UINT32(0, stop_distance_steps(StopDistanceRequest(0, 1)));
  // Very small acceleration yields larger stopping distances; check rounding up
  uint32_t d = stop_distance_steps(StopDistanceRequest(1000, 1)); // 1000^2 / 2 = 500000
  TEST_ASSERT_TRUE(d >= 500000);
}

// Registration in test_main.cpp
