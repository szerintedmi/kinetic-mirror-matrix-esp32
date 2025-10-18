#include "MotorControl/MotionKinematics.h"
#include <stdint.h>

namespace {

static inline int64_t iabs64(int64_t v) { return v < 0 ? -v : v; }

static inline int64_t ceil_div(int64_t num, int64_t den) {
  if (den <= 0) return 0;
  if (num >= 0) return (num + den - 1) / den;
  // For negative values (shouldn't occur here), fallback to trunc towards zero then adjust
  return -(( -num) / den);
}

// Integer sqrt ceil: smallest x such that x*x >= n (for n >= 0)
static int64_t isqrt_ceil(int64_t n) {
  if (n <= 0) return 0;
  int64_t x0 = 0;
  int64_t x1 = 1;
  // Exponential search to find an upper bound
  while (x1 < n / x1) {
    x0 = x1;
    x1 <<= 1;
  }
  // Binary search between x0..x1
  int64_t lo = x0, hi = x1;
  while (lo < hi) {
    int64_t mid = lo + ((hi - lo) >> 1);
    if (mid == 0) {
      lo = 1;
      continue;
    }
    int64_t sq = mid * mid;
    if (sq >= n) {
      hi = mid;
    } else {
      lo = mid + 1;
    }
  }
  return lo;
}

} // anonymous namespace

namespace MotionKinematics {

uint32_t estimateMoveTimeMs(int64_t distance_steps, int64_t speed_sps, int64_t accel_sps2) {
  int64_t d = iabs64(distance_steps);
  if (d <= 0) return 0;
  if (speed_sps <= 0) speed_sps = 1;  // avoid div-by-zero
  if (accel_sps2 <= 0) accel_sps2 = 1; // avoid div-by-zero

  // Determine if we can reach speed_sps given distance and accel: trapezoidal vs triangular
  // Threshold distance where profile transitions: d_thresh = v^2 / a (since s_acc + s_dec = v^2/a)
  int64_t d_thresh = ceil_div(speed_sps * speed_sps, accel_sps2);
  if (d >= d_thresh) {
    // Trapezoidal: t = d/v + v/a (seconds)
    int64_t t_ms = ceil_div(d * 1000, speed_sps) + ceil_div(speed_sps * 1000, accel_sps2);
    if (t_ms < 0) t_ms = 0;
    return (uint32_t)t_ms;
  } else {
    // Triangular: t = 2 * sqrt(d / a)
    // Compute sqrt(d/a) in milliseconds without floating point:
    // s_ms = ceil( sqrt( ceil(d*1e6 / a) ) )  => sqrt(d/a) * 1000 rounded up
    int64_t scaled = ceil_div(d * 1000000, accel_sps2);
    int64_t s_ms = isqrt_ceil(scaled);
    int64_t t_ms = 2 * s_ms;
    if (t_ms < 0) t_ms = 0;
    return (uint32_t)t_ms;
  }
}

uint32_t estimateHomeTimeMs(int64_t overshoot_steps, int64_t backoff_steps,
                            int64_t speed_sps, int64_t accel_sps2) {
  int64_t o = iabs64(overshoot_steps);
  int64_t b = iabs64(backoff_steps);
  uint32_t t1 = estimateMoveTimeMs(o, speed_sps, accel_sps2);
  uint32_t t2 = estimateMoveTimeMs(b, speed_sps, accel_sps2);
  return t1 + t2;
}

} // namespace MotionKinematics

