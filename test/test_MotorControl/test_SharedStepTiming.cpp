#include <unity.h>
#include "MotorControl/SharedStepTiming.h"

void test_shared_timing_period_basic() {
  using namespace SharedStepTiming;
  TEST_ASSERT_EQUAL_UINT32(250, step_period_us(4000));
  TEST_ASSERT_EQUAL_UINT32(100, step_period_us(10000));
}

void test_shared_timing_align_to_next_edge() {
  using namespace SharedStepTiming;
  const uint32_t p = 250; // 4 k sps
  TEST_ASSERT_EQUAL_UINT64(1000, align_to_next_edge_us(PeriodAlignmentRequest(1000, p)));
  TEST_ASSERT_EQUAL_UINT64(1250, align_to_next_edge_us(PeriodAlignmentRequest(1000 + p, p)));
  TEST_ASSERT_EQUAL_UINT64(1250, align_to_next_edge_us(PeriodAlignmentRequest(1001, p)));
  TEST_ASSERT_EQUAL_UINT64(1250, align_to_next_edge_us(PeriodAlignmentRequest(1249, p)));
}

void test_shared_timing_guard_fits() {
  using namespace SharedStepTiming;
  const uint32_t p = 250; // 4 k sps
  TEST_ASSERT_TRUE(guard_fits_between_edges(p, GuardWindow(2, 2)));
  TEST_ASSERT_TRUE(guard_fits_between_edges(p, GuardWindow(5, 5)));
  TEST_ASSERT_FALSE(guard_fits_between_edges(p, GuardWindow(200, 60)));
}
