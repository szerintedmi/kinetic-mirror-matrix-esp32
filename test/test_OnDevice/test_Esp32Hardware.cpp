#ifdef ARDUINO
#include <Arduino.h>
#include <unity.h>
#include <functional>

#include "MotorControl/HardwareMotorController.h"
#include "MotorControl/MotionKinematics.h"

#ifndef TEST_MOTOR_ID
#define TEST_MOTOR_ID 0
#endif
#if TEST_MOTOR_ID < 0 || TEST_MOTOR_ID > 7
#error "TEST_MOTOR_ID must be in [0..7]"
#endif
static constexpr uint8_t kMotor = (uint8_t)TEST_MOTOR_ID;

static HardwareMotorController ctrl; // uses ESP32 drivers + 74HC595

static bool wait_until(std::function<bool()> pred, uint32_t timeout_ms) {
  uint32_t start = millis();
  while ((millis() - start) < timeout_ms) {
    ctrl.tick(millis());
    if (pred()) return true;
    delay(5);
  }
  return false;
}

void test_device_wake_sleep_single() {
  // Ensure asleep first
  ctrl.sleepMask(0xFF);
  ctrl.tick(millis());
  // WAKE selected motor
  ctrl.wakeMask(1u << kMotor);
  ctrl.tick(millis());
  TEST_ASSERT_TRUE(ctrl.state(kMotor).awake);
  // SLEEP selected motor
  bool ok = ctrl.sleepMask(1u << kMotor);
  TEST_ASSERT_TRUE(ok);
  ctrl.tick(millis());
  TEST_ASSERT_FALSE(ctrl.state(kMotor).awake);
}

void test_device_move_short_and_complete() {
  // Move selected motor to +100 steps quickly
  bool ok = ctrl.moveAbsMask(1u << kMotor, 100, 6000, 16000, millis());
  TEST_ASSERT_TRUE(ok);
  // Should report moving soon
  bool started = wait_until([] { return ctrl.state(kMotor).moving; }, 500);
  TEST_ASSERT_TRUE_MESSAGE(started, "motor did not start moving");
  // Wait for completion and final position
  bool finished = wait_until([] { return !ctrl.state(kMotor).moving; }, 5000);
  TEST_ASSERT_TRUE_MESSAGE(finished, "motor did not finish in time");
  TEST_ASSERT_EQUAL(100, ctrl.state(kMotor).position);
}

void test_device_home_sequence_sets_zero() {
  // Execute HOME on selected motor with defaults
  bool ok = ctrl.homeMask(1u << kMotor, 800, 150, 4000, 16000, 2400, millis());
  TEST_ASSERT_TRUE(ok);
  // Wait until not moving
  bool finished = wait_until([] { return !ctrl.state(kMotor).moving; }, 8000);
  TEST_ASSERT_TRUE_MESSAGE(finished, "home did not finish in time");
  TEST_ASSERT_EQUAL(0, ctrl.state(kMotor).position);
  TEST_ASSERT_FALSE(ctrl.state(kMotor).awake);
}

void test_device_move_estimate_vs_actual() {
  // Plan a short move and compare to estimator
  const long target = 200;
  const int speed = 2000;
  const int accel = 8000;
  uint32_t t0 = millis();
  bool ok = ctrl.moveAbsMask(1u << kMotor, target, speed, accel, t0);
  TEST_ASSERT_TRUE(ok);
  // Firmware records its own estimate (model depends on build); use that
  uint32_t est_ms = (uint32_t)ctrl.state(kMotor).last_op_est_ms;
  // Wait for completion
  bool finished = wait_until([] { return !ctrl.state(kMotor).moving; }, 5000);
  TEST_ASSERT_TRUE(finished);
  uint32_t dt = millis() - t0;
  // Symmetric tolerance check around firmware's estimate
  uint32_t tol_abs_ms = 150;
  uint32_t tol_pct_ms = (uint32_t)((est_ms * 10) / 100); // 10%
  uint32_t tol = (tol_pct_ms > tol_abs_ms) ? tol_pct_ms : tol_abs_ms;
  uint32_t lo = (est_ms > tol) ? (est_ms - tol) : 0;
  uint32_t hi = est_ms + tol;
  std::string msg = std::string("actual=") + std::to_string(dt) +
                    " est=" + std::to_string(est_ms) +
                    " tol=±" + std::to_string(tol);
  TEST_ASSERT_TRUE_MESSAGE(dt >= lo && dt <= hi, msg.c_str());
}

void test_device_home_estimate_vs_actual() {
  const long overshoot = 800;
  const long backoff = 150;
  const int speed = 4000;
  const int accel = 16000;
  uint32_t t0 = millis();
  bool ok = ctrl.homeMask(1u << kMotor, overshoot, backoff, speed, accel, 2400, t0);
  TEST_ASSERT_TRUE(ok);
  bool finished = wait_until([] { return !ctrl.state(kMotor).moving; }, 8000);
  TEST_ASSERT_TRUE(finished);
  uint32_t est_ms = (uint32_t)ctrl.state(kMotor).last_op_est_ms;
  uint32_t dt = millis() - t0;
  // Symmetric tolerance check around firmware's estimate
  uint32_t tol_abs_ms = 150;
  uint32_t tol_pct_ms = (uint32_t)((est_ms * 10) / 100); // 10%
  uint32_t tol = (tol_pct_ms > tol_abs_ms) ? tol_pct_ms : tol_abs_ms;
  uint32_t lo = (est_ms > tol) ? (est_ms - tol) : 0;
  uint32_t hi = est_ms + tol;
  std::string msg = std::string("actual=") + std::to_string(dt) +
                    " est=" + std::to_string(est_ms) +
                    " tol=±" + std::to_string(tol);
  TEST_ASSERT_TRUE_MESSAGE(dt >= lo && dt <= hi, msg.c_str());
}

// Arduino test entry points provided by a shared runner in this suite
#endif
