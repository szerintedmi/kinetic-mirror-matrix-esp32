// Minimal timing helpers for shared-STEP generator (host-testable)
#pragma once
#include <stdint.h>

namespace SharedStepTiming {

// Compute STEP period in microseconds for a given speed (steps per second).
// Returns at least 1 us to avoid divide-by-zero and zero-length periods.
inline uint32_t step_period_us(uint32_t speed_sps) {
  if (speed_sps == 0) return 0; // explicit zero to signal generator should stop
  // Round to nearest integer microseconds
  const uint64_t us = (1000000ULL + (speed_sps / 2)) / (uint64_t)speed_sps;
  return (uint32_t)(us > 0 ? us : 1);
}

// Align an absolute timestamp (microseconds) to the next STEP edge, assuming
// periodic edges every `period_us` starting at t=0.
inline uint64_t align_to_next_edge_us(uint64_t now_us, uint32_t period_us) {
  if (period_us == 0) return now_us;
  const uint64_t rem = now_us % period_us;
  return rem == 0 ? now_us : (now_us + (period_us - rem));
}

// Return true if a guard window (pre+post) comfortably fits between STEP edges
// for the given period. This is a quick feasibility check for DIR flips.
inline bool guard_fits_between_edges(uint32_t period_us, uint32_t pre_us, uint32_t post_us) {
  // Require total guard strictly less than period to stay away from edges
  const uint64_t total = (uint64_t)pre_us + (uint64_t)post_us;
  return (period_us > 0) && (total + 2 /*margin*/ < period_us);
}

// Compute stopping distance in steps to decelerate from current speed to zero
// at acceleration 'accel_sps2'. Returns ceil(v^2 / (2a)).
inline uint32_t stop_distance_steps(uint32_t speed_sps, uint32_t accel_sps2) {
  if (accel_sps2 == 0) return 0;
  const uint64_t v2 = (uint64_t)speed_sps * (uint64_t)speed_sps;
  const uint64_t den = 2ull * (uint64_t)accel_sps2;
  return (uint32_t)((v2 + den - 1) / den);
}

} // namespace SharedStepTiming
