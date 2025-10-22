#if defined(ARDUINO) && (defined(ESP32) || defined(ARDUINO_ARCH_ESP32))

#include "drivers/Esp32/SharedStepRmt.h"
#include "MotorControl/SharedStepTiming.h"
#include <Arduino.h>

// For a spike, we can start with LEDC PWM as a stand-in for RMT to keep
// the implementation minimal while we validate timing hooks.
// Later we can switch to ESP-IDF RMT for precise control.

// Pick an LEDC channel/timer that doesn't clash with existing code
static constexpr int kLedcChannel = 6;
static constexpr int kLedcTimer = 2;
#ifndef SHARED_STEP_GPIO
#define SHARED_STEP_GPIO 4
#endif
static constexpr int kStepPin = SHARED_STEP_GPIO; // overridable via -DSHARED_STEP_GPIO

SharedStepRmtGenerator::SharedStepRmtGenerator()
    : speed_sps_(0), hook_(nullptr) {}

bool SharedStepRmtGenerator::begin()
{
  // Configure STEP pin
  pinMode(kStepPin, OUTPUT);
  digitalWrite(kStepPin, LOW);
  // Setup LEDC timer
  ledcSetup(kLedcChannel, /*freq Hz*/ 1, /*resolution bits*/ 10);
  ledcAttachPin(kStepPin, kLedcChannel);
  return true;
}

void SharedStepRmtGenerator::setSpeed(uint32_t speed_sps)
{
  speed_sps_ = speed_sps;
  if (speed_sps_ == 0)
  {
    ledcWrite(kLedcChannel, 0);
    return;
  }
  // Convert steps/s to Hz for LEDC; 50% duty
  const uint32_t freq_hz = speed_sps_;
  ledcWriteTone(kLedcChannel, freq_hz);
  // 50% duty on 10-bit resolution
  ledcWrite(kLedcChannel, 512);
}

void SharedStepRmtGenerator::start()
{
  if (speed_sps_ == 0)
    return;
  // LEDC already running after setSpeed via writeTone; ensure duty set
  ledcWrite(kLedcChannel, 512);
}

void SharedStepRmtGenerator::stop()
{
  ledcWrite(kLedcChannel, 0);
}

void SharedStepRmtGenerator::setEdgeHook(SharedStepEdgeHook hook)
{
  hook_ = hook;
  // Note: With LEDC, we don't get an ISR per edge. When migrating to RMT,
  // the hook can be invoked in the RMT ISR after a rising edge item.
}

#endif // ARDUINO ESP32
