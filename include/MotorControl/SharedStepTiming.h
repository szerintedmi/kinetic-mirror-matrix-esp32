// Minimal timing helpers for shared-STEP generator (host-testable)
#pragma once
#include <cstdint>

namespace SharedStepTiming {

struct PeriodAlignmentRequest {
  uint64_t timestamp_us = 0;
  uint32_t period_us = 0;
  explicit constexpr PeriodAlignmentRequest(uint64_t timestamp = 0, uint32_t period = 0)
      : timestamp_us(timestamp), period_us(period) {}
};

struct GuardWindow {
  uint32_t pre_us = 0;
  uint32_t post_us = 0;
  explicit constexpr GuardWindow(uint32_t pre = 0, uint32_t post = 0) : pre_us(pre), post_us(post) {}
};

struct StopDistanceRequest {
  uint32_t speed_sps = 0;
  uint32_t accel_sps2 = 0;
  explicit constexpr StopDistanceRequest(uint32_t speed = 0, uint32_t accel = 0)
      : speed_sps(speed), accel_sps2(accel) {}
};

// Compute STEP period in microseconds for a given speed (steps per second).
// Returns at least 1 us to avoid divide-by-zero and zero-length periods.
[[nodiscard]] inline uint32_t step_period_us(uint32_t speed_sps) {
  if (speed_sps == 0) {
    // Explicit zero to signal generator should stop
    return 0;
  }
  // Round to nearest integer microseconds
  const uint64_t denom = static_cast<uint64_t>(speed_sps);
  const uint64_t rounded_period_us = (1000000ULL + (denom / 2ULL)) / denom;
  return static_cast<uint32_t>(rounded_period_us > 0 ? rounded_period_us : 1ULL);
}

// Align an absolute timestamp (microseconds) to the next STEP edge, assuming
// periodic edges every `period_us` starting at t=0.
[[nodiscard]] inline uint64_t align_to_next_edge_us(PeriodAlignmentRequest request) {
  if (request.period_us == 0) {
    return request.timestamp_us;
  }
  const uint64_t remainder = request.timestamp_us % request.period_us;
  return (remainder == 0)
             ? request.timestamp_us
             : (request.timestamp_us + (request.period_us - remainder));
}

// Return true if a guard window (pre+post) comfortably fits between STEP edges
// for the given period. This is a quick feasibility check for DIR flips.
[[nodiscard]] inline bool guard_fits_between_edges(uint32_t period_us, GuardWindow guard) {
  const uint64_t total_guard = static_cast<uint64_t>(guard.pre_us) + static_cast<uint64_t>(guard.post_us);
  return (period_us > 0U) && (total_guard + 2U /*margin*/ < period_us);
}

// Compute stopping distance in steps to decelerate from current speed to zero
// at acceleration 'accel_sps2'. Returns ceil(v^2 / (2a)).
[[nodiscard]] inline uint32_t stop_distance_steps(StopDistanceRequest request) {
  if (request.accel_sps2 == 0U) {
    return 0;
  }
  const uint64_t speed_squared = static_cast<uint64_t>(request.speed_sps) * static_cast<uint64_t>(request.speed_sps);
  const uint64_t denominator = 2ULL * static_cast<uint64_t>(request.accel_sps2);
  return static_cast<uint32_t>((speed_squared + denominator - 1ULL) / denominator);
}

} // namespace SharedStepTiming
