#include <unity.h>
#include "MotorControl/SharedStepTiming.h"
#include "MotorControl/SharedStepGuards.h"

using namespace SharedStepTiming;
using namespace SharedStepGuards;

void test_shared_timing_period_zero() {
  // Explicit zero speed returns 0 period to indicate generator should stop
  TEST_ASSERT_EQUAL_UINT32(0u, step_period_us(0));
}

void test_guard_fit_thresholds() {
  const uint32_t pre = DIR_GUARD_US_PRE;
  const uint32_t post = DIR_GUARD_US_POST;
  // Exactly total+2 should fail (strictly less is required)
  TEST_ASSERT_FALSE(guard_fits_between_edges(pre + post + 2, pre, post));
  // Adding one more microsecond should pass
  TEST_ASSERT_TRUE(guard_fits_between_edges(pre + post + 3, pre, post));
}

void test_compute_flip_window_too_tight_period() {
  // With very small period, guard window cannot fit → no schedule
  const uint32_t tight_period = DIR_GUARD_US_PRE + DIR_GUARD_US_POST + 2; // just below threshold
  DirFlipWindow w{};
  bool ok = compute_flip_window(12345, tight_period, w);
  TEST_ASSERT_FALSE(ok);
}

void test_compute_flip_window_spacing_matches_constants() {
  const uint32_t period = 250; // 4k sps
  DirFlipWindow w{};
  bool ok = compute_flip_window(4321, period, w);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL_UINT64((uint64_t)DIR_GUARD_US_PRE, w.t_dir_flip - w.t_sleep_low);
  TEST_ASSERT_EQUAL_UINT64((uint64_t)DIR_GUARD_US_POST, w.t_sleep_high - w.t_dir_flip);
}

void test_align_next_edge_near_boundary() {
  const uint32_t p = 250;
  // One microsecond before the edge should align to the next edge
  TEST_ASSERT_EQUAL_UINT64(1250, align_to_next_edge_us(1249, p));
}

void test_stop_distance_round_up_small_values() {
  // v=1 sps, a=2 sps^2 => v^2/(2a) = 0.25 → ceil to 1 step
  TEST_ASSERT_EQUAL_UINT32(1u, stop_distance_steps(1, 2));
}

