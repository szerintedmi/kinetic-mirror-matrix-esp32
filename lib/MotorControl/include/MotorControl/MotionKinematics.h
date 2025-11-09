#pragma once
#include <stdint.h>

namespace MotionKinematics {

// Estimate time in milliseconds for a point-to-point move over
// distance_steps at capped speed (steps/s) with acceleration (steps/s^2).
// Uses triangular/trapezoidal profile with integer math and ceil rounding.
uint32_t estimateMoveTimeMs(int64_t distance_steps, int64_t speed_sps, int64_t accel_sps2);
// Asymmetric variant: separate acceleration up and deceleration down. If decel_down_sps2==0,
// model stops at target with no ramp-down time (SLEEP gating).
uint32_t estimateMoveTimeMsAsym(int64_t distance_steps,
                                int64_t speed_sps,
                                int64_t accel_up_sps2,
                                int64_t decel_down_sps2);

// Estimate HOME total time as two legs (overshoot + backoff), each
// treated as an independent move. Returns total milliseconds.
uint32_t estimateHomeTimeMs(int64_t overshoot_steps,
                            int64_t backoff_steps,
                            int64_t speed_sps,
                            int64_t accel_sps2);

// Estimate HOME time including hardware sequence legs:
//  - Leg1: negative run of (full_range + overshoot)
//  - Leg2: positive backoff of backoff
//  - Leg3: positive center to midpoint of (full_range/2)
// Returns total milliseconds.
uint32_t estimateHomeTimeMsWithFullRange(int64_t overshoot_steps,
                                         int64_t backoff_steps,
                                         int64_t full_range_steps,
                                         int64_t speed_sps,
                                         int64_t accel_sps2);
uint32_t estimateHomeTimeMsWithFullRangeAsym(int64_t overshoot_steps,
                                             int64_t backoff_steps,
                                             int64_t full_range_steps,
                                             int64_t speed_sps,
                                             int64_t accel_up_sps2,
                                             int64_t decel_down_sps2);

// Shared-STEP variants include small empirical overheads for gating/barriers.
uint32_t estimateMoveTimeMsSharedStep(int64_t distance_steps,
                                      int64_t speed_sps,
                                      int64_t accel_up_sps2,
                                      int64_t decel_down_sps2);
uint32_t estimateHomeTimeMsWithFullRangeSharedStep(int64_t overshoot_steps,
                                                   int64_t backoff_steps,
                                                   int64_t full_range_steps,
                                                   int64_t speed_sps,
                                                   int64_t accel_up_sps2,
                                                   int64_t decel_down_sps2);

}  // namespace MotionKinematics
