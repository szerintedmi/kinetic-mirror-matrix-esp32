#ifdef ARDUINO
#include "MotorControl/HardwareMotorController.h"

#include <Arduino.h>
#include <functional>
#include <unity.h>

#ifndef TEST_MOTOR_ID
#define TEST_MOTOR_ID 0
#endif
#if TEST_MOTOR_ID < 0 || TEST_MOTOR_ID > 7
#error "TEST_MOTOR_ID must be in [0..7]"
#endif
static constexpr uint8_t kMotor = (uint8_t)TEST_MOTOR_ID;

static HardwareMotorController ctrl;

static bool wait_until(std::function<bool()> pred, uint32_t timeout_ms) {
  uint32_t start = millis();
  while ((millis() - start) < timeout_ms) {
    ctrl.tick(millis());
    if (pred())
      return true;
    delay(5);
  }
  return false;
}

void test_auto_sleep_after_move() {
  // Ensure fully asleep baseline
  ctrl.sleepMask(0xFF);
  ctrl.tick(millis());
  // Issue a short move at moderate speed (accel ignored for shared step)
  const int speed = 1000;
  const int accel = 8000;
  bool ok = ctrl.moveAbsMask(1u << kMotor, 100, speed, accel, millis());
  TEST_ASSERT_TRUE(ok);
  // Wait until it reports moving
  TEST_ASSERT_TRUE_MESSAGE(wait_until([] { return ctrl.state(kMotor).moving; }, 500),
                           "motor did not start");
  // Wait until completion
  TEST_ASSERT_TRUE_MESSAGE(wait_until([] { return !ctrl.state(kMotor).moving; }, 5000),
                           "motor did not finish");
  // Expect auto-sleep after move (unless explicitly WAKE'd, which we did not)
  TEST_ASSERT_FALSE(ctrl.state(kMotor).awake);
}

void test_two_motor_overlap_constant_speed() {
  // Choose a second motor id different from kMotor
  uint8_t m2 = (kMotor == 0) ? 1 : 0;
  ctrl.sleepMask(0xFF);
  ctrl.tick(millis());
  const int speed = 1000;
  const int accel = 8000;
  // Start a concurrent move for two motors at the global constant speed
  uint32_t mask = (1u << kMotor) | (1u << m2);
  bool ok = ctrl.moveAbsMask(mask, 120, speed, accel, millis());
  TEST_ASSERT_TRUE(ok);
  // Wait both report moving
  TEST_ASSERT_TRUE_MESSAGE(
      wait_until([&] { return ctrl.state(kMotor).moving && ctrl.state(m2).moving; }, 800),
      "both motors did not start");
  // Wait both complete
  TEST_ASSERT_TRUE_MESSAGE(
      wait_until([&] { return !ctrl.state(kMotor).moving && !ctrl.state(m2).moving; }, 6000),
      "both motors did not finish");
  // Expect both are asleep by default after completion
  TEST_ASSERT_FALSE(ctrl.state(kMotor).awake);
  TEST_ASSERT_FALSE(ctrl.state(m2).awake);
}

// Arduino test entry points provided by a shared runner in this suite
#endif
