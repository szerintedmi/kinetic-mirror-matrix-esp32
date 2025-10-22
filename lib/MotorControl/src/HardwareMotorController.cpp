#include "MotorControl/HardwareMotorController.h"
#include "MotorControl/MotorControlConstants.h"
#include "MotorControl/MotionKinematics.h"
#include <string.h>

#if defined(ARDUINO)
#include <Arduino.h>
#include "boards/Esp32Dev.hpp"
#include "drivers/Esp32/Shift595Vspi.h"
// The ESP32 adapter is declared below (guarded) to avoid native includes.
#endif

HardwareMotorController::HardwareMotorController(IShift595& shift, IFasAdapter& fas, uint8_t count) {
  if (count > 8) count = 8;
  count_ = count;
  shift_ = &shift;
  fas_ = &fas;
  for (uint8_t i = 0; i < count_; ++i) {
    motors_[i] = MotorState{ i, 0,
      MotorControlConstants::DEFAULT_SPEED_SPS,
      MotorControlConstants::DEFAULT_ACCEL_SPS2,
      false, false,
      false, 0,
      MotorControlConstants::BUDGET_TENTHS_MAX,
      0, 0, 0, 0, 0, false };
    homing_[i] = HomingPlan{ false, 0, 0, 0, 0, 0, 0 };
  }
  // Initialize hardware/adapters
  shift_->begin();
  fas_->begin();
  // Wire 74HC595 into FAS external-pin callback
  fas_->attachShiftRegister(shift_);
  
  // Default initial state: all sleeping handled by initial latch outside
  for (uint8_t i = 0; i < count_; ++i) {
    motors_[i].awake = false;
    homing_[i].active = false;
  }
#if !defined(ARDUINO)
  dir_bits_ = 0;
  sleep_bits_ = 0;
  latch_();
#endif
}

#if defined(ARDUINO)
// Forward declare ESP32 FastAccelStepper adapter type
class FasAdapterEsp32;

HardwareMotorController::HardwareMotorController() {
  // Own the concrete drivers under Arduino/ESP32
  owned_shift_.reset(new Shift595Esp32(SHIFT595_RCLK, SHIFT595_OE));
  shift_ = owned_shift_.get();
  // Motion adapter factory selects shared-step or FAS based on build flags
  owned_fas_.reset(createEsp32MotionAdapter());
  fas_ = owned_fas_.get();

  for (uint8_t i = 0; i < count_; ++i) {
    motors_[i] = MotorState{ i, 0,
      MotorControlConstants::DEFAULT_SPEED_SPS,
      MotorControlConstants::DEFAULT_ACCEL_SPS2,
      false, false,
      false, 0,
      MotorControlConstants::BUDGET_TENTHS_MAX,
      0, 0, 0, 0, 0, false };
    homing_[i] = HomingPlan{ false, 0, 0, 0, 0, 0, 0 };
  }
  shift_->begin();
  fas_->begin();
  fas_->attachShiftRegister(shift_);
  // Configure step pins for motors 0..7
  for (uint8_t i = 0; i < count_; ++i) {
    fas_->configureStepPin(i, STEP_PINS[i]);
  }
  // Initial sleeping state handled by Shift595Esp32 begin() + controller setup
  for (uint8_t i = 0; i < count_; ++i) {
    motors_[i].awake = false;
    homing_[i].active = false;
  }
#if !defined(ARDUINO)
  dir_bits_ = 0;
  sleep_bits_ = 0;
  latch_();
#endif
}
#endif

bool HardwareMotorController::isAnyMovingForMask(uint32_t mask) const {
  for (uint8_t i = 0; i < count_; ++i) {
    if (mask & maskForId(i)) {
      if (fas_->isMoving(i)) return true;
    }
  }
  return false;
}

void HardwareMotorController::latch_() {
  // Push current bits to 74HC595
  shift_->setDirSleep(dir_bits_, sleep_bits_);
}

void HardwareMotorController::startMoveSingle_(uint8_t i, long target, int speed, int accel) {
  long cur = fas_->currentPosition(i);
  motors_[i].position = cur;
  motors_[i].speed = speed;
  motors_[i].accel = accel;
  motors_[i].moving = true;
#if defined(ARDUINO)
  motors_[i].awake = true;
#else
  long delta = target - cur;
  if (delta >= 0) { dir_bits_ |= (1u << i); } else { dir_bits_ &= (uint8_t)~(1u << i); }
  sleep_bits_ |= (1u << i);
  latch_();
#endif
  fas_->startMoveAbs(i, target, speed, accel);
}

void HardwareMotorController::wakeMask(uint32_t mask) {
  for (uint8_t i = 0; i < count_; ++i) {
    if (mask & maskForId(i)) {
      motors_[i].awake = true;
      forced_awake_mask_ |= (1u << i);
#if defined(ARDUINO)
      fas_->setAutoEnable(i, false);
      fas_->enableOutputs(i);
#else
      sleep_bits_ |= (1u << i);
#endif
    }
  }
#if !defined(ARDUINO)
  latch_();
#endif
}

bool HardwareMotorController::sleepMask(uint32_t mask) {
  // Disallow sleep if any targeted motor is moving
  if (isAnyMovingForMask(mask)) return false;
  for (uint8_t i = 0; i < count_; ++i) {
    if (mask & maskForId(i)) {
      motors_[i].awake = false;
      forced_awake_mask_ &= (uint8_t)~(1u << i);
#if defined(ARDUINO)
      fas_->disableOutputs(i);
      fas_->setAutoEnable(i, true);
#else
      sleep_bits_ &= (uint8_t)~(1u << i);
#endif
    }
  }
#if !defined(ARDUINO)
  latch_();
#endif
  return true;
}

bool HardwareMotorController::moveAbsMask(uint32_t mask, long target, int speed, int accel, uint32_t now_ms) {
  // Busy if any selected motor is already running
  if (isAnyMovingForMask(mask)) return false;

  // Update motor state and start moves; DIR/SLEEP handled by FAS on Arduino,
  // and by controller (native) to satisfy unit tests
  for (uint8_t i = 0; i < count_; ++i) {
    if (mask & maskForId(i)) {
      long cur = fas_->currentPosition(i);
      motors_[i].position = cur;
      motors_[i].speed = speed;
      motors_[i].accel = accel;
      motors_[i].moving = true;
#if defined(ARDUINO)
      // Arduino backends (FAS or SharedStep adapter) manage DIR/SLEEP internally.
      // Reflect desired intent in state only; actual gating handled by adapter.
      motors_[i].awake = true;
#else
      long delta = target - cur;
      if (delta >= 0) { dir_bits_ |= (1u << i); } else { dir_bits_ &= (uint8_t)~(1u << i); }
      sleep_bits_ |= (1u << i);
#endif
      // Record last op timing
      long dist = (target > cur) ? (target - cur) : (cur - target);
      uint32_t est = MotionKinematics::estimateMoveTimeMs(dist, speed, accel);
      motors_[i].last_op_type = 1;
      motors_[i].last_op_started_ms = now_ms;
      motors_[i].last_op_est_ms = est;
      motors_[i].last_op_ongoing = true;
    }
  }
 
#if !defined(ARDUINO)
  latch_();
#endif
  // Start steppers
  bool ok = true;
  for (uint8_t i = 0; i < count_; ++i) {
    if (mask & maskForId(i)) {
      if (!fas_->startMoveAbs(i, target, speed, accel)) ok = false;
    }
  }
  return ok;
}

bool HardwareMotorController::homeMask(uint32_t mask, long overshoot, long backoff, int speed, int accel, long full_range, uint32_t now_ms) {
  // Reject if any targeted motor is currently busy
  if (isAnyMovingForMask(mask)) return false;

  // Derive defaults as needed
  if (full_range <= 0) full_range = 2400; // kMaxPos - kMinPos by convention
  long oshot = (overshoot < 0) ? -overshoot : overshoot;
  long bko = (backoff < 0) ? -backoff : backoff;

  // Prepare DIR/SLEEP bits first for all targets, then latch once before starts (native)
  for (uint8_t i = 0; i < count_; ++i) {
    if (mask & maskForId(i)) {
      // Initialize homing plan for motor i
      homing_[i] = HomingPlan{ true, 0, oshot, bko, full_range, speed, accel };
      long cur = fas_->currentPosition(i);
      motors_[i].position = cur;
      motors_[i].speed = speed;
      motors_[i].accel = accel;
      motors_[i].moving = true;
#if defined(ARDUINO)
      // Arduino backends (FAS or SharedStep) manage DIR/SLEEP; set intent only.
      motors_[i].awake = true;
#else
      long target = cur - (full_range + oshot);
      long delta = target - cur;
      if (delta >= 0) { dir_bits_ |= (1u << i); } else { dir_bits_ &= (uint8_t)~(1u << i); }
      sleep_bits_ |= (1u << i);
#endif
      // Record last op timing (entire HOME sequence estimate)
      uint32_t est = MotionKinematics::estimateHomeTimeMsWithFullRange(overshoot, backoff, full_range, speed, accel);
      motors_[i].last_op_type = 2;
      motors_[i].last_op_started_ms = now_ms;
      motors_[i].last_op_est_ms = est;
      motors_[i].last_op_ongoing = true;
    }
  }
#if !defined(ARDUINO)
  latch_();
#endif
  // Issue first leg (negative run) moves
  for (uint8_t i = 0; i < count_; ++i) {
    if (mask & maskForId(i)) {
      long cur = fas_->currentPosition(i);
      long target = cur - (full_range + oshot);
      fas_->startMoveAbs(i, target, speed, accel);
    }
  }
  return true;
}

void HardwareMotorController::tick(uint32_t now_ms) {
  // Pull runtime state from adapter; awake reflects running or WAKE override
  for (uint8_t i = 0; i < count_; ++i) {
    if (now_ms >= motors_[i].last_update_ms) {
      uint32_t dt_ms = now_ms - motors_[i].last_update_ms;
      uint32_t whole_sec = dt_ms / 1000;
      if (whole_sec > 0) {
        const int32_t kBudgetFloor = (int32_t)(MotorControlConstants::BUDGET_TENTHS_MAX -
          MotorControlConstants::REFILL_TENTHS_PER_SEC * (int32_t)MotorControlConstants::MAX_COOL_DOWN_TIME_S);
        if (motors_[i].awake) {
          motors_[i].budget_tenths -= (int32_t)(MotorControlConstants::SPEND_TENTHS_PER_SEC * (int32_t)whole_sec);
          if (motors_[i].budget_tenths < kBudgetFloor) motors_[i].budget_tenths = kBudgetFloor;
        } else {
          motors_[i].budget_tenths += (int32_t)(MotorControlConstants::REFILL_TENTHS_PER_SEC * (int32_t)whole_sec);
          if (motors_[i].budget_tenths > MotorControlConstants::BUDGET_TENTHS_MAX) motors_[i].budget_tenths = MotorControlConstants::BUDGET_TENTHS_MAX;
        }
        motors_[i].last_update_ms += whole_sec * 1000;
      }
    }
    bool running = fas_->isMoving(i);
    motors_[i].moving = running;
    long pos = fas_->currentPosition(i);
    long old_pos = motors_[i].position;
    if (motors_[i].homed && pos != old_pos) {
      long d = pos - old_pos;
      if (d < 0) d = -d;
      motors_[i].steps_since_home += (int32_t)d;
    }
    motors_[i].position = pos;
#if defined(ARDUINO)
    motors_[i].awake = running || ((forced_awake_mask_ & (1u << i)) != 0);
#else
    // Auto-sleep at idle in native mode to validate latch behavior
    if (!running && (forced_awake_mask_ & (1u << i)) == 0) {
      if (sleep_bits_ & (1u << i)) { sleep_bits_ &= (uint8_t)~(1u << i); latch_(); }
      motors_[i].awake = false;
    } else {
      motors_[i].awake = true;
    }
#endif

    // Auto-sleep when budget overrun exceeds grace period
    if (thermal_limits_enabled_) {
      const int32_t overrun_tenths = -MotorControlConstants::AUTO_SLEEP_IF_OVER_BUDGET_S * 10;
      if (motors_[i].budget_tenths < overrun_tenths) {
        // Clear WAKE override and force outputs off
        forced_awake_mask_ &= (uint8_t)~(1u << i);
#if defined(ARDUINO)
        fas_->disableOutputs(i);
        fas_->setAutoEnable(i, true);
#else
        if (sleep_bits_ & (1u << i)) { sleep_bits_ &= (uint8_t)~(1u << i); latch_(); }
#endif
        motors_[i].awake = false;
        // Stop homing plan if active; mark operation complete
        if (homing_[i].active || motors_[i].moving) {
          homing_[i].active = false;
          motors_[i].moving = false;
          if (motors_[i].last_op_ongoing) {
            motors_[i].last_op_ongoing = false;
            if (motors_[i].last_op_started_ms != 0 && now_ms >= motors_[i].last_op_started_ms) {
              motors_[i].last_op_last_ms = now_ms - motors_[i].last_op_started_ms;
            }
          }
        }
      }
    }

    // Homing sequence state machine
    if (homing_[i].active && !motors_[i].moving) {
      HomingPlan &hp = homing_[i];
      if (hp.phase == 0) {
        // Leg 2: backoff in positive direction by backoff
        long cur = fas_->currentPosition(i);
        long target = cur + hp.backoff;
        startMoveSingle_(i, target, hp.speed, hp.accel);
        hp.phase = 1;
      } else if (hp.phase == 1) {
        // Leg 3: center to soft midpoint (~ half of full_range forward)
        long cur = fas_->currentPosition(i);
        long mid_delta = (hp.full_range > 0) ? (hp.full_range / 2) : 1200;
        long target = cur + mid_delta;
        startMoveSingle_(i, target, hp.speed, hp.accel);
        hp.phase = 2;
      } else {
        // Finalize: set runtime zero and complete
        fas_->setCurrentPosition(i, 0);
        motors_[i].position = 0;
        motors_[i].moving = false;
        motors_[i].homed = true;
        motors_[i].steps_since_home = 0;
        hp.active = false;
        // Awake state will be handled by regular logic above on next tick
      }
    }
    // Record completion time for last op when no longer moving and no active homing
    if (motors_[i].last_op_ongoing && !motors_[i].moving && !homing_[i].active) {
      motors_[i].last_op_ongoing = false;
      if (motors_[i].last_op_started_ms != 0) {
        motors_[i].last_op_last_ms = now_ms - motors_[i].last_op_started_ms;
      }
    }
  }
  // Native: start/stop latches handled above; Arduino: adapter handles gating
}
