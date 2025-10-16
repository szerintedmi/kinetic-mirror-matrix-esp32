#include "StubBackend.h"
#include <math.h>

static uint32_t maskForId(uint8_t id) { return 1u << id; }

StubBackend::StubBackend(uint8_t count) : count_(count) {
  if (count_ > 8) count_ = 8;
  for (uint8_t i = 0; i < count_; ++i) {
    motors_[i] = MotorState{ i, 0, 4000, 16000, false, false };
    plans_[i] = MovePlan{ false, 0, 0 };
  }
}

bool StubBackend::isAnyMovingForMask(uint32_t mask) const {
  for (uint8_t i = 0; i < count_; ++i) {
    if (mask & maskForId(i)) {
      if (motors_[i].moving) return true;
    }
  }
  return false;
}

void StubBackend::wakeMask(uint32_t mask) {
  for (uint8_t i = 0; i < count_; ++i) {
    if (mask & maskForId(i)) {
      motors_[i].awake = true;
    }
  }
}

bool StubBackend::sleepMask(uint32_t mask) {
  // Error if any targeted is moving
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

bool StubBackend::moveAbsMask(uint32_t mask, long target, int speed, int accel, uint32_t now_ms) {
  // Busy if any moving
  if (isAnyMovingForMask(mask)) return false;
  for (uint8_t i = 0; i < count_; ++i) {
    if (mask & maskForId(i)) {
      motors_[i].awake = true; // auto-wake
      motors_[i].speed = speed;
      motors_[i].accel = accel;
      motors_[i].moving = true;
      long delta = labs(target - motors_[i].position);
      uint32_t dur_ms = (uint32_t)ceil((1000.0 * (double)delta) / (double)((speed <= 0) ? 1 : speed));
      plans_[i].active = true;
      plans_[i].target = target;
      plans_[i].end_ms = now_ms + dur_ms;
    }
  }
  return true;
}

bool StubBackend::homeMask(uint32_t mask, long overshoot, long backoff, int speed, int accel, long full_range, uint32_t now_ms) {
  // Busy if any moving
  if (isAnyMovingForMask(mask)) return false;
  // Simulate homing duration roughly proportional to overshoot + backoff
  long steps = labs(overshoot) + labs(backoff);
  uint32_t dur_ms = (uint32_t)ceil((1000.0 * (double)steps) / (double)((speed <= 0) ? 1 : speed));
  for (uint8_t i = 0; i < count_; ++i) {
    if (mask & maskForId(i)) {
      motors_[i].awake = true; // auto-wake
      motors_[i].speed = speed;
      motors_[i].accel = accel;
      motors_[i].moving = true;
      plans_[i].active = true;
      plans_[i].target = 0; // midpoint baseline
      plans_[i].end_ms = now_ms + dur_ms;
    }
  }
  return true;
}

void StubBackend::tick(uint32_t now_ms) {
  for (uint8_t i = 0; i < count_; ++i) {
    if (plans_[i].active && now_ms >= plans_[i].end_ms) {
      motors_[i].position = plans_[i].target;
      motors_[i].moving = false;
      motors_[i].awake = false; // auto-sleep after completion
      plans_[i].active = false;
    }
  }
}

