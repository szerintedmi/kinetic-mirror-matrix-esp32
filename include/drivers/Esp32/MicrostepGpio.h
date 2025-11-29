#pragma once
#include <cstdint>

// Microstepping mode for DRV8825 drivers
// Values correspond to log2 of the multiplier for efficient storage
enum class MicrostepMode : uint8_t {
  FULL = 0,           // 1x  - M0=L, M1=L, M2=L
  HALF = 1,           // 2x  - M0=H, M1=L, M2=L
  QUARTER = 2,        // 4x  - M0=L, M1=H, M2=L
  EIGHTH = 3,         // 8x  - M0=H, M1=H, M2=L
  SIXTEENTH = 4,      // 16x - M0=L, M1=L, M2=H
  THIRTY_SECOND = 5   // 32x - M0=H, M1=L, M2=H
};

// Returns the step multiplier for a given mode (1, 2, 4, 8, 16, or 32)
inline uint8_t MicrostepModeToMultiplier(MicrostepMode mode) {
  return 1U << static_cast<uint8_t>(mode);
}

// Converts mode enum to human-readable string
inline const char* MicrostepModeToString(MicrostepMode mode) {
  switch (mode) {
  case MicrostepMode::FULL:
    return "FULL";
  case MicrostepMode::HALF:
    return "HALF";
  case MicrostepMode::QUARTER:
    return "1/4";
  case MicrostepMode::EIGHTH:
    return "1/8";
  case MicrostepMode::SIXTEENTH:
    return "1/16";
  case MicrostepMode::THIRTY_SECOND:
    return "1/32";
  default:
    return "UNKNOWN";
  }
}

// GPIO driver for DRV8825 microstepping control
// Controls M0, M1, M2 pins shared across all motor drivers
class MicrostepGpio {
public:
  MicrostepGpio(int m0_pin, int m1_pin, int m2_pin);
  void begin();
  void setMode(MicrostepMode mode);
  [[nodiscard]] MicrostepMode mode() const { return current_mode_; }
  [[nodiscard]] uint8_t multiplier() const { return MicrostepModeToMultiplier(current_mode_); }

private:
  int m0_pin_;
  int m1_pin_;
  int m2_pin_;
  MicrostepMode current_mode_ = MicrostepMode::THIRTY_SECOND;

  void applyPinStates();
};
