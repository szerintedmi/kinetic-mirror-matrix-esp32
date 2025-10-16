#ifdef ARDUINO
#include <Arduino.h>
#endif
#include <unity.h>
#include "drivers/shift595.h"

void test_shift595_captures_bytes_and_latch();

void test_shift595_captures_bytes_and_latch() {
  Shift595 drv(5);
  drv.begin();
  drv.setDirSleep(0xAA, 0x55);
#ifdef UNIT_TEST
  TEST_ASSERT_EQUAL_HEX8(0xAA, drv.last_dir());
  TEST_ASSERT_EQUAL_HEX8(0x55, drv.last_sleep());
  TEST_ASSERT_TRUE(drv.latch_count() >= 1);
#else
  TEST_ASSERT_TRUE(true);
#endif
}

