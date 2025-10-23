#if defined(ARDUINO)

#include "drivers/Esp32/SharedStepAdapterEsp32.h"
using namespace SharedStepTiming;
using namespace SharedStepGuards;

// Static storage for ISR hook
SharedStepAdapterEsp32* SharedStepAdapterEsp32::self_ = nullptr;
void IRAM_ATTR SharedStepAdapterEsp32::onRisingEdgeHook_() {
  if (self_) {
    self_->phase_anchor_us_ = micros();
  }
}

SharedStepAdapterEsp32::SharedStepAdapterEsp32() {}

void SharedStepAdapterEsp32::begin() {
  gen_.begin();
  // Attach rising-edge hook to align guard scheduling to generator edges
  self_ = this;
  gen_.setEdgeHook(&SharedStepAdapterEsp32::onRisingEdgeHook_);
  gen_running_ = false;
  current_speed_sps_ = 0;
  period_us_ = 0;
  dir_bits_ = 0;
  sleep_bits_ = 0;
  for (uint8_t i = 0; i < 8; ++i) { slots_[i] = Slot{}; flips_[i] = FlipSchedule{}; }
  v_cur_sps_ = 0;
  v_max_sps_ = 0;
  a_sps2_ = 0;
  d_sps2_ = 0;
  dv_accum_ = 0;
  ramp_last_us_ = micros();
}

void SharedStepAdapterEsp32::maybeStartGen_(int speed_sps) const {
  if (!gen_running_) {
    uint32_t sp = (uint32_t)((speed_sps > 0) ? speed_sps : (int)kMinGenSps);
    if (sp < kMinGenSps) sp = kMinGenSps;
    gen_.setSpeed(sp);
    gen_.start();
    gen_running_ = true;
    current_speed_sps_ = (int)sp;
    period_us_ = step_period_us(sp);
    phase_anchor_us_ = micros();
    
  } else {
    if (speed_sps != current_speed_sps_) {
      uint32_t sp = (uint32_t)((speed_sps > 0) ? speed_sps : (int)kMinGenSps);
      if (sp < kMinGenSps) sp = kMinGenSps;
      gen_.setSpeed(sp);
      current_speed_sps_ = (int)sp;
      period_us_ = step_period_us(sp);
      phase_anchor_us_ = micros();
      
    }
  }
}

void SharedStepAdapterEsp32::maybeStopGen_() const {
  for (uint8_t i = 0; i < 8; ++i) {
    if (slots_[i].moving) return;
  }
  if (gen_running_) {
    gen_.stop();
    gen_running_ = false;
  }
}

void SharedStepAdapterEsp32::flushLatch_() const {
  if (shift_) {
    shift_->setDirSleep(dir_bits_, sleep_bits_);
  }
}

void SharedStepAdapterEsp32::enableOutputs(uint8_t id) {
  if (id >= 8) return;
  slots_[id].awake = true;
  slots_[id].forced_awake = true;
  sleep_bits_ |= (1u << id);
  flushLatch_();
}

void SharedStepAdapterEsp32::disableOutputs(uint8_t id) {
  if (id >= 8) return;
  slots_[id].awake = false;
  slots_[id].forced_awake = false;
  sleep_bits_ &= (uint8_t)~(1u << id);
  flushLatch_();
}

bool SharedStepAdapterEsp32::startMoveAbs(uint8_t id, long target, int speed, int accel) {
  if (id >= 8) return false;
  Slot &s = slots_[id];
  long delta = target - s.pos;
  if (delta == 0) { s.moving = false; return true; }
  int desired_dir = (delta > 0) ? +1 : -1;
  int cur_dir_sign = (dir_bits_ & (1u << id)) ? +1 : -1;
  bool need_flip = (desired_dir != cur_dir_sign);
  s.target = target;
  s.speed_sps = speed > 0 ? speed : 1;
  // Update global ramp targets
  v_max_sps_ = s.speed_sps;
  a_sps2_ = (accel > 0) ? accel : 1;
  // If direction must change, force SLEEP low before scheduling flip
  if (need_flip) {
    sleep_bits_ &= (uint8_t)~(1u << id);
    flushLatch_();
  }
  
  // Decide safe approach: if generator is running, schedule a guarded flip in the next safe gap.
  // Otherwise, perform immediate safe sequence before starting pulses.
  FlipSchedule &fs = flips_[id];
  fs.active = false; fs.phase = 0; fs.new_dir_sign = desired_dir;
  uint32_t now_us = micros();
  if (need_flip && gen_running_ && period_us_ > 0) {
    DirFlipWindow w{};
    uint64_t now_rel = (now_us >= phase_anchor_us_) ? (now_us - phase_anchor_us_) : 0;
    if (compute_flip_window(now_rel, period_us_, w)) {
      fs.w.t_sleep_low = w.t_sleep_low + phase_anchor_us_;
      fs.w.t_dir_flip  = w.t_dir_flip  + phase_anchor_us_;
      fs.w.t_sleep_high= w.t_sleep_high+ phase_anchor_us_;
      fs.phase = 0;
      fs.active = true;
    }
  }
  if (!fs.active) {
    if (need_flip) {
      // Immediate safe flip while generator is idle (or as a fallback)
      if (desired_dir > 0) dir_bits_ |= (1u << id); else dir_bits_ &= (uint8_t)~(1u << id);
      flushLatch_();
      delayMicroseconds(3);
    }
    // Enable output without delay if direction already matches (no hazard)
    sleep_bits_ |= (1u << id);
    flushLatch_();
    slots_[id].awake = true;
  }
  // Start move bookkeeping; s.awake becomes true after SLEEP high phase
  s.dir_sign = desired_dir;
  s.moving = true;
  if (fs.active) s.awake = false; // will set true after guard window
  s.last_us = micros();
  s.acc_us = 0;
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

void SharedStepAdapterEsp32::updateProgress_(uint8_t id) const {
  Slot &s = const_cast<Slot&>(slots_[id]);
  if (!s.moving) return;
  uint32_t now_us = micros();
  uint32_t dt_us = now_us - s.last_us;
  s.last_us = now_us;
  // Handle any pending guarded flip/gating for this motor
  runFlipScheduler_(id, now_us);
  // Integrate position using global generator speed (shared STEP)
  if (s.awake && current_speed_sps_ > 0) {
    uint64_t total = (uint64_t)dt_us * (uint64_t)current_speed_sps_ + (uint64_t)s.acc_us;
    uint32_t steps = (uint32_t)(total / 1000000ULL);
    s.acc_us = (uint32_t)(total % 1000000ULL);
    if (steps > 0) {
      long new_pos = s.pos + (long)(s.dir_sign * (int)steps);
      if (s.dir_sign > 0) {
        if (new_pos >= s.target) { new_pos = s.target; s.moving = false; }
      } else {
        if (new_pos <= s.target) { new_pos = s.target; s.moving = false; }
      }
      s.pos = new_pos;
    }
  }
  // Update global ramp after integrating at least one motor
  updateRamp_(now_us);
  if (!s.moving) {
    // If no slots moving, stop generator
    if (!s.forced_awake) {
      // Auto-sleep this motor when it finishes
      sleep_bits_ &= (uint8_t)~(1u << id);
      flushLatch_();
      s.awake = false;
      
    }
    maybeStopGen_();
  }
}

void SharedStepAdapterEsp32::updateRamp_(uint32_t now_us) const {
  // Determine if any motor is moving
  uint32_t min_remaining = UINT_MAX;
  bool any_moving = false;
  for (uint8_t i = 0; i < 8; ++i) {
    const Slot &s = slots_[i];
    if (!s.moving) continue;
    any_moving = true;
    long d = s.target - s.pos;
    if (d < 0) d = -d;
    if ((uint32_t)d < min_remaining) min_remaining = (uint32_t)d;
  }
  if (!any_moving) {
    // No motion requested: stop generator and reset ramp
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

  // Compute dt since last ramp update
  uint32_t dt_us = (ramp_last_us_ <= now_us) ? (now_us - ramp_last_us_) : 0;
  if (dt_us == 0) return;
  ramp_last_us_ = now_us;

  // Decide whether to accelerate, coast, or decelerate
  uint32_t accel_up = (a_sps2_ > 0) ? (uint32_t)a_sps2_ : 1u;
  uint32_t decel_down = (d_sps2_ > 0) ? (uint32_t)d_sps2_ : 0u;
  uint32_t vcur = (v_cur_sps_ > 0) ? (uint32_t)v_cur_sps_ : 0u;
  uint32_t vmax = (v_max_sps_ > 0) ? (uint32_t)v_max_sps_ : (uint32_t)kMinGenSps;
  // stop distance to zero under accel: ceil(v^2 / (2a))
  uint32_t d_stop = 0;
  if (decel_down > 0) {
    d_stop = ceil_div_u32((uint64_t)vcur * (uint64_t)vcur, 2ull * (uint64_t)decel_down);
  }
  bool need_decel = (decel_down > 0) && ((min_remaining <= d_stop) || (vcur > vmax));
  bool need_accel = (!need_decel) && (vcur < vmax);
  // Compute dv = accel * dt (sps^2 * us / 1e6)
  uint32_t a_eff = need_accel ? accel_up : (need_decel ? decel_down : 0u);
  uint64_t inc = (uint64_t)a_eff * (uint64_t)dt_us + (uint64_t)dv_accum_;
  uint32_t dv = (uint32_t)(inc / 1000000ull);
  dv_accum_ = (uint32_t)(inc % 1000000ull);
  int new_v = (int)vcur;
  if (need_accel && dv > 0) {
    new_v += (int)dv;
    if (new_v > vmax) new_v = (int)vmax;
  } else if (need_decel && dv > 0) {
    new_v -= (int)dv;
    if (new_v < 0) new_v = 0;
  } else {
    // Keep dv accumulator from growing unbounded without changes
  }

  // Apply minimum running speed clamp for generator hardware when non-zero
  int apply_v = new_v;
  if (apply_v > 0 && apply_v < (int)kMinGenSps) apply_v = (int)kMinGenSps;

  if (!gen_running_) {
    maybeStartGen_(apply_v);
  } else if (apply_v != current_speed_sps_) {
    maybeStartGen_(apply_v);
  }
  v_cur_sps_ = new_v;
}

void SharedStepAdapterEsp32::runFlipScheduler_(uint8_t id, uint32_t now_us) const {
  FlipSchedule &fs = const_cast<FlipSchedule&>(flips_[id]);
  if (!fs.active) return;
  // If we passed the intended window due to coarse polling, recompute a new window
  if (period_us_ == 0) {
    fs.active = false;
    return;
  }
  // Fast-forward: if we've already passed the end of the window, complete in one shot
  if (now_us >= fs.w.t_sleep_high) {
    if (fs.new_dir_sign > 0) dir_bits_ |= (1u << id); else dir_bits_ &= (uint8_t)~(1u << id);
    sleep_bits_ |= (1u << id);
    flushLatch_();
    slots_[id].awake = true;
    fs.active = false;
    
    return;
  }
  // Phase 0: drop SLEEP low in window (idempotent; we already dropped before scheduling)
  if (fs.phase == 0) {
    if (now_us >= fs.w.t_sleep_low) {
      sleep_bits_ &= (uint8_t)~(1u << id);
      flushLatch_();
      fs.phase = 1;
      
    } else {
      return;
    }
  }
  // Phase 1: flip DIR at midpoint
  if (fs.phase == 1) {
    if (now_us >= fs.w.t_dir_flip) {
      if (fs.new_dir_sign > 0) dir_bits_ |= (1u << id); else dir_bits_ &= (uint8_t)~(1u << id);
      flushLatch_();
      fs.phase = 2;
      
    } else {
      return;
    }
  }
  // Phase 2: re-enable SLEEP after post-guard
  if (fs.phase == 2) {
    if (now_us >= fs.w.t_sleep_high) {
      sleep_bits_ |= (1u << id);
      flushLatch_();
      // Motor is now allowed to step again
      slots_[id].awake = true;
      fs.active = false;
      
}
  }
}

bool SharedStepAdapterEsp32::isMoving(uint8_t id) const {
  if (id >= 8) return false;
  updateProgress_(id);
  return slots_[id].moving;
}

long SharedStepAdapterEsp32::currentPosition(uint8_t id) const {
  if (id >= 8) return 0;
  updateProgress_(id);
  return slots_[id].pos;
}

void SharedStepAdapterEsp32::setCurrentPosition(uint8_t id, long pos) {
  if (id >= 8) return;
  Slot &s = slots_[id];
  s.pos = pos;
}

// Debug helpers removed after stabilization

#endif // ARDUINO
