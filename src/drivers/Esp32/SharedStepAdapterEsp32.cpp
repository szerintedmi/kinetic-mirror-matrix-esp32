#if defined(ARDUINO)

#include "drivers/Esp32/SharedStepAdapterEsp32.h"
using namespace SharedStepTiming;
using namespace SharedStepGuards;

SharedStepAdapterEsp32::SharedStepAdapterEsp32() {}

void SharedStepAdapterEsp32::begin() {
  gen_.begin();
  gen_running_ = false;
  current_speed_sps_ = 0;
  period_us_ = 0;
  dir_bits_ = 0;
  sleep_bits_ = 0;
  for (uint8_t i = 0; i < 8; ++i) { slots_[i] = Slot{}; flips_[i] = FlipSchedule{}; }
}

void SharedStepAdapterEsp32::maybeStartGen_(int speed_sps) {
  if (!gen_running_) {
    gen_.setSpeed((uint32_t)speed_sps);
    gen_.start();
    gen_running_ = true;
    current_speed_sps_ = speed_sps;
    period_us_ = step_period_us((uint32_t)(speed_sps > 0 ? speed_sps : 1));
    phase_anchor_us_ = micros();
  } else {
    if (speed_sps != current_speed_sps_) {
      gen_.setSpeed((uint32_t)speed_sps);
      current_speed_sps_ = speed_sps;
      period_us_ = step_period_us((uint32_t)(speed_sps > 0 ? speed_sps : 1));
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

bool SharedStepAdapterEsp32::startMoveAbs(uint8_t id, long target, int speed, int /*accel*/) {
  if (id >= 8) return false;
  Slot &s = slots_[id];
  long delta = target - s.pos;
  if (delta == 0) { s.moving = false; return true; }
  int desired_dir = (delta > 0) ? +1 : -1;
  s.target = target;
  s.speed_sps = speed > 0 ? speed : 1;
  // Ensure generator is running and period known
  maybeStartGen_(s.speed_sps);
  // Immediately force SLEEP low for this motor to avoid any edge hazard
  sleep_bits_ &= (uint8_t)~(1u << id);
  flushLatch_();
  // Schedule guarded DIR flip + SLEEP high in the next safe gap
  FlipSchedule &fs = flips_[id];
  fs.active = false; fs.phase = 0; fs.new_dir_sign = desired_dir;
  uint32_t now_us = micros();
  DirFlipWindow w{};
  if (period_us_ > 0) {
    uint64_t now_rel = (now_us >= phase_anchor_us_) ? (now_us - phase_anchor_us_) : 0;
    if (compute_flip_window(now_rel, period_us_, w)) {
      // Convert relative window to absolute times using anchor
      fs.w.t_sleep_low = w.t_sleep_low + phase_anchor_us_;
      fs.w.t_dir_flip  = w.t_dir_flip  + phase_anchor_us_;
      fs.w.t_sleep_high= w.t_sleep_high+ phase_anchor_us_;
      fs.phase = 0;
      fs.active = true;
    }
  }
  if (!fs.active) {
    fs.phase = 0;
    // If period unknown or guard too tight, perform immediate safe sequence:
    // keep SLEEP low (already), flip DIR now, then raise SLEEP after a small delay.
    if (desired_dir > 0) dir_bits_ |= (1u << id); else dir_bits_ &= (uint8_t)~(1u << id);
    flushLatch_();
    delayMicroseconds(3);
    sleep_bits_ |= (1u << id);
    flushLatch_();
    fs.active = false;
  }
  // Start move bookkeeping; s.awake becomes true after SLEEP high phase
  s.dir_sign = desired_dir;
  s.moving = true;
  s.awake = false;
  s.last_us = micros();
  s.acc_us = 0;
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
  if (s.awake && s.speed_sps > 0) {
    uint64_t total = (uint64_t)dt_us * (uint64_t)s.speed_sps + (uint64_t)s.acc_us;
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

void SharedStepAdapterEsp32::runFlipScheduler_(uint8_t id, uint32_t now_us) const {
  FlipSchedule &fs = const_cast<FlipSchedule&>(flips_[id]);
  if (!fs.active) return;
  // If we passed the intended window due to coarse polling, recompute a new window
  if (period_us_ == 0) {
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

#endif // ARDUINO
