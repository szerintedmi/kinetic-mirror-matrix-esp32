#pragma once
#include <stdint.h>

inline uint8_t compute_dir_bits(uint8_t forward_mask) {
  return forward_mask; // direct mapping
}

inline uint8_t compute_sleep_bits(uint8_t base_sleep_bits, uint8_t target_mask, bool awake) {
  return awake ? (uint8_t)(base_sleep_bits | target_mask)
               : (uint8_t)(base_sleep_bits & (uint8_t)~target_mask);
}

