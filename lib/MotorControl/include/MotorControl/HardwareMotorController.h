#pragma once
#include "hal/FasAdapter.h"
#include "hal/shift595.h"
#include "MotorControl/MotorController.h"

#include <memory>
#if defined(ARDUINO)
#include "drivers/Esp32/AdapterFactory.h"
#endif

// Hardware-backed motor controller integrating FastAccelStepper and
// two chained 74HC595 for DIR/SLEEP.
class HardwareMotorController : public MotorController {
public:
  // Test/injection constructor: provide dependencies.
  HardwareMotorController(IShift595& shift, IFasAdapter& fas, uint8_t count = 8);

#if defined(ARDUINO)
  // Default hardware constructor for ESP32 builds.
  HardwareMotorController();
#endif

  // MotorController API
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
  void setThermalLimitsEnabled(bool enabled) override {
    thermal_limits_enabled_ = enabled;
  }
  void setDeceleration(int decel_sps2) override;

private:
  void latch_();
  void startMoveSingle_(uint8_t id, long target, int speed, int accel);
  static uint32_t maskForId(uint8_t id) {
    return 1u << id;
  }

  uint8_t count_ = 8;
  MotorState motors_[8];
  struct HomingPlan {
    bool active;
    uint8_t phase;
    long overshoot;
    long backoff;
    long full_range;
    int speed;
    int accel;
  };
  HomingPlan homing_[8];

  // Current latched outputs to 74HC595 (used in native tests)
  uint8_t dir_bits_ = 0;    // 1 = forward
  uint8_t sleep_bits_ = 0;  // 1 = awake/high
  // WAKE override bitmask
  uint8_t forced_awake_mask_ = 0;  // 1 bit per motor, WAKE override

  // Ownership depends on constructor
  IShift595* shift_ = nullptr;
  IFasAdapter* fas_ = nullptr;
  std::unique_ptr<IShift595> owned_shift_;
  std::unique_ptr<IFasAdapter> owned_fas_;

  bool thermal_limits_enabled_ = true;
  int decel_sps2_ = 0;  // global deceleration hint for shared-STEP estimates
};
