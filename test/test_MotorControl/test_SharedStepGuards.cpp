#include <unity.h>
#include "MotorControl/SharedStepGuards.h"

void test_dir_guard_constants_reasonable() {
  using namespace SharedStepTiming;
  // At 10 ksteps/s (100 us period), our default guards should fit easily
  TEST_ASSERT_TRUE(guard_fits_between_edges(100, DIR_GUARD_US_PRE, DIR_GUARD_US_POST));
}

void test_compute_flip_window_mid_gap() {
  using namespace SharedStepGuards;
  const uint32_t period = 100; // us
  DirFlipWindow w{};
  // Pick an arbitrary now that is not aligned
  bool ok = compute_flip_window(1234, period, w);
  TEST_ASSERT_TRUE(ok);
  // The window should lie strictly inside (edge, edge+period)
  const uint64_t edge_next = SharedStepTiming::align_to_next_edge_us(1234, period);
  TEST_ASSERT_TRUE(w.t_sleep_low > edge_next);
  TEST_ASSERT_TRUE(w.t_sleep_high < edge_next + period);
  TEST_ASSERT_TRUE(w.t_sleep_low < w.t_dir_flip);
  TEST_ASSERT_TRUE(w.t_dir_flip < w.t_sleep_high);
}

void test_compute_flip_window_aligns_from_edge() {
  using namespace SharedStepGuards;
  const uint32_t period = 250; // 4 k sps
  DirFlipWindow w{};
  // When now is exactly at an edge, mid-gap scheduling still valid
  bool ok = compute_flip_window(1000, period, w);
  TEST_ASSERT_TRUE(ok);
  const uint64_t edge_next = 1000; // align_to_next_edge_us returns now
  TEST_ASSERT_TRUE(w.t_sleep_low > edge_next);
  TEST_ASSERT_TRUE(w.t_sleep_high < edge_next + period);
}

