#pragma once
#include "MotorBackend.h"
#include <stdint.h>

class StubBackend : public MotorBackend {
public:
  explicit StubBackend(uint8_t count = 8);
  size_t motorCount() const override { return count_; }
  const MotorState& state(size_t idx) const override { return motors_[idx]; }
  bool isAnyMovingForMask(uint32_t mask) const override;

  void wakeMask(uint32_t mask) override;
  bool sleepMask(uint32_t mask) override;
  bool moveAbsMask(uint32_t mask, long target, int speed, int accel, uint32_t now_ms) override;
  bool homeMask(uint32_t mask, long overshoot, long backoff, int speed, int accel, long full_range, uint32_t now_ms) override;
  void tick(uint32_t now_ms) override;

private:
  struct MovePlan { bool active; long target; uint32_t end_ms; };
  uint8_t count_;
  MotorState motors_[8];
  MovePlan plans_[8];
};

