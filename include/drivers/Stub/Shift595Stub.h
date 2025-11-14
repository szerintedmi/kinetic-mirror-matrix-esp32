#pragma once
#include "hal/Shift595.h"

#include <cstdint>

class Shift595Stub : public IShift595 {
public:
  explicit Shift595Stub(int /*latch_pin*/ = -1) {}
  void begin() override {
    last_dir_ = 0;
    last_sleep_ = 0;
    latch_count_ = 0;
  }
  void setDirSleep(uint8_t dir_bits, uint8_t sleep_bits) override {
    last_dir_ = dir_bits;
    last_sleep_ = sleep_bits;
    latch_count_++;
  }

  // Accessors for tests
  [[nodiscard]] uint8_t last_dir() const {
    return last_dir_;
  }
  [[nodiscard]] uint8_t last_sleep() const {
    return last_sleep_;
  }
  [[nodiscard]] unsigned latch_count() const {
    return latch_count_;
  }

private:
  uint8_t last_dir_ = 0;
  uint8_t last_sleep_ = 0;
  unsigned latch_count_ = 0;
};
