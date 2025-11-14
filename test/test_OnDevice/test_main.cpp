#ifdef ARDUINO
#include <Arduino.h>
#include <unity.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Forward declarations of test functions defined across this suite
void test_auto_sleep_after_move();
void test_two_motor_overlap_constant_speed();
void test_device_wake_sleep_single();
void test_device_move_short_and_complete();
void test_device_home_sequence_sets_zero();
void test_device_move_estimate_vs_actual();
void test_device_home_estimate_vs_actual();
// NetOnboarding tests
void test_net_ap_after_reset_no_creds();
void test_net_connect_timeout_to_ap();
void test_net_happy_path_if_seeded();

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
  // Net onboarding smoke tests
  RUN_TEST(test_net_ap_after_reset_no_creds);
  RUN_TEST(test_net_connect_timeout_to_ap);
  RUN_TEST(test_net_happy_path_if_seeded);
  UNITY_END();
  vTaskDelay(portMAX_DELAY);
}

void loop() {}
#endif
