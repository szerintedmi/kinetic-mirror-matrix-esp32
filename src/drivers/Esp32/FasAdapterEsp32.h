#pragma once

#if defined(ARDUINO)

#include <stdint.h>
#include "Hal/FasAdapter.h"
#include <FastAccelStepper.h>

class FasAdapterEsp32 : public IFasAdapter {
public:
  FasAdapterEsp32() {}
  void begin() override;
  void configureStepPin(uint8_t id, int gpio) override;
  bool startMoveAbs(uint8_t id, long target, int speed, int accel) override;
  bool isMoving(uint8_t id) const override;
  long currentPosition(uint8_t id) const override;
  void setCurrentPosition(uint8_t id, long pos) override;
  
  void attachShiftRegister(IShift595* drv) override;
  void setAutoEnable(uint8_t id, bool auto_enable) override;
  void enableOutputs(uint8_t id) override;
  void disableOutputs(uint8_t id) override;

  // Public static to allow engine registration
  static bool externalPinHandler(uint8_t pin, uint8_t value);
};

// Factory for HardwareMotorController default construction
IFasAdapter* createEsp32FasAdapter();

#endif
