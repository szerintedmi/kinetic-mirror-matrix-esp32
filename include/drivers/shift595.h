#pragma once
#include <stdint.h>

// 74HC595 controller for two daisy-chained chips (16 outputs):
// Byte 0 = DIR bits (1=forward), Byte 1 = SLEEP bits (1=awake)
class Shift595 {
public:
  explicit Shift595(int latch_pin);
  void begin();
  void setDirSleep(uint8_t dir_bits, uint8_t sleep_bits);

#ifdef UNIT_TEST
  // For tests (native): capture last shifted bytes
  uint8_t last_dir() const { return last_dir_; }
  uint8_t last_sleep() const { return last_sleep_; }
  unsigned latch_count() const { return latch_count_; }
#endif

private:
  int latch_pin_;
#ifdef UNIT_TEST
  uint8_t last_dir_ = 0;
  uint8_t last_sleep_ = 0;
  unsigned latch_count_ = 0;
#endif
};

