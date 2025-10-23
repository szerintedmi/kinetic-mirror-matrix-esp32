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

// Asymmetric estimator allowing separate accel (up) and decel (down).
// If decel_sps2 == 0, model stops instantly at target (SLEEP gating) with no ramp-down time.
uint32_t estimateMoveTimeMsAsym(int64_t distance_steps, int64_t speed_sps, int64_t accel_up_sps2, int64_t decel_down_sps2) {
  auto ceil_div64 = [](int64_t n, int64_t d) -> int64_t { if (d <= 0) return 0; return (n + d - 1) / d; };
  int64_t d = iabs64(distance_steps);
  if (d <= 0) return 0;
  if (speed_sps <= 0) speed_sps = 1;
  if (accel_up_sps2 <= 0) accel_up_sps2 = 1;
  int64_t v = speed_sps;
  int64_t a_up = accel_up_sps2;
  int64_t a_dn = decel_down_sps2 < 0 ? 0 : decel_down_sps2;

  // Triangular vs trapezoidal threshold in steps
  // s_up = v^2/(2 a_up); s_dn = (a_dn>0 ? v^2/(2 a_dn) : 0)
  int64_t s_up = ceil_div64(v * v, 2 * a_up);
  int64_t s_dn = (a_dn > 0) ? ceil_div64(v * v, 2 * a_dn) : 0;
  int64_t s_thresh = s_up + s_dn;

  if (d >= s_thresh) {
    // Trapezoidal: t = d/v + v/(2a_up) + (a_dn>0 ? v/(2a_dn) : 0)
    int64_t t_ms = 0;
    t_ms += ceil_div64(d * 1000, v);
    t_ms += ceil_div64(v * 1000, 2 * a_up);
    if (a_dn > 0) t_ms += ceil_div64(v * 1000, 2 * a_dn);
    if (t_ms < 0) t_ms = 0;
    return (uint32_t)t_ms;
  } else {
    // Triangular: peak speed below v
    // If a_dn == 0: t = sqrt(2*d/a_up)
    // Else: t = sqrt( 2*d*(a_up + a_dn)/(a_up * a_dn) )
    int64_t scaled;
    if (a_dn <= 0) {
      scaled = ceil_div64(2 * d * 1000000, a_up);
    } else {
      // scale numerator first to preserve precision
      int64_t num = 2 * d * (a_up + a_dn) * 1000000;
      int64_t den = a_up * a_dn;
      scaled = ceil_div64(num, den);
    }
    int64_t s_ms = isqrt_ceil(scaled);
    if (s_ms < 0) s_ms = 0;
    return (uint32_t)s_ms;
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

uint32_t estimateHomeTimeMsWithFullRange(int64_t overshoot_steps, int64_t backoff_steps,
                                         int64_t full_range_steps,
                                         int64_t speed_sps, int64_t accel_sps2) {
  int64_t o = iabs64(overshoot_steps);
  int64_t b = iabs64(backoff_steps);
  int64_t fr = iabs64(full_range_steps);
  // Leg1: full_range + overshoot (negative direction)
  uint32_t t1 = estimateMoveTimeMs(fr + o, speed_sps, accel_sps2);
  // Leg2: backoff (positive)
  uint32_t t2 = estimateMoveTimeMs(b, speed_sps, accel_sps2);
  // Leg3: center to midpoint (positive), approx fr/2
  uint32_t t3 = estimateMoveTimeMs(fr / 2, speed_sps, accel_sps2);
  return t1 + t2 + t3;
}

uint32_t estimateHomeTimeMsWithFullRangeAsym(int64_t overshoot_steps, int64_t backoff_steps,
                                             int64_t full_range_steps,
                                             int64_t speed_sps,
                                             int64_t accel_up_sps2,
                                             int64_t decel_down_sps2) {
  int64_t o = iabs64(overshoot_steps);
  int64_t b = iabs64(backoff_steps);
  int64_t fr = iabs64(full_range_steps);
  uint32_t t1 = estimateMoveTimeMsAsym(fr + o, speed_sps, accel_up_sps2, decel_down_sps2);
  uint32_t t2 = estimateMoveTimeMsAsym(b, speed_sps, accel_up_sps2, decel_down_sps2);
  uint32_t t3 = estimateMoveTimeMsAsym(fr / 2, speed_sps, accel_up_sps2, decel_down_sps2);
  return t1 + t2 + t3;
}

// Internal helpers for shared-STEP empirical overhead
static inline uint32_t overhead_move_ms(int64_t accel_up_sps2, int64_t speed_sps) {
  if (accel_up_sps2 <= 0) return 10u;
  uint64_t a = (uint64_t)accel_up_sps2;
  uint64_t v = (speed_sps > 0) ? (uint64_t)speed_sps : 1ull;
  // Scale overhead down with higher speed (align/guard costs amortize)
  // Base ~10 ms, plus (a/800) scaled by 500/v
  uint64_t base = 10u;
  uint64_t term = (a / 800u) * 500u / v;
  return (uint32_t)(base + term);
}
static inline uint32_t overhead_home_ms(int64_t accel_up_sps2, int64_t speed_sps) {
  if (accel_up_sps2 <= 0) return 40u;
  uint64_t a = (uint64_t)accel_up_sps2;
  uint64_t v = (speed_sps > 0) ? (uint64_t)speed_sps : 1ull;
  // Base ~40 ms, plus (a/200) scaled by 500/v
  uint64_t base = 40u;
  uint64_t term = (a / 200u) * 500u / v;
  return (uint32_t)(base + term);
}

uint32_t estimateMoveTimeMsSharedStep(int64_t distance_steps, int64_t speed_sps,
                                      int64_t accel_up_sps2, int64_t decel_down_sps2) {
  uint32_t t = estimateMoveTimeMsAsym(distance_steps, speed_sps, accel_up_sps2, decel_down_sps2);
  return t + overhead_move_ms(accel_up_sps2, speed_sps);
}

uint32_t estimateHomeTimeMsWithFullRangeSharedStep(int64_t overshoot_steps, int64_t backoff_steps,
                                                   int64_t full_range_steps,
                                                   int64_t speed_sps,
                                                   int64_t accel_up_sps2,
                                                   int64_t decel_down_sps2) {
  uint32_t t = estimateHomeTimeMsWithFullRangeAsym(overshoot_steps, backoff_steps, full_range_steps,
                                                   speed_sps, accel_up_sps2, decel_down_sps2);
  return t + overhead_home_ms(accel_up_sps2, speed_sps);
}

} // namespace MotionKinematics
