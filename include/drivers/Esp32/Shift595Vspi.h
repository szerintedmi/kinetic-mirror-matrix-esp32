#pragma once
#include <stdint.h>
#include "Hal/Shift595.h"

class Shift595Esp32 : public IShift595 {
public:
  explicit Shift595Esp32(int latch_pin);
  void begin() override;
  void setDirSleep(uint8_t dir_bits, uint8_t sleep_bits) override;

private:
  int latch_pin_;
};

