#ifdef ARDUINO
#include <Arduino.h>
#include <unity.h>

// Forward declarations of test functions defined across this suite
void test_auto_sleep_after_move();
void test_two_motor_overlap_constant_speed();
void test_device_wake_sleep_single();
void test_device_move_short_and_complete();
void test_device_home_sequence_sets_zero();
void test_device_move_estimate_vs_actual();
void test_device_home_estimate_vs_actual();

void setup() {
  delay(300);
  UNITY_BEGIN();
  // Shared-step basic tests
  RUN_TEST(test_auto_sleep_after_move);
  RUN_TEST(test_two_motor_overlap_constant_speed);
  // Hardware integration tests
  RUN_TEST(test_device_wake_sleep_single);
  RUN_TEST(test_device_move_short_and_complete);
  RUN_TEST(test_device_home_sequence_sets_zero);
  RUN_TEST(test_device_move_estimate_vs_actual);
  RUN_TEST(test_device_home_estimate_vs_actual);
  UNITY_END();
  // Avoid re-running tests
  while (true) { delay(1000); }
}

void loop() {}
#endif

