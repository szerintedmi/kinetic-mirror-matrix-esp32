#ifdef ARDUINO
#include <Arduino.h>
#endif
#include <unity.h>
#include <stdint.h>
#include "MotorControl/Bitpack.h"

void test_bitpack_dir_sleep_basic();

void test_bitpack_dir_sleep_basic() {
  TEST_ASSERT_EQUAL_HEX8(0xA5, compute_dir_bits(0xA5));
  TEST_ASSERT_EQUAL_HEX8(0x0F, compute_sleep_bits(0x00, 0x0F, true));
  TEST_ASSERT_EQUAL_HEX8(0xF3, compute_sleep_bits(0xFF, 0x0C, false));
}

