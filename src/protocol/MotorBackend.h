#pragma once
#include <stdint.h>
#include <stddef.h>

struct MotorState {
  uint8_t id;
  long position;       // absolute steps
  int speed;           // steps/s (last applied or default)
  int accel;           // steps/s^2 (last applied or default)
  bool moving;
  bool awake;
};

class MotorBackend {
public:
  virtual ~MotorBackend() {}
  virtual size_t motorCount() const = 0;
  virtual const MotorState& state(size_t idx) const = 0;
  virtual bool isAnyMovingForMask(uint32_t mask) const = 0;

  // Control
  virtual void wakeMask(uint32_t mask) = 0;
  virtual bool sleepMask(uint32_t mask) = 0; // returns false if any targeted are moving

  // Motion
  virtual bool moveAbsMask(uint32_t mask, long target, int speed, int accel, uint32_t now_ms) = 0; // false = busy conflict
  virtual bool homeMask(uint32_t mask, long overshoot, long backoff, int speed, int accel, long full_range, uint32_t now_ms) = 0; // false = busy conflict

  // Progress time-based completions
  virtual void tick(uint32_t now_ms) = 0;
};

