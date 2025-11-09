#if defined(ARDUINO) && !defined(USE_SHARED_STEP)

#include "drivers/Esp32/FasAdapterEsp32.h"
#include <Arduino.h>
#include <FastAccelStepper.h>
#include <array>
#include "MotorControl/BuildConfig.h"

// External pin integration state
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
static IShift595 *g_shift = nullptr;
static uint8_t g_dir_bits = 0;
static uint8_t g_sleep_bits = 0;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)
static constexpr uint8_t DIR_BASE = 0;    // virtual range [0..7]
static constexpr uint8_t SLEEP_BASE = 32; // virtual range [32..39]
static constexpr uint8_t kMotorSlots = 8;
static constexpr uint16_t kDirSetupDelayUs = 200;
static constexpr uint16_t kAutoEnableDelayUs = 2000;

// Concrete adapter using FastAccelStepper library, step pin only
class FasAdapterEsp32Impl : public FasAdapterEsp32
{
public:
  FasAdapterEsp32Impl() // NOLINT(modernize-use-equals-default)
  {
    steppers_.fill(nullptr);
    step_pins_.fill(-1);
    last_speed_.fill(-1);
    last_accel_.fill(-1);
  }

  void begin() override
  {
    engine_.init();
#if !(USE_SHARED_STEP)
    engine_.setExternalCallForPin(&FasAdapterEsp32::externalPinHandler);
#endif
  }

  void configureStepPin(uint8_t motor_id, int gpio) override // NOLINT(readability-convert-member-functions-to-static)
  {
    if (motor_id >= kMotorSlots)
    {
      return;
    }
    step_pins_[motor_id] = gpio;
    if (steppers_[motor_id] == nullptr && gpio >= 0)
    {
      FastAccelStepper *stepper = engine_.stepperConnectToPin(static_cast<uint8_t>(gpio));
      steppers_[motor_id] = stepper;
      if (stepper != nullptr)
      {
        const uint8_t dir_pin = static_cast<uint8_t>(DIR_BASE + motor_id) | PIN_EXTERNAL_FLAG;
        const uint8_t sleep_pin = static_cast<uint8_t>(SLEEP_BASE + motor_id) | PIN_EXTERNAL_FLAG;
        stepper->setDirectionPin(dir_pin, true /*HighCountsUp*/, kDirSetupDelayUs);
        stepper->setEnablePin(sleep_pin, false /*low_active_enables_stepper?*/);
#if !(USE_SHARED_STEP)
        stepper->setAutoEnable(true);
        stepper->setDelayToEnable(kAutoEnableDelayUs);
#else
        stepper->setAutoEnable(false);
#endif
      }
    }
  }

  bool startMoveAbs(uint8_t motor_id, long target, int speed, int accel) override // NOLINT(readability-convert-member-functions-to-static)
  {
    if (motor_id >= kMotorSlots)
    {
      return false;
    }
    FastAccelStepper *stepper = steppers_[motor_id];
    if (stepper == nullptr)
    {
      return false;
    }
    if (speed != last_speed_[motor_id])
    {
      stepper->setSpeedInHz(static_cast<uint32_t>(speed));
      last_speed_[motor_id] = speed;
    }
    if (accel != last_accel_[motor_id])
    {
      stepper->setAcceleration(static_cast<int32_t>(accel));
      last_accel_[motor_id] = accel;
    }
    return stepper->moveTo(target) == MOVE_OK;
  }

  [[nodiscard]] bool isMoving(uint8_t motor_id) const override // NOLINT(readability-convert-member-functions-to-static)
  {
    if (motor_id >= kMotorSlots)
    {
      return false;
    }
    FastAccelStepper *stepper = steppers_[motor_id];
    return (stepper != nullptr) && stepper->isRunning();
  }

  [[nodiscard]] long currentPosition(uint8_t motor_id) const override // NOLINT(readability-convert-member-functions-to-static)
  {
    if (motor_id >= kMotorSlots)
    {
      return 0;
    }
    FastAccelStepper *stepper = steppers_[motor_id];
    return (stepper != nullptr) ? stepper->getCurrentPosition() : 0;
  }

  void setCurrentPosition(uint8_t motor_id, long pos) override // NOLINT(readability-convert-member-functions-to-static)
  {
    if (motor_id >= kMotorSlots)
    {
      return;
    }
    FastAccelStepper *stepper = steppers_[motor_id];
    if (stepper != nullptr)
    {
      stepper->setCurrentPosition(pos);
    }
  }

  void attachShiftRegister(IShift595 *drv) override { g_shift = drv; }
  void setAutoEnable(uint8_t motor_id, bool auto_enable) override // NOLINT(readability-convert-member-functions-to-static)
  {
    if (motor_id >= kMotorSlots)
    {
      return;
    }
    auto *stepper = steppers_[motor_id];
    if (stepper == nullptr)
    {
      return;
    }
    stepper->setAutoEnable(auto_enable);
  }
  void enableOutputs(uint8_t motor_id) override // NOLINT(readability-convert-member-functions-to-static)
  {
    if (motor_id >= kMotorSlots)
    {
      return;
    }
    auto *stepper = steppers_[motor_id];
    if (stepper == nullptr)
    {
      return;
    }
    stepper->enableOutputs();
  }
  void disableOutputs(uint8_t motor_id) override // NOLINT(readability-convert-member-functions-to-static)
  {
    if (motor_id >= kMotorSlots)
    {
      return;
    }
    auto *stepper = steppers_[motor_id];
    if (stepper == nullptr)
    {
      return;
    }
    stepper->disableOutputs();
  }

private:
  FastAccelStepperEngine engine_{};
  std::array<FastAccelStepper *, kMotorSlots> steppers_{};
  std::array<int, kMotorSlots> step_pins_{};
  std::array<int, kMotorSlots> last_speed_{};
  std::array<int, kMotorSlots> last_accel_{};
};

void FasAdapterEsp32::begin() {} // NOLINT(readability-convert-member-functions-to-static)
void FasAdapterEsp32::configureStepPin(uint8_t /*motor_id*/, int /*gpio*/) {} // NOLINT(readability-convert-member-functions-to-static)
bool FasAdapterEsp32::startMoveAbs(uint8_t /*motor_id*/, long /*target*/, int /*speed*/, int /*accel*/) { return false; } // NOLINT(readability-convert-member-functions-to-static)
bool FasAdapterEsp32::isMoving(uint8_t /*motor_id*/) const { return false; } // NOLINT(readability-convert-member-functions-to-static)
long FasAdapterEsp32::currentPosition(uint8_t /*motor_id*/) const { return 0; } // NOLINT(readability-convert-member-functions-to-static)
void FasAdapterEsp32::setCurrentPosition(uint8_t /*motor_id*/, long /*position*/) {} // NOLINT(readability-convert-member-functions-to-static)

IFasAdapter *createEsp32FasAdapter()
{
  return new FasAdapterEsp32Impl();
}

// static
bool FasAdapterEsp32::externalPinHandler(uint8_t pin_identifier, uint8_t value) // NOLINT(readability-convert-member-functions-to-static)
{
  uint8_t pin_index = static_cast<uint8_t>(pin_identifier & ~PIN_EXTERNAL_FLAG);
  const bool is_high = (value != 0);
  if (pin_index >= SLEEP_BASE)
  {
    uint8_t motor_id = pin_index - SLEEP_BASE;
    if (is_high)
    {
      g_sleep_bits |= (1U << motor_id);
    }
    else
    {
      g_sleep_bits &= static_cast<uint8_t>(~(1U << motor_id));
    }
  }
  else
  {
    uint8_t motor_id = pin_index - DIR_BASE;
    if (is_high)
    {
      g_dir_bits |= (1U << motor_id);
    }
    else
    {
      g_dir_bits &= static_cast<uint8_t>(~(1U << motor_id));
    }
  }
  if (g_shift != nullptr)
  {
    g_shift->setDirSleep(g_dir_bits, g_sleep_bits);
  }
  return is_high;
}

#endif
