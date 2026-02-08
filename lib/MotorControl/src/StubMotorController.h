#pragma once
#include "MotorControl/MotorController.h"
#include "drivers/Esp32/MicrostepGpio.h"

class StubMotorController : public MotorController {
public:
  explicit StubMotorController(uint8_t count = 8);
  size_t motorCount() const override {
    return count_;
  }
  const MotorState& state(size_t idx) const override {
    return motors_[idx];
  }
  bool isAnyMovingForMask(uint32_t mask) const override;

  void wakeMask(uint32_t mask) override;
  bool sleepMask(uint32_t mask) override;
  bool moveAbsMask(uint32_t mask, long target, int speed, int accel, uint32_t now_ms) override;
  bool homeMask(uint32_t mask,
                long overshoot,
                long backoff,
                int speed,
                int accel,
                long full_range,
                uint32_t now_ms) override;
  void tick(uint32_t now_ms) override;
  void setSettleParams(uint32_t mask, int overshoot,
                       int dither_amplitude, int dither_cycles,
                       int dither_min_amplitude) override;
  void setThermalLimitsEnabled(bool enabled) override {
    thermal_limits_enabled_ = enabled;
  }
  void setBudget(size_t idx, int32_t budget_tenths) override {
    if (idx < count_) {
      motors_[idx].budget_tenths = budget_tenths;
    }
  }
  void setDeceleration(int) override {}
  void setMicrostepMode(MicrostepMode mode) override;

private:
  struct MovePlan {
    bool active;
    bool is_home;
    long target;
    uint32_t end_ms;
    long start_pos;
  };
  uint8_t count_;
  MotorState motors_[8];
  MovePlan plans_[8];
  bool thermal_limits_enabled_ = true;
  uint8_t microstep_multiplier_ = 32;  // default: 1/32
};
