#ifdef ARDUINO
#include <Arduino.h>
#endif
#include <unity.h>
#include <stdint.h>
#include "backend/bitpack.h"

void test_bitpack_dir_sleep_basic();

void test_bitpack_dir_sleep_basic() {
  // DIR: pass-through of forward mask
  TEST_ASSERT_EQUAL_HEX8(0xA5, compute_dir_bits(0xA5));
  // SLEEP: base=0x00, target=0x0F, awake=true => 0x0F
  TEST_ASSERT_EQUAL_HEX8(0x0F, compute_sleep_bits(0x00, 0x0F, true));
  // SLEEP: base=0xFF, target=0x0C, awake=false => 0xF3
  TEST_ASSERT_EQUAL_HEX8(0xF3, compute_sleep_bits(0xFF, 0x0C, false));
}

