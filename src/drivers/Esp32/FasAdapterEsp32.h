#pragma once

#if defined(ARDUINO)

#include "Hal/FasAdapter.h"

#include <FastAccelStepper.h>
#include <cstdint>

class FasAdapterEsp32 : public IFasAdapter {
public:
  FasAdapterEsp32() = default;
  void begin() override;
  void configureStepPin(uint8_t motor_id, int gpio) override;
  bool startMoveAbs(uint8_t motor_id, long target, int speed, int accel) override;
  [[nodiscard]] bool isMoving(uint8_t motor_id) const override;
  [[nodiscard]] long currentPosition(uint8_t motor_id) const override;
  void setCurrentPosition(uint8_t motor_id, long position) override;

  void attachShiftRegister(IShift595* drv) override;
  void setAutoEnable(uint8_t motor_id, bool auto_enable) override;
  void enableOutputs(uint8_t motor_id) override;
  void disableOutputs(uint8_t motor_id) override;

  // Public static to allow engine registration
  static bool externalPinHandler(uint8_t pin, uint8_t value);
};

// Factory for HardwareMotorController default construction
IFasAdapter* createEsp32FasAdapter();

#endif
