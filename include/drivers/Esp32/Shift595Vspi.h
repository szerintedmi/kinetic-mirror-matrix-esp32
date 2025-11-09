#pragma once
#include <cstdint>
#include "Hal/Shift595.h"

class Shift595Esp32 : public IShift595 {
public:
  explicit Shift595Esp32(int latch_pin, int oe_pin = -1);
  void begin() override;
  void setDirSleep(uint8_t dir_bits, uint8_t sleep_bits) override;

private:
  int latch_pin_;
  int oe_pin_;
  bool outputs_enabled_ = false;
};
