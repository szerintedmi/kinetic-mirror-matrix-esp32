#include "drivers/Stub/Shift595Stub.h"

#include <unity.h>

void test_shift595_captures_bytes_and_latch();

void test_shift595_captures_bytes_and_latch() {
  Shift595Stub drv(5);
  drv.begin();
  drv.setDirSleep(0xAA, 0x55);
  TEST_ASSERT_EQUAL_HEX8(0xAA, drv.last_dir());
  TEST_ASSERT_EQUAL_HEX8(0x55, drv.last_sleep());
  TEST_ASSERT_EQUAL_UINT(1, drv.latch_count());
}

#ifndef ARDUINO
int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_shift595_captures_bytes_and_latch);
  return UNITY_END();
}
#endif
