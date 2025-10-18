#include "StubMotorController.h"
#include "MotorControl/MotorControlConstants.h"
#include "MotorControl/MotionKinematics.h"
#include <math.h>

static uint32_t maskForId(uint8_t id) { return 1u << id; }

StubMotorController::StubMotorController(uint8_t count) : count_(count) {
  if (count_ > 8) count_ = 8;
  for (uint8_t i = 0; i < count_; ++i) {
    motors_[i] = MotorState{ i, 0, MotorControlConstants::DEFAULT_SPEED_SPS, MotorControlConstants::DEFAULT_ACCEL_SPS2, false, false, false, 0, MotorControlConstants::BUDGET_TENTHS_MAX, 0, 0, 0, 0, 0, false };
    plans_[i] = MovePlan{ false, false, 0, 0, 0 };
  }
}

bool StubMotorController::isAnyMovingForMask(uint32_t mask) const {
  for (uint8_t i = 0; i < count_; ++i) {
    if (mask & maskForId(i)) {
      if (motors_[i].moving) return true;
    }
  }
  return false;
}

void StubMotorController::wakeMask(uint32_t mask) {
  for (uint8_t i = 0; i < count_; ++i) {
    if (mask & maskForId(i)) {
      motors_[i].awake = true;
    }
  }
}

bool StubMotorController::sleepMask(uint32_t mask) {
  for (uint8_t i = 0; i < count_; ++i) {
    if (mask & maskForId(i)) {
      if (motors_[i].moving) return false;
    }
  }
  for (uint8_t i = 0; i < count_; ++i) {
    if (mask & maskForId(i)) {
      motors_[i].awake = false;
    }
  }
  return true;
}

bool StubMotorController::moveAbsMask(uint32_t mask, long target, int speed, int accel, uint32_t now_ms) {
  if (isAnyMovingForMask(mask)) return false;
  for (uint8_t i = 0; i < count_; ++i) {
    if (mask & maskForId(i)) {
      motors_[i].awake = true;
      motors_[i].speed = speed;
      motors_[i].accel = accel;
      motors_[i].moving = true;
      long delta = labs(target - motors_[i].position);
      uint32_t dur_ms = MotionKinematics::estimateMoveTimeMs(delta, speed, accel);
      plans_[i].active = true;
      plans_[i].is_home = false;
      plans_[i].target = target;
      plans_[i].start_pos = motors_[i].position;
      plans_[i].end_ms = now_ms + dur_ms;
      motors_[i].last_op_type = 1;
      motors_[i].last_op_started_ms = now_ms;
      motors_[i].last_op_est_ms = dur_ms;
      motors_[i].last_op_ongoing = true;
    }
  }
  return true;
}

bool StubMotorController::homeMask(uint32_t mask, long overshoot, long backoff, int speed, int accel, long /*full_range*/, uint32_t now_ms) {
  if (isAnyMovingForMask(mask)) return false;
  uint32_t dur_ms = MotionKinematics::estimateHomeTimeMs(overshoot, backoff, speed, accel);
  for (uint8_t i = 0; i < count_; ++i) {
    if (mask & maskForId(i)) {
      motors_[i].awake = true;
      motors_[i].speed = speed;
      motors_[i].accel = accel;
      motors_[i].moving = true;
      plans_[i].active = true;
      plans_[i].is_home = true;
      plans_[i].target = 0;
      plans_[i].start_pos = motors_[i].position;
      plans_[i].end_ms = now_ms + dur_ms;
      motors_[i].last_op_type = 2;
      motors_[i].last_op_started_ms = now_ms;
      motors_[i].last_op_est_ms = dur_ms;
      motors_[i].last_op_ongoing = true;
    }
  }
  return true;
}

void StubMotorController::tick(uint32_t now_ms) {
  // Budget bookkeeping (tenths of seconds)

  for (uint8_t i = 0; i < count_; ++i) {
    // Update budget based on awake state; accumulate in whole seconds
    if (now_ms >= motors_[i].last_update_ms) {
      uint32_t dt_ms = now_ms - motors_[i].last_update_ms;
      uint32_t whole_sec = dt_ms / 1000;
      if (whole_sec > 0) {
        if (motors_[i].awake) {
          motors_[i].budget_tenths -= (int32_t)(MotorControlConstants::SPEND_TENTHS_PER_SEC * (int32_t)whole_sec);
          // Clamp minimum so max cooldown is bounded
          const int32_t kBudgetFloor = (int32_t)(MotorControlConstants::BUDGET_TENTHS_MAX -
            MotorControlConstants::REFILL_TENTHS_PER_SEC * (int32_t)MotorControlConstants::MAX_COOL_DOWN_TIME_S);
          if (motors_[i].budget_tenths < kBudgetFloor) motors_[i].budget_tenths = kBudgetFloor;
        } else {
          motors_[i].budget_tenths += (int32_t)(MotorControlConstants::REFILL_TENTHS_PER_SEC * (int32_t)whole_sec);
          if (motors_[i].budget_tenths > MotorControlConstants::BUDGET_TENTHS_MAX) motors_[i].budget_tenths = MotorControlConstants::BUDGET_TENTHS_MAX;
        }
        motors_[i].last_update_ms += whole_sec * 1000;
      }
    }

    // Complete moves scheduled in the stub plan
    if (plans_[i].active && now_ms >= plans_[i].end_ms) {
      long old_pos = motors_[i].position;
      motors_[i].position = plans_[i].target;
      motors_[i].moving = false;
      motors_[i].awake = false;
      if (motors_[i].last_op_ongoing) {
        motors_[i].last_op_ongoing = false;
        if (motors_[i].last_op_started_ms != 0) {
          motors_[i].last_op_last_ms = now_ms - motors_[i].last_op_started_ms;
        }
      }
      if (plans_[i].is_home) {
        motors_[i].homed = true;
        motors_[i].steps_since_home = 0;
      } else {
        if (motors_[i].homed) {
          long delta = labs(motors_[i].position - old_pos);
          motors_[i].steps_since_home += (int32_t)delta;
        }
      }
      plans_[i].active = false;
    }
  }
}
