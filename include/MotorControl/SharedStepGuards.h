// Direction/SLEEP guard timing and scheduling helpers
#pragma once
#include <cstdint>
#include "MotorControl/SharedStepTiming.h"

namespace SharedStepGuards {

#if defined(DIR_GUARD_US_PRE)
constexpr uint32_t kDirGuardPreUs = static_cast<uint32_t>(DIR_GUARD_US_PRE);
#else
constexpr uint32_t kDirGuardPreUs = 3U;
#endif

#if defined(DIR_GUARD_US_POST)
constexpr uint32_t kDirGuardPostUs = static_cast<uint32_t>(DIR_GUARD_US_POST);
#else
constexpr uint32_t kDirGuardPostUs = 3U;
#endif

struct DirFlipWindow {
  uint64_t t_sleep_low = 0;   // time to disable SLEEP (stop stepping for this motor)
  uint64_t t_dir_flip = 0;    // time to toggle DIR
  uint64_t t_sleep_high = 0;  // time to re-enable SLEEP (resume stepping)
};

struct FlipWindowRequest {
  uint64_t now_us = 0;
  uint32_t period_us = 0;
  explicit constexpr FlipWindowRequest(uint64_t now = 0, uint32_t period = 0)
      : now_us(now), period_us(period) {}
};

// Compute a safe scheduling window for a DIR change.
// We always schedule within the gap following the next STEP edge to ensure the
// full period is available for pre/post guard.
[[nodiscard]] inline bool compute_flip_window(FlipWindowRequest request, DirFlipWindow &win) {
  using SharedStepTiming::GuardWindow;
  using SharedStepTiming::align_to_next_edge_us;
  using SharedStepTiming::guard_fits_between_edges;

  if (!guard_fits_between_edges(request.period_us, GuardWindow(kDirGuardPreUs, kDirGuardPostUs))) {
    return false;
  }
  // Next STEP edge after now
  const uint64_t t_edge_next{align_to_next_edge_us(SharedStepTiming::PeriodAlignmentRequest(request.now_us, request.period_us))};
  // Schedule at the middle of the gap to stay away from edges
  const uint64_t t_mid{t_edge_next + (request.period_us / 2U)};
  win.t_dir_flip = t_mid;
  win.t_sleep_low = t_mid - kDirGuardPreUs;
  win.t_sleep_high = t_mid + kDirGuardPostUs;
  return true;
}

} // namespace SharedStepGuards
