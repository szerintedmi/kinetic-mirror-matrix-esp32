// Direction/SLEEP guard timing and scheduling helpers
#pragma once
#include <stdint.h>
#include "MotorControl/SharedStepTiming.h"

// Allow overriding via build flags if needed
#ifndef DIR_GUARD_US_PRE
#define DIR_GUARD_US_PRE 3
#endif
#ifndef DIR_GUARD_US_POST
#define DIR_GUARD_US_POST 3
#endif

namespace SharedStepGuards {

struct DirFlipWindow {
  uint64_t t_sleep_low;   // time to disable SLEEP (stop stepping for this motor)
  uint64_t t_dir_flip;    // time to toggle DIR
  uint64_t t_sleep_high;  // time to re-enable SLEEP (resume stepping)
};

// Compute a safe scheduling window for a DIR change.
// We always schedule within the gap following the next STEP edge to ensure the
// full period is available for pre/post guard.
inline bool compute_flip_window(uint64_t now_us, uint32_t period_us, DirFlipWindow &win) {
  using namespace SharedStepTiming;
  if (!guard_fits_between_edges(period_us, DIR_GUARD_US_PRE, DIR_GUARD_US_POST)) {
    return false;
  }
  // Next STEP edge after now
  const uint64_t t_edge_next = align_to_next_edge_us(now_us, period_us);
  // Schedule at the middle of the gap to stay away from edges
  const uint64_t t_mid = t_edge_next + (period_us / 2);
  win.t_dir_flip = t_mid;
  win.t_sleep_low = t_mid - DIR_GUARD_US_PRE;
  win.t_sleep_high = t_mid + DIR_GUARD_US_POST;
  return true;
}

} // namespace SharedStepGuards

