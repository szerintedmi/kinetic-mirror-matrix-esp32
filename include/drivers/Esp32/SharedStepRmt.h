#pragma once

#include <cstdint>

#if !defined(ARDUINO)
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif
#endif

using SharedStepEdgeHook = void (*)();

// Minimal SPIKE interface for a shared STEP generator on ESP32 using RMT/LEDC.
// This file is safe to include on host; implementation is ESP32-only.
class SharedStepRmtGenerator {
public:
  SharedStepRmtGenerator();
  // Initialize peripherals; return false on hardware init error.
  bool begin();
  // Set global steps-per-second. speed==0 stops generator.
  void setSpeed(uint32_t speed_sps);
  // Start/stop pulse generation. No-op if speed is zero.
  void start();
  void stop();
  // Optional hook called shortly after each STEP rising edge (ISR context!).
  void setEdgeHook(SharedStepEdgeHook hook);

  void onTxEndIsr();

private:
  volatile uint32_t speed_sps_ = 0;
  SharedStepEdgeHook hook_ = nullptr;
  volatile bool running_ = false;
};
