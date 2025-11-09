#ifdef ARDUINO
#include "boards/Esp32Dev.hpp"
#include "drivers/Esp32/Shift595Vspi.h"

#include <Arduino.h>
#include <unity.h>

static const int LATCH_PIN = SHIFT595_RCLK;  // RCLK from board map

void test_shift595_esp32_minimal() {
  Shift595Esp32 drv(LATCH_PIN);
  drv.begin();
  // Latch pin should be LOW after begin
  TEST_ASSERT_EQUAL_INT_MESSAGE(LOW, digitalRead(LATCH_PIN), "Latch should be LOW after begin");

  // Call setDirSleep a couple of times; final latch level should remain LOW
  drv.setDirSleep(0x00, 0xFF);
  TEST_ASSERT_EQUAL_INT_MESSAGE(LOW, digitalRead(LATCH_PIN), "Latch should be LOW after first set");

  drv.setDirSleep(0xAA, 0x55);
  TEST_ASSERT_EQUAL_INT_MESSAGE(
      LOW, digitalRead(LATCH_PIN), "Latch should be LOW after second set");
}

void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_shift595_esp32_minimal);
  UNITY_END();
}

void loop() {}
#endif
