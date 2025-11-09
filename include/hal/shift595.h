#pragma once
#include <cstdint>

// HAL interface for two daisy-chained 74HC595 (16 outputs)
// Byte 0 = DIR bits (1=forward), Byte 1 = SLEEP bits (1=awake)
class IShift595 {
public:
  IShift595() = default;
  IShift595(const IShift595&) = default;
  IShift595& operator=(const IShift595&) = default;
  IShift595(IShift595&&) = default;
  IShift595& operator=(IShift595&&) = default;
  virtual ~IShift595() = default;
  virtual void begin() = 0;
  virtual void setDirSleep(uint8_t dir_bits, uint8_t sleep_bits) = 0;
};
