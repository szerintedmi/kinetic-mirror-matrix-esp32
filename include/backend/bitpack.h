#pragma once
#include <stdint.h>

// Bit masks cover motors 0..7. Bit i corresponds to motor i.
// DIR semantics: 1 = forward, 0 = reverse (logical convention in firmware)
// SLEEP semantics: 1 = awake (SLEEP pin HIGH), 0 = sleep (SLEEP pin LOW)

// Returns the DIR bits given a mask of motors moving forward.
inline uint8_t compute_dir_bits(uint8_t forward_mask) {
  return forward_mask; // direct mapping
}

// Returns the SLEEP bits after applying an override for the target set.
// If awake == true, sets target bits to 1; if false, clears target bits.
inline uint8_t compute_sleep_bits(uint8_t base_sleep_bits, uint8_t target_mask, bool awake) {
  return awake ? (uint8_t)(base_sleep_bits | target_mask)
               : (uint8_t)(base_sleep_bits & (uint8_t)~target_mask);
}

