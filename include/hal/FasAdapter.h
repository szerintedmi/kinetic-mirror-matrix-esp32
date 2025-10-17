#pragma once
#include <stdint.h>
#include "Hal/Shift595.h"

// Abstract adapter over FastAccelStepper (or equivalent) to enable
// unit testing on native by swapping in a stub.
class IFasAdapter {
public:
  virtual ~IFasAdapter() {}

  // One-time setup
  virtual void begin() = 0;

  // Configure the step pin for a given motor id [0..7]
  virtual void configureStepPin(uint8_t id, int gpio) = 0;

  // Start an absolute move to 'target' with given speed/accel.
  // Returns false if cannot start (e.g., not configured).
  virtual bool startMoveAbs(uint8_t id, long target, int speed, int accel) = 0;

  // Query running state for motor id.
  virtual bool isMoving(uint8_t id) const = 0;

  // Current absolute position for motor id.
  virtual long currentPosition(uint8_t id) const = 0;

  // Force set current position (e.g., homing or rebase).
  virtual void setCurrentPosition(uint8_t id, long pos) = 0;
  
  // Optional hooks used on ESP32 hardware to integrate external pin control
  // via FastAccelStepper and a 74HC595 shift register. Defaults are no-ops
  // for non-hardware adapters/native tests.
  virtual void attachShiftRegister(IShift595* /*drv*/) {}
  virtual void setAutoEnable(uint8_t /*id*/, bool /*auto_enable*/) {}
  virtual void enableOutputs(uint8_t /*id*/) {}
  virtual void disableOutputs(uint8_t /*id*/) {}
};
