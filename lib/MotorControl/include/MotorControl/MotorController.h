#pragma once
#include <stdint.h>
#include <cstddef>

struct MotorState {
  uint8_t id;
  long position;       // absolute steps
  int speed;           // steps/s (last applied or default)
  int accel;           // steps/s^2 (last applied or default)
  bool moving;
  bool awake;
  // Extended diagnostics/state
  bool homed;              // 1 after successful HOME; cleared on reboot
  int32_t steps_since_home; // absolute steps accumulated since last HOME
  int32_t budget_tenths;    // remaining runtime budget in tenths of seconds (can go < 0)
  uint32_t last_update_ms;  // last time budget bookkeeping ran
  // Last operation timing (for host queries)
  uint32_t last_op_started_ms;   // device ms when last MOVE/HOME began (0 if none)
  uint32_t last_op_last_ms;      // duration of last completed MOVE/HOME in ms
  uint32_t last_op_est_ms;       // estimated duration for last MOVE/HOME in ms
  uint8_t last_op_type;          // 0=none, 1=move, 2=home
  bool last_op_ongoing;          // true while MOVE/HOME is in progress
};

class MotorController {
public:
  virtual ~MotorController() {}
  virtual size_t motorCount() const = 0;
  virtual const MotorState& state(size_t idx) const = 0;
  virtual bool isAnyMovingForMask(uint32_t mask) const = 0;

  virtual void wakeMask(uint32_t mask) = 0;
  virtual bool sleepMask(uint32_t mask) = 0;
  virtual bool moveAbsMask(uint32_t mask, long target, int speed, int accel, uint32_t now_ms) = 0;
  virtual bool homeMask(uint32_t mask, long overshoot, long backoff, int speed, int accel, long full_range, uint32_t now_ms) = 0;
  virtual void tick(uint32_t now_ms) = 0;

  // Global thermal runtime limiting flag control
  virtual void setThermalLimitsEnabled(bool enabled) = 0;

};
