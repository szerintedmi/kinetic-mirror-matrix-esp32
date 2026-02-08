#include "MotorControl/HardwareMotorController.h"

#include "MotorControl/BuildConfig.h"
#include "MotorControl/MotionKinematics.h"
#include "MotorControl/MotorControlConstants.h"

#include <string.h>

#if defined(ARDUINO)
#include "boards/Esp32Dev.hpp"
#include "drivers/Esp32/Shift595Vspi.h"

#include <Arduino.h>
// The ESP32 adapter is declared below (guarded) to avoid native includes.
#endif

HardwareMotorController::HardwareMotorController(IShift595& shift,
                                                 IFasAdapter& fas,
                                                 uint8_t count) {
  if (count > 8)
    count = 8;
  count_ = count;
  shift_ = &shift;
  fas_ = &fas;
  for (uint8_t i = 0; i < count_; ++i) {
    motors_[i] = MotorState{i,
                            0,
                            MotorControlConstants::DEFAULT_SPEED_SPS,
                            MotorControlConstants::DEFAULT_ACCEL_SPS2,
                            false,
                            false,
                            false,
                            0,
                            MotorControlConstants::BUDGET_TENTHS_MAX,
                            0,
                            0,  // budget_accum_ms
                            0,
                            0,
                            0,
                            0,
                            false,
                            MovePhase::NONE,
                            0, 0, 0, 0, 0, 0, 0, 0, 0};
    homing_[i] = HomingPlan{false, 0, 0, 0, 0, 0, 0};
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
    motors_[i] = MotorState{i,
                            0,
                            MotorControlConstants::DEFAULT_SPEED_SPS,
                            MotorControlConstants::DEFAULT_ACCEL_SPS2,
                            false,
                            false,
                            false,
                            0,
                            MotorControlConstants::BUDGET_TENTHS_MAX,
                            0,
                            0,  // budget_accum_ms
                            0,
                            0,
                            0,
                            0,
                            false,
                            MovePhase::NONE,
                            0, 0, 0, 0, 0, 0, 0, 0, 0};
    homing_[i] = HomingPlan{false, 0, 0, 0, 0, 0, 0};
  }
  shift_->begin();
  fas_->begin();
  fas_->attachShiftRegister(shift_);
  // Configure step pins for motors 0..7
  for (uint8_t i = 0; i < count_; ++i) {
    fas_->configureStepPin(i, STEP_PINS[i]);
  }
  // Initialize microstepping GPIO driver (shared across all DRV8825 drivers)
  microstep_gpio_.reset(new MicrostepGpio(MICROSTEP_M0_PIN, MICROSTEP_M1_PIN, MICROSTEP_M2_PIN));
  microstep_gpio_->begin();  // Sets 1/32 microstepping mode by default
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
      if (fas_->isMoving(i))
        return true;
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
  if (delta >= 0) {
    dir_bits_ |= (1u << i);
  } else {
    dir_bits_ &= (uint8_t)~(1u << i);
  }
  sleep_bits_ |= (1u << i);
  latch_();
#endif
  (void)fas_->startMoveAbs(i, target, speed, accel);
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
  if (isAnyMovingForMask(mask))
    return false;
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

void HardwareMotorController::setSettleParams(uint32_t mask, int overshoot,
                                               int dither_amplitude, int dither_cycles,
                                               int dither_min_amplitude) {
  for (uint8_t i = 0; i < count_; ++i) {
    if (mask & maskForId(i)) {
      motors_[i].settle_overshoot = overshoot;
      motors_[i].settle_dither_amplitude = dither_amplitude;
      motors_[i].settle_dither_cycles = dither_cycles;
      motors_[i].settle_dither_min_amplitude = dither_min_amplitude;
    }
  }
}

bool HardwareMotorController::moveAbsMask(
    uint32_t mask, long target, int speed, int accel, uint32_t now_ms) {
  // Busy if any selected motor is already running
  if (isAnyMovingForMask(mask))
    return false;

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
      if (delta >= 0) {
        dir_bits_ |= (1u << i);
      } else {
        dir_bits_ &= (uint8_t)~(1u << i);
      }
      sleep_bits_ |= (1u << i);
#endif
      // Record last op timing
      long dist = (target > cur) ? (target - cur) : (cur - target);
      uint32_t est = 0;
#if (USE_SHARED_STEP)
      est = MotionKinematics::estimateMoveTimeMsSharedStep(dist, speed, accel, decel_sps2_);
#else
      est = MotionKinematics::estimateMoveTimeMs(dist, speed, accel);
#endif
      motors_[i].last_op_type = 1;
      motors_[i].last_op_started_ms = now_ms;
      motors_[i].last_op_est_ms = est;
      motors_[i].last_op_ongoing = true;
      // Initialize settle state from pre-populated params (set by setSettleParams)
      if (motors_[i].settle_overshoot != 0 || motors_[i].settle_dither_amplitude > 0) {
        motors_[i].settle_center = target;
        motors_[i].settle_speed = speed;
        motors_[i].settle_accel = accel;
        motors_[i].settle_remaining_cycles = motors_[i].settle_dither_cycles;
        motors_[i].settle_current_amplitude = motors_[i].settle_dither_amplitude;
        if (motors_[i].settle_overshoot != 0) {
          motors_[i].move_phase = MovePhase::OVERSHOOT;
        } else {
          motors_[i].move_phase = MovePhase::PRIMARY;
        }
      } else {
        motors_[i].move_phase = MovePhase::NONE;
      }
    }
  }

#if !defined(ARDUINO)
  latch_();
#endif
  // Start steppers — per-motor target may differ when overshoot is active
  bool ok = true;
  for (uint8_t i = 0; i < count_; ++i) {
    if (mask & maskForId(i)) {
      long initial_target = target;
      if (motors_[i].move_phase == MovePhase::OVERSHOOT) {
        // Go directly to overshoot position: target - overshoot
        initial_target = target - motors_[i].settle_overshoot;
      }
      if (!fas_->startMoveAbs(i, initial_target, speed, accel))
        ok = false;
    }
  }
  return ok;
}

bool HardwareMotorController::homeMask(uint32_t mask,
                                       long overshoot,
                                       long backoff,
                                       int speed,
                                       int accel,
                                       long full_range,
                                       uint32_t now_ms) {
  // Reject if any targeted motor is currently busy
  if (isAnyMovingForMask(mask))
    return false;

  // Derive defaults as needed
  if (full_range <= 0)
    full_range = 2400;  // kMaxPos - kMinPos by convention
  long oshot = (overshoot < 0) ? -overshoot : overshoot;
  long bko = (backoff < 0) ? -backoff : backoff;

  // Prepare DIR/SLEEP bits first for all targets, then latch once before starts (native)
  for (uint8_t i = 0; i < count_; ++i) {
    if (mask & maskForId(i)) {
      // Initialize homing plan for motor i
      homing_[i] = HomingPlan{true, 0, oshot, bko, full_range, speed, accel};
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
      if (delta >= 0) {
        dir_bits_ |= (1u << i);
      } else {
        dir_bits_ &= (uint8_t)~(1u << i);
      }
      sleep_bits_ |= (1u << i);
#endif
      // Record last op timing (entire HOME sequence estimate)
      uint32_t est = 0;
#if (USE_SHARED_STEP)
      est = MotionKinematics::estimateHomeTimeMsWithFullRangeSharedStep(
          overshoot, backoff, full_range, speed, accel, decel_sps2_);
#else
      est = MotionKinematics::estimateHomeTimeMsWithFullRange(
          overshoot, backoff, full_range, speed, accel);
#endif
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
      (void)fas_->startMoveAbs(i, target, speed, accel);
    }
  }
  return true;
}


void HardwareMotorController::tick(uint32_t now_ms) {
  // Flush any ISR-deferred shift register updates (SPI not safe in ISR)
  fas_->pollLatch();

  // Pull runtime state from adapter; awake reflects running or WAKE override
  for (uint8_t i = 0; i < count_; ++i) {
    // Thermal budget bookkeeping with sub-second accumulator to prevent drift
    if (now_ms >= motors_[i].last_update_ms) {
      uint32_t dt_ms = now_ms - motors_[i].last_update_ms;
      motors_[i].last_update_ms = now_ms;  // Always update timestamp

      // Accumulate milliseconds and apply budget changes at 100ms granularity (1 tenth)
      uint32_t total_ms = motors_[i].budget_accum_ms + dt_ms;
      uint32_t tenths = total_ms / 100;  // Each tenth = 100ms
      motors_[i].budget_accum_ms = static_cast<uint16_t>(total_ms % 100);

      if (tenths > 0) {
        const int32_t kBudgetFloor =
            (int32_t)(MotorControlConstants::BUDGET_TENTHS_MAX -
                      MotorControlConstants::REFILL_TENTHS_PER_SEC *
                          (int32_t)MotorControlConstants::MAX_COOL_DOWN_TIME_S);
        if (motors_[i].awake) {
          // Spend: 10 tenths per second = 1 tenth per 100ms
          motors_[i].budget_tenths -= (int32_t)tenths;
          if (motors_[i].budget_tenths < kBudgetFloor)
            motors_[i].budget_tenths = kBudgetFloor;
        } else {
          // Refill: 1.5 tenths per second = 0.15 tenths per 100ms
          // Apply per-second rate scaled: (tenths * REFILL_TENTHS_PER_SEC) / 10
          int32_t refill = ((int32_t)tenths * MotorControlConstants::REFILL_TENTHS_PER_SEC) / 10;
          if (refill < 1 && tenths > 0)
            refill = 1;  // Ensure at least some refill for long intervals
          motors_[i].budget_tenths += refill;
          if (motors_[i].budget_tenths > MotorControlConstants::BUDGET_TENTHS_MAX)
            motors_[i].budget_tenths = MotorControlConstants::BUDGET_TENTHS_MAX;
        }
      }
    }

    // Position tracking (no critical section needed - reads are atomic on ESP32)
    bool running = fas_->isMoving(i);
    motors_[i].moving = running;
    long pos = fas_->currentPosition(i);
    long old_pos = motors_[i].position;
    if (motors_[i].homed && pos != old_pos) {
      long d = pos - old_pos;
      if (d < 0)
        d = -d;
      motors_[i].steps_since_home += (int32_t)d;
    }
    motors_[i].position = pos;
#if defined(ARDUINO)
    bool should_be_awake = running || ((forced_awake_mask_ & (1u << i)) != 0) ||
                           (motors_[i].move_phase != MovePhase::NONE);
    if (motors_[i].awake && !should_be_awake) {
      fas_->disableOutputs(i);
      fas_->setAutoEnable(i, true);
    }
    motors_[i].awake = should_be_awake;
#else
    // Auto-sleep at idle in native mode to validate latch behavior
    if (!running && (forced_awake_mask_ & (1u << i)) == 0 &&
        motors_[i].move_phase == MovePhase::NONE) {
      if (sleep_bits_ & (1u << i)) {
        sleep_bits_ &= (uint8_t)~(1u << i);
        latch_();
      }
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
        // Stop motion first to prevent auto-enable fighting with disable
        fas_->forceStop(i);
        fas_->disableOutputs(i);
#if defined(ARDUINO)
        fas_->setAutoEnable(i, true);
#else
        if (sleep_bits_ & (1u << i)) {
          sleep_bits_ &= (uint8_t)~(1u << i);
          latch_();
        }
#endif
        motors_[i].awake = false;
        // Stop homing/settle plans if active; mark operation complete
        if (homing_[i].active || motors_[i].moving ||
            motors_[i].move_phase != MovePhase::NONE) {
          homing_[i].active = false;
          motors_[i].moving = false;
          motors_[i].move_phase = MovePhase::NONE;
          if (motors_[i].last_op_ongoing) {
            motors_[i].last_op_ongoing = false;
            if (motors_[i].last_op_started_ms != 0 && now_ms >= motors_[i].last_op_started_ms) {
              motors_[i].last_op_last_ms = now_ms - motors_[i].last_op_started_ms;
            }
          }
        }
      }
    }

    // Homing sequence state machine handled via group barriers below
    // Record completion time for last op when no longer moving, no active homing, and settle done
    if (motors_[i].last_op_ongoing && !motors_[i].moving && !homing_[i].active &&
        motors_[i].move_phase == MovePhase::NONE) {
      motors_[i].last_op_ongoing = false;
      if (motors_[i].last_op_started_ms != 0) {
        motors_[i].last_op_last_ms = now_ms - motors_[i].last_op_started_ms;
      }
    }
  }
  // Group barrier for HOME legs: start next leg only when all motors in that leg have finished
  for (uint8_t phase = 0; phase <= 1; ++phase) {
    bool any = false;
    bool all_done = true;
    for (uint8_t i = 0; i < count_; ++i) {
      if (homing_[i].active && homing_[i].phase == phase) {
        any = true;
        if (motors_[i].moving) {
          all_done = false;
          break;
        }
      }
    }
    if (any && all_done) {
      for (uint8_t i = 0; i < count_; ++i) {
        if (!(homing_[i].active && homing_[i].phase == phase))
          continue;
        HomingPlan& hp = homing_[i];
        if (phase == 0) {
          long cur = fas_->currentPosition(i);
          long target = cur + hp.backoff;
          startMoveSingle_(i, target, hp.speed, hp.accel);
          hp.phase = 1;
        } else if (phase == 1) {
          long cur = fas_->currentPosition(i);
          long mid_delta = (hp.full_range > 0) ? (hp.full_range / 2) : 1200;
          long target = cur + mid_delta;
          startMoveSingle_(i, target, hp.speed, hp.accel);
          hp.phase = 2;
        }
      }
    }
  }
  // Finalize homing after last leg complete
  for (uint8_t i = 0; i < count_; ++i) {
    if (homing_[i].active && homing_[i].phase == 2 && !motors_[i].moving) {
      fas_->setCurrentPosition(i, 0);
      motors_[i].position = 0;
      motors_[i].moving = false;
      motors_[i].homed = true;
      motors_[i].steps_since_home = 0;
      homing_[i].active = false;
      if (motors_[i].last_op_ongoing) {
        motors_[i].last_op_ongoing = false;
        if (motors_[i].last_op_started_ms != 0 && now_ms >= motors_[i].last_op_started_ms) {
          motors_[i].last_op_last_ms = now_ms - motors_[i].last_op_started_ms;
        }
      }
    }
  }
  // Settle sequence state machine (overshoot + dither for MOVE commands)
  for (uint8_t i = 0; i < count_; ++i) {
    if (motors_[i].move_phase == MovePhase::NONE)
      continue;
    if (fas_->isMoving(i))
      continue;  // Current phase leg still running

    switch (motors_[i].move_phase) {
    case MovePhase::PRIMARY: {
      // Primary move complete (dither-only path, no overshoot)
      if (motors_[i].settle_dither_amplitude > 0 &&
          motors_[i].settle_remaining_cycles > 0) {
        int total = motors_[i].settle_dither_cycles;
        int remaining = motors_[i].settle_remaining_cycles;
        int amp = motors_[i].settle_dither_amplitude * remaining / total;
        if (amp >= motors_[i].settle_dither_min_amplitude) {
          motors_[i].settle_current_amplitude = amp;
          motors_[i].settle_remaining_cycles--;
          long pos_target = motors_[i].settle_center + amp;
          startMoveSingle_(i, pos_target, motors_[i].settle_speed, motors_[i].settle_accel);
          motors_[i].move_phase = MovePhase::DITHER_POS;
        } else {
          motors_[i].move_phase = MovePhase::NONE;
        }
      } else {
        motors_[i].move_phase = MovePhase::NONE;
      }
      break;
    }
    case MovePhase::OVERSHOOT: {
      // Overshoot complete; return to center (approach)
      startMoveSingle_(i, motors_[i].settle_center, motors_[i].settle_speed,
                        motors_[i].settle_accel);
      motors_[i].move_phase = MovePhase::APPROACH;
      break;
    }
    case MovePhase::APPROACH: {
      // Approach complete; start dither if configured
      if (motors_[i].settle_dither_amplitude > 0 &&
          motors_[i].settle_remaining_cycles > 0) {
        int total = motors_[i].settle_dither_cycles;
        int remaining = motors_[i].settle_remaining_cycles;
        int amp = motors_[i].settle_dither_amplitude * remaining / total;
        if (amp >= motors_[i].settle_dither_min_amplitude) {
          motors_[i].settle_current_amplitude = amp;
          motors_[i].settle_remaining_cycles--;
          long pos_target = motors_[i].settle_center + amp;
          startMoveSingle_(i, pos_target, motors_[i].settle_speed, motors_[i].settle_accel);
          motors_[i].move_phase = MovePhase::DITHER_POS;
        } else {
          motors_[i].move_phase = MovePhase::NONE;
        }
      } else {
        motors_[i].move_phase = MovePhase::NONE;
      }
      break;
    }
    case MovePhase::DITHER_POS: {
      // Positive dither complete; swing to negative side
      long neg_target = motors_[i].settle_center - motors_[i].settle_current_amplitude;
      startMoveSingle_(i, neg_target, motors_[i].settle_speed, motors_[i].settle_accel);
      motors_[i].move_phase = MovePhase::DITHER_NEG;
      break;
    }
    case MovePhase::DITHER_NEG: {
      // Negative dither complete; check for next cycle or return to center
      if (motors_[i].settle_remaining_cycles > 0) {
        int total = motors_[i].settle_dither_cycles;
        int remaining = motors_[i].settle_remaining_cycles;
        int amp = motors_[i].settle_dither_amplitude * remaining / total;
        if (amp >= motors_[i].settle_dither_min_amplitude) {
          motors_[i].settle_current_amplitude = amp;
          motors_[i].settle_remaining_cycles--;
          long pos_target = motors_[i].settle_center + amp;
          startMoveSingle_(i, pos_target, motors_[i].settle_speed, motors_[i].settle_accel);
          motors_[i].move_phase = MovePhase::DITHER_POS;
          break;
        }
      }
      // All cycles done or amplitude below threshold; return to center
      startMoveSingle_(i, motors_[i].settle_center, motors_[i].settle_speed,
                        motors_[i].settle_accel);
      motors_[i].move_phase = MovePhase::DITHER_RETURN;
      break;
    }
    case MovePhase::DITHER_RETURN: {
      // Final return to center complete; settle sequence done
      motors_[i].move_phase = MovePhase::NONE;
      break;
    }
    default:
      break;
    }
  }
  // Native: start/stop latches handled above; Arduino: adapter handles gating
}

void HardwareMotorController::setDeceleration(int decel_sps2) {
#if defined(ARDUINO)
  if (fas_)
    fas_->setDeceleration(decel_sps2);
#else
  (void)decel_sps2;
#endif
  decel_sps2_ = decel_sps2;
}

void HardwareMotorController::setMicrostepMode(MicrostepMode mode) {
#if defined(ARDUINO)
  if (microstep_gpio_) {
    uint8_t old_mult = microstep_gpio_->multiplier();
    microstep_gpio_->setMode(mode);
    uint8_t new_mult = microstep_gpio_->multiplier();

    // Rescale all motor positions when mode changes
    if (old_mult != new_mult) {
      for (uint8_t i = 0; i < count_; ++i) {
        // Rescale with rounding: new_hw_pos = round(hw_pos * new_mult / old_mult)
        // Use integer math with half-up rounding: (a * new + old/2) / old
        long hw_pos = motors_[i].position;
        long new_hw_pos =
            (hw_pos * static_cast<long>(new_mult) + old_mult / 2) / static_cast<long>(old_mult);
        motors_[i].position = new_hw_pos;
        if (fas_) {
          fas_->setCurrentPosition(i, new_hw_pos);
        }
      }
    }
  }
#else
  (void)mode;
#endif
}
