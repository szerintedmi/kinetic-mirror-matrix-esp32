#pragma once
#include <stdint.h>

namespace MotionKinematics {

// Estimate time in milliseconds for a point-to-point move over
// distance_steps at capped speed (steps/s) with acceleration (steps/s^2).
// Uses triangular/trapezoidal profile with integer math and ceil rounding.
uint32_t estimateMoveTimeMs(int64_t distance_steps, int64_t speed_sps, int64_t accel_sps2);

// Estimate HOME total time as two legs (overshoot + backoff), each
// treated as an independent move. Returns total milliseconds.
uint32_t estimateHomeTimeMs(int64_t overshoot_steps, int64_t backoff_steps,
                            int64_t speed_sps, int64_t accel_sps2);

} // namespace MotionKinematics

