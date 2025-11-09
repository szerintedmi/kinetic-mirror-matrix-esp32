#include "MotorControl/SharedStepGuards.h"

#include <unity.h>

using namespace SharedStepGuards;

void test_dir_guard_constants_reasonable() {
  // At 10 ksteps/s (100 us period), our default guards should fit easily
  TEST_ASSERT_TRUE(SharedStepTiming::guard_fits_between_edges(
      100, SharedStepTiming::GuardWindow(kDirGuardPreUs, kDirGuardPostUs)));
}

void test_compute_flip_window_mid_gap() {
  using namespace SharedStepGuards;
  const uint32_t period = 100;  // us
  DirFlipWindow w{};
  // Pick an arbitrary now that is not aligned
  bool ok = compute_flip_window(FlipWindowRequest(1234ULL, period), w);
  TEST_ASSERT_TRUE(ok);
  // The window should lie strictly inside (edge, edge+period)
  const uint64_t edge_next = SharedStepTiming::align_to_next_edge_us(
      SharedStepTiming::PeriodAlignmentRequest(1234ULL, period));
  TEST_ASSERT_TRUE(w.t_sleep_low > edge_next);
  TEST_ASSERT_TRUE(w.t_sleep_high < edge_next + period);
  TEST_ASSERT_TRUE(w.t_sleep_low < w.t_dir_flip);
  TEST_ASSERT_TRUE(w.t_dir_flip < w.t_sleep_high);
}

void test_compute_flip_window_aligns_from_edge() {
  using namespace SharedStepGuards;
  const uint32_t period = 250;  // 4 k sps
  DirFlipWindow w{};
  // When now is exactly at an edge, mid-gap scheduling still valid
  bool ok = compute_flip_window(FlipWindowRequest(1000ULL, period), w);
  TEST_ASSERT_TRUE(ok);
  const uint64_t edge_next = 1000;  // align_to_next_edge_us returns now
  TEST_ASSERT_TRUE(w.t_sleep_low > edge_next);
  TEST_ASSERT_TRUE(w.t_sleep_high < edge_next + period);
}
