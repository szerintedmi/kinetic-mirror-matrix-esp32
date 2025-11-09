#pragma once
#include <cstddef>
#include <cstdint>
#include "Hal/Shift595.h"

// Abstract adapter over FastAccelStepper (or equivalent) to enable
// unit testing on native by swapping in a stub.
class IFasAdapter {
public:
  IFasAdapter() = default;
  IFasAdapter(const IFasAdapter &) = default;
  IFasAdapter &operator=(const IFasAdapter &) = default;
  IFasAdapter(IFasAdapter &&) = default;
  IFasAdapter &operator=(IFasAdapter &&) = default;
  virtual ~IFasAdapter() = default;

  // One-time setup
  virtual void begin() = 0;

  // Configure the step pin for a given motor id [0..7]
  virtual void configureStepPin(uint8_t motor_id, int gpio) = 0;

  // Start an absolute move to 'target' with given speed/accel.
  // Returns false if cannot start (e.g., not configured).
  [[nodiscard]] virtual bool startMoveAbs(uint8_t motor_id, long target, int speed, int accel) = 0;

  // Query running state for motor id.
  [[nodiscard]] virtual bool isMoving(uint8_t motor_id) const = 0;

  // Current absolute position for motor id.
  [[nodiscard]] virtual long currentPosition(uint8_t motor_id) const = 0;

  // Force set current position (e.g., homing or rebase).
  virtual void setCurrentPosition(uint8_t motor_id, long pos) = 0;
  
  // Optional hooks used on ESP32 hardware to integrate external pin control
  // via FastAccelStepper and a 74HC595 shift register. Defaults are no-ops
  // for non-hardware adapters/native tests.
  virtual void attachShiftRegister(IShift595 * /*drv*/) {}
  virtual void setAutoEnable(uint8_t /*motor_id*/, bool /*auto_enable*/) {}
  virtual void enableOutputs(uint8_t /*motor_id*/) {}
  virtual void disableOutputs(uint8_t /*motor_id*/) {}

  // Optional: deceleration hint for adapters that implement asymmetric ramps.
  // Default is no-op; FastAccelStepper path uses symmetric acceleration.
  virtual void setDeceleration(int /*decel_sps2*/) {}
};
