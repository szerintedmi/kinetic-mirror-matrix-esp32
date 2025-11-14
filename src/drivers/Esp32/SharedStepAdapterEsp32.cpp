#if defined(ARDUINO)

#include "drivers/Esp32/SharedStepAdapterEsp32.h"
using namespace SharedStepTiming;
using namespace SharedStepGuards;

namespace {
constexpr uint32_t kImmediateFlipDelayUs = 3;
}

// ISR hook invoked by the shared STEP generator
void IRAM_ATTR SharedStepAdapterEsp32::onRisingEdgeHook_(void* context) {
  if (context == nullptr) {
    return;
  }
  auto* self = static_cast<SharedStepAdapterEsp32*>(context);
  self->phase_anchor_us_ = micros();
}

SharedStepAdapterEsp32::SharedStepAdapterEsp32() = default;

void SharedStepAdapterEsp32::begin() {
  gen_.begin();
  // Attach rising-edge hook to align guard scheduling to generator edges
  gen_.setEdgeHook(&SharedStepAdapterEsp32::onRisingEdgeHook_, this);
  gen_running_ = false;
  current_speed_sps_ = 0;
  period_us_ = 0;
  dir_bits_ = 0;
  sleep_bits_ = 0;
  for (uint8_t i = 0; i < kMotorSlots; ++i) {
    slots_[i] = Slot{};
    flips_[i] = FlipSchedule{};
  }
  v_cur_sps_ = 0;
  v_max_sps_ = 0;
  a_sps2_ = 0;
  d_sps2_ = 0;
  dv_accum_ = 0;
  ramp_last_us_ = micros();
}

void SharedStepAdapterEsp32::maybeStartGen_(int speed_sps) const {
  uint32_t speed_setting =
      static_cast<uint32_t>((speed_sps > 0) ? speed_sps : static_cast<int>(kMinGenSps));
  if (speed_setting < kMinGenSps) {
    speed_setting = kMinGenSps;
  }

  const bool need_speed_update =
      !gen_running_ || (speed_setting != static_cast<uint32_t>(current_speed_sps_));
  if (!need_speed_update) {
    return;
  }

  gen_.setSpeed(speed_setting);
  if (!gen_running_) {
    gen_.start();
    gen_running_ = true;
  }
  current_speed_sps_ = static_cast<int>(speed_setting);
  period_us_ = step_period_us(speed_setting);
  phase_anchor_us_ = micros();
}

void SharedStepAdapterEsp32::maybeStopGen_() const {
  for (uint8_t i = 0; i < kMotorSlots; ++i) {
    if (slots_[i].moving) {
      return;
    }
  }
  if (gen_running_) {
    gen_.stop();
    gen_running_ = false;
  }
}

void SharedStepAdapterEsp32::flushLatch_() const {
  if (shift_ != nullptr) {
    shift_->setDirSleep(dir_bits_, sleep_bits_);
  }
}

void SharedStepAdapterEsp32::setDirectionBit_(uint8_t motor_id, int dir_sign)
    const {  // NOLINT(readability-convert-member-functions-to-static)
  const uint8_t mask = static_cast<uint8_t>(1u << motor_id);
  if (dir_sign > 0) {
    this->dir_bits_ |= mask;
    return;
  }
  this->dir_bits_ &= static_cast<uint8_t>(~mask);
}

bool SharedStepAdapterEsp32::computeMotionWindow_(
    uint32_t* min_remaining) const {  // NOLINT(readability-convert-member-functions-to-static)
  bool any_moving = false;
  uint32_t local_min = UINT_MAX;
  for (uint8_t motor_index = 0; motor_index < kMotorSlots; ++motor_index) {
    const Slot& slot = this->slots_[motor_index];
    if (!slot.moving) {
      continue;
    }
    any_moving = true;
    long remaining = slot.target - slot.pos;
    if (remaining < 0) {
      remaining = -remaining;
    }
    const uint32_t remaining_u32 = static_cast<uint32_t>(remaining);
    if (remaining_u32 < local_min) {
      local_min = remaining_u32;
    }
  }
  if (any_moving && min_remaining != nullptr) {
    *min_remaining = local_min;
  }
  return any_moving;
}

void SharedStepAdapterEsp32::enableOutputs(uint8_t motor_id) {
  if (motor_id >= kMotorSlots) {
    return;
  }
  slots_[motor_id].awake = true;
  slots_[motor_id].forced_awake = true;
  sleep_bits_ |= (1u << motor_id);
  flushLatch_();
}

void SharedStepAdapterEsp32::disableOutputs(uint8_t motor_id) {
  if (motor_id >= kMotorSlots) {
    return;
  }
  slots_[motor_id].awake = false;
  slots_[motor_id].forced_awake = false;
  sleep_bits_ &= (uint8_t)~(1u << motor_id);
  flushLatch_();
}

bool SharedStepAdapterEsp32::startMoveAbs(uint8_t motor_id, long target, int speed, int accel) {
  if (motor_id >= kMotorSlots) {
    return false;
  }
  Slot& slot = slots_[motor_id];
  long delta = target - slot.pos;
  if (delta == 0) {
    slot.moving = false;
    return true;
  }
  int desired_dir = (delta > 0) ? +1 : -1;
  int cur_dir_sign = (dir_bits_ & (1u << motor_id)) ? +1 : -1;
  bool need_flip = (desired_dir != cur_dir_sign);
  slot.target = target;
  slot.speed_sps = speed > 0 ? speed : 1;
  // Update global ramp targets
  v_max_sps_ = slot.speed_sps;
  a_sps2_ = (accel > 0) ? accel : 1;
  // If direction must change, force SLEEP low before scheduling flip
  if (need_flip) {
    sleep_bits_ &= (uint8_t)~(1u << motor_id);
    flushLatch_();
  }

  // Decide safe approach: if generator is running, schedule a guarded flip in the next safe gap.
  // Otherwise, perform immediate safe sequence before starting pulses.
  FlipSchedule& flip_schedule = flips_[motor_id];
  flip_schedule.active = false;
  flip_schedule.phase = 0;
  flip_schedule.new_dir_sign = desired_dir;
  uint32_t now_us = micros();
  if (need_flip && gen_running_ && period_us_ > 0) {
    DirFlipWindow flip_window{};
    uint64_t now_rel = (now_us >= phase_anchor_us_) ? (now_us - phase_anchor_us_) : 0;
    if (compute_flip_window(FlipWindowRequest(now_rel, period_us_), flip_window)) {
      flip_schedule.w.t_sleep_low = flip_window.t_sleep_low + phase_anchor_us_;
      flip_schedule.w.t_dir_flip = flip_window.t_dir_flip + phase_anchor_us_;
      flip_schedule.w.t_sleep_high = flip_window.t_sleep_high + phase_anchor_us_;
      flip_schedule.phase = 0;
      flip_schedule.active = true;
    }
  }
  if (!flip_schedule.active) {
    if (need_flip) {
      // Immediate safe flip while generator is idle (or as a fallback)
      setDirectionBit_(motor_id, desired_dir);
      flushLatch_();
      delayMicroseconds(kImmediateFlipDelayUs);
    }
    // Enable output without delay if direction already matches (no hazard)
    sleep_bits_ |= (1u << motor_id);
    flushLatch_();
    slots_[motor_id].awake = true;
  }
  // Start move bookkeeping; s.awake becomes true after SLEEP high phase
  slot.dir_sign = desired_dir;
  slot.moving = true;
  if (flip_schedule.active) {
    slot.awake = false;  // will set true after guard window
  }
  slot.last_us = micros();
  slot.acc_us = 0;
  // Ensure generator is running and ramp state initialized/updated
  if (!gen_running_) {
    // Start at a conservative speed for valid RMT durations, ramp from there
    v_cur_sps_ = (int)kMinGenSps;
    maybeStartGen_(v_cur_sps_);
    ramp_last_us_ = micros();
  } else {
    // Update anchor on new intent to keep guard scheduling aligned
    phase_anchor_us_ = micros();
  }
  return true;
}

void SharedStepAdapterEsp32::updateProgress_(uint8_t motor_id) const {
  Slot& slot = const_cast<Slot&>(slots_[motor_id]);
  if (!slot.moving) {
    return;
  }
  uint32_t now_us = micros();
  uint32_t dt_us = now_us - slot.last_us;
  slot.last_us = now_us;
  // Handle any pending guarded flip/gating for this motor
  runFlipScheduler_(motor_id, now_us);
  // Integrate position using global generator speed (shared STEP)
  if (slot.awake && current_speed_sps_ > 0) {
    uint64_t total = (uint64_t)dt_us * (uint64_t)current_speed_sps_ + (uint64_t)slot.acc_us;
    uint32_t steps = (uint32_t)(total / 1000000ULL);
    slot.acc_us = (uint32_t)(total % 1000000ULL);
    if (steps > 0) {
      long new_pos = slot.pos + (long)(slot.dir_sign * (int)steps);
      if (slot.dir_sign > 0) {
        if (new_pos >= slot.target) {
          new_pos = slot.target;
          slot.moving = false;
        }
      } else {
        if (new_pos <= slot.target) {
          new_pos = slot.target;
          slot.moving = false;
        }
      }
      slot.pos = new_pos;
    }
  }
  // Update global ramp after integrating at least one motor
  updateRamp_(now_us);
  if (!slot.moving) {
    // If no slots moving, stop generator
    if (!slot.forced_awake) {
      // Auto-sleep this motor when it finishes
      sleep_bits_ &= (uint8_t)~(1u << motor_id);
      flushLatch_();
      slot.awake = false;
    }
    maybeStopGen_();
  }
}

void SharedStepAdapterEsp32::updateRamp_(uint32_t now_us) const {
  uint32_t min_remaining = UINT_MAX;
  if (!computeMotionWindow_(&min_remaining)) {
    if (gen_running_) {
      gen_.stop();
      gen_running_ = false;
    }
    v_cur_sps_ = 0;
    current_speed_sps_ = 0;
    period_us_ = 0;
    dv_accum_ = 0;
    ramp_last_us_ = now_us;
    return;
  }

  const uint32_t elapsed_us = (now_us >= ramp_last_us_) ? (now_us - ramp_last_us_) : 0U;
  if (elapsed_us == 0U) {
    return;
  }
  ramp_last_us_ = now_us;

  const uint32_t accel_up = (a_sps2_ > 0) ? static_cast<uint32_t>(a_sps2_) : 1U;
  const uint32_t decel_down = (d_sps2_ > 0) ? static_cast<uint32_t>(d_sps2_) : 0U;
  const uint32_t current_speed = (v_cur_sps_ > 0) ? static_cast<uint32_t>(v_cur_sps_) : 0U;
  const uint32_t target_speed =
      (v_max_sps_ > 0) ? static_cast<uint32_t>(v_max_sps_) : static_cast<uint32_t>(kMinGenSps);

  uint32_t stopping_distance = 0;
  if (decel_down > 0U) {
    stopping_distance = ceil_div_u32(DivisionOperands(static_cast<uint64_t>(current_speed) *
                                                          static_cast<uint64_t>(current_speed),
                                                      2ULL * static_cast<uint64_t>(decel_down)));
  }

  const bool need_decel =
      (decel_down > 0U) && ((min_remaining <= stopping_distance) || (current_speed > target_speed));
  const bool need_accel = (!need_decel) && (current_speed < target_speed);

  const uint32_t accel_effective = need_accel ? accel_up : (need_decel ? decel_down : 0U);
  const uint64_t dv_total =
      static_cast<uint64_t>(accel_effective) * static_cast<uint64_t>(elapsed_us) +
      static_cast<uint64_t>(dv_accum_);
  const uint32_t delta_velocity = static_cast<uint32_t>(dv_total / 1000000ULL);
  dv_accum_ = static_cast<uint32_t>(dv_total % 1000000ULL);

  int new_speed = static_cast<int>(current_speed);
  if (need_accel && delta_velocity > 0U) {
    new_speed += static_cast<int>(delta_velocity);
    if (new_speed > static_cast<int>(target_speed)) {
      new_speed = static_cast<int>(target_speed);
    }
  } else if (need_decel && delta_velocity > 0U) {
    new_speed -= static_cast<int>(delta_velocity);
    if (new_speed < 0) {
      new_speed = 0;
    }
  }

  int commanded_speed = new_speed;
  if (commanded_speed > 0 && commanded_speed < static_cast<int>(kMinGenSps)) {
    commanded_speed = static_cast<int>(kMinGenSps);
  }

  if (!gen_running_ || commanded_speed != current_speed_sps_) {
    maybeStartGen_(commanded_speed);
  }
  v_cur_sps_ = new_speed;
}

void SharedStepAdapterEsp32::runFlipScheduler_(uint8_t motor_id, uint32_t now_us) const {
  FlipSchedule& schedule = const_cast<FlipSchedule&>(flips_[motor_id]);
  if (!schedule.active) {
    return;
  }
  if (period_us_ == 0) {
    schedule.active = false;
    return;
  }
  if (now_us >= schedule.w.t_sleep_high) {
    setDirectionBit_(motor_id, schedule.new_dir_sign);
    sleep_bits_ |= (1u << motor_id);
    flushLatch_();
    slots_[motor_id].awake = true;
    schedule.active = false;
    return;
  }
  if (schedule.phase == 0) {
    if (now_us >= schedule.w.t_sleep_low) {
      sleep_bits_ &= (uint8_t)~(1u << motor_id);
      flushLatch_();
      schedule.phase = 1;
    } else {
      return;
    }
  }
  if (schedule.phase == 1) {
    if (now_us >= schedule.w.t_dir_flip) {
      setDirectionBit_(motor_id, schedule.new_dir_sign);
      flushLatch_();
      schedule.phase = 2;
    } else {
      return;
    }
  }
  if (schedule.phase == 2 && now_us >= schedule.w.t_sleep_high) {
    sleep_bits_ |= (1u << motor_id);
    flushLatch_();
    slots_[motor_id].awake = true;
    schedule.active = false;
  }
}

bool SharedStepAdapterEsp32::isMoving(
    uint8_t motor_id) const {  // NOLINT(readability-convert-member-functions-to-static)
  if (motor_id >= kMotorSlots) {
    return false;
  }
  this->updateProgress_(motor_id);
  return this->slots_[motor_id].moving;
}

long SharedStepAdapterEsp32::currentPosition(
    uint8_t motor_id) const {  // NOLINT(readability-convert-member-functions-to-static)
  if (motor_id >= kMotorSlots) {
    return 0;
  }
  this->updateProgress_(motor_id);
  return this->slots_[motor_id].pos;
}

void SharedStepAdapterEsp32::setCurrentPosition(
    uint8_t motor_id, long pos) {  // NOLINT(readability-convert-member-functions-to-static)
  if (motor_id >= kMotorSlots) {
    return;
  }
  Slot& slot = this->slots_[motor_id];
  slot.pos = pos;
}

// Debug helpers removed after stabilization

#endif  // ARDUINO
