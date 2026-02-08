#include "StubMotorController.h"

#include "MotorControl/MotionKinematics.h"
#include "MotorControl/MotorControlConstants.h"

#include <math.h>

static uint32_t maskForId(uint8_t id) {
  return 1u << id;
}

StubMotorController::StubMotorController(uint8_t count) : count_(count) {
  if (count_ > 8)
    count_ = 8;
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
    plans_[i] = MovePlan{false, false, 0, 0, 0};
  }
}

bool StubMotorController::isAnyMovingForMask(uint32_t mask) const {
  for (uint8_t i = 0; i < count_; ++i) {
    if (mask & maskForId(i)) {
      if (motors_[i].moving)
        return true;
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
      if (motors_[i].moving)
        return false;
    }
  }
  for (uint8_t i = 0; i < count_; ++i) {
    if (mask & maskForId(i)) {
      motors_[i].awake = false;
    }
  }
  return true;
}

void StubMotorController::setSettleParams(uint32_t mask, int overshoot,
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

bool StubMotorController::moveAbsMask(
    uint32_t mask, long target, int speed, int accel, uint32_t now_ms) {
  if (isAnyMovingForMask(mask))
    return false;
  for (uint8_t i = 0; i < count_; ++i) {
    if (mask & maskForId(i)) {
      motors_[i].awake = true;
      motors_[i].speed = speed;
      motors_[i].accel = accel;
      motors_[i].moving = true;
      long delta = labs(target - motors_[i].position);
      uint32_t dur_ms = MotionKinematics::estimateMoveTimeMs(delta, speed, accel);
      // Add settle phase durations (overshoot + dither)
      uint32_t settle_dur = 0;
      if (motors_[i].settle_overshoot != 0) {
        // Actual path: current → (target - overshoot) → target
        long overshoot_pos = target - motors_[i].settle_overshoot;
        long leg1 = labs(overshoot_pos - motors_[i].position);
        long leg2 = labs(motors_[i].settle_overshoot);
        dur_ms = MotionKinematics::estimateMoveTimeMs(leg1, speed, accel);
        settle_dur += MotionKinematics::estimateMoveTimeMs(leg2, speed, accel);
      }
      if (motors_[i].settle_dither_amplitude > 0 && motors_[i].settle_dither_cycles > 0) {
        int total_cycles = motors_[i].settle_dither_cycles;
        int last_amp = 0;
        int prev_amp = 0;
        for (int ci = 1; ci <= total_cycles; ++ci) {
          int amp_i = motors_[i].settle_dither_amplitude * (total_cycles - ci + 1) / total_cycles;
          if (amp_i < motors_[i].settle_dither_min_amplitude)
            break;
          last_amp = amp_i;
          // POS: from -prev_amp to +amp_i (first cycle starts from center)
          settle_dur += MotionKinematics::estimateMoveTimeMs(prev_amp + amp_i, speed, accel);
          // NEG: from +amp_i to -amp_i
          settle_dur += MotionKinematics::estimateMoveTimeMs(2 * amp_i, speed, accel);
          prev_amp = amp_i;
        }
        if (last_amp > 0) {
          settle_dur += MotionKinematics::estimateMoveTimeMs(last_amp, speed, accel);
        }
      }
      plans_[i].active = true;
      plans_[i].is_home = false;
      plans_[i].target = target;
      plans_[i].start_pos = motors_[i].position;
      plans_[i].end_ms = now_ms + dur_ms + settle_dur;
      motors_[i].last_op_type = 1;
      motors_[i].last_op_started_ms = now_ms;
      motors_[i].last_op_est_ms = dur_ms;
      motors_[i].last_op_ongoing = true;
    }
  }
  return true;
}

bool StubMotorController::homeMask(uint32_t mask,
                                   long overshoot,
                                   long backoff,
                                   int speed,
                                   int accel,
                                   long /*full_range*/,
                                   uint32_t now_ms) {
  if (isAnyMovingForMask(mask))
    return false;
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
  // Budget bookkeeping (tenths of seconds, 100ms granularity)

  for (uint8_t i = 0; i < count_; ++i) {
    // Update budget based on awake state; accumulate at 100ms (1 tenth) granularity
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
          // Spend budget: 10 tenths per second = 1 tenth per 100ms
          motors_[i].budget_tenths -= (int32_t)tenths;
          // Clamp minimum so max cooldown is bounded
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

    // Auto-sleep overrun handling (runtime enforcement)
    if (thermal_limits_enabled_) {
      const int32_t overrun_tenths = -MotorControlConstants::AUTO_SLEEP_IF_OVER_BUDGET_S * 10;
      if (motors_[i].budget_tenths < overrun_tenths) {
        // Force sleep and cancel any active plan
        motors_[i].awake = false;
        if (plans_[i].active || motors_[i].moving ||
            motors_[i].move_phase != MovePhase::NONE) {
          motors_[i].moving = false;
          plans_[i].active = false;
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

void StubMotorController::setMicrostepMode(MicrostepMode mode) {
  uint8_t old_mult = microstep_multiplier_;
  uint8_t new_mult = MicrostepModeToMultiplier(mode);

  // Rescale all motor positions when mode changes
  if (old_mult != new_mult) {
    for (uint8_t i = 0; i < count_; ++i) {
      // Rescale with rounding: new_hw_pos = round(hw_pos * new_mult / old_mult)
      // Use integer math with half-up rounding: (a * new + old/2) / old
      long hw_pos = motors_[i].position;
      long new_hw_pos =
          (hw_pos * static_cast<long>(new_mult) + old_mult / 2) / static_cast<long>(old_mult);
      motors_[i].position = new_hw_pos;
    }
    microstep_multiplier_ = new_mult;
  }
}
