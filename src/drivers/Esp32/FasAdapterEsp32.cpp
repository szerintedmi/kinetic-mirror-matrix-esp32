#if defined(ARDUINO) && !defined(USE_SHARED_STEP)

#include "drivers/Esp32/FasAdapterEsp32.h"

#include "MotorControl/BuildConfig.h"

#include <Arduino.h>
#include <FastAccelStepper.h>
#include <array>
#include <atomic>

// External pin integration state
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
static IShift595* g_shift = nullptr;
// Use atomics for ISR-safe access - these are modified from FastAccelStepper ISR
static std::atomic<uint8_t> g_dir_bits{0};
static std::atomic<uint8_t> g_sleep_bits{0};
// Dirty flag: set in ISR, cleared after SPI latch in main loop
static std::atomic<bool> g_latch_dirty{false};
// Shadow copies for main-loop latch (avoids repeated atomic loads)
static uint8_t g_last_latched_dir{0};
static uint8_t g_last_latched_sleep{0};
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)
static constexpr uint8_t DIR_BASE = 0;     // virtual range [0..7]
static constexpr uint8_t SLEEP_BASE = 32;  // virtual range [32..39]
static constexpr uint8_t kMotorSlots = 8;
static constexpr uint16_t kDirSetupDelayUs = 200;
static constexpr uint16_t kAutoEnableDelayUs = 2000;

// Concrete adapter using FastAccelStepper library, step pin only
class FasAdapterEsp32Impl : public FasAdapterEsp32 {
public:
  // NOLINTNEXTLINE(modernize-use-equals-default) - need to fill arrays with -1, not zero
  FasAdapterEsp32Impl() {
    steppers_.fill(nullptr);
    step_pins_.fill(-1);
    last_speed_.fill(-1);
    last_accel_.fill(-1);
  }

  void begin() override {
    this->engine_.init();
#if !(USE_SHARED_STEP)
    this->engine_.setExternalCallForPin(&FasAdapterEsp32::externalPinHandler);
#endif
  }

  void
  configureStepPin(uint8_t motor_id,
                   int gpio) override  // NOLINT(readability-convert-member-functions-to-static)
  {
    if (motor_id >= kMotorSlots) {
      return;
    }
    this->step_pins_[motor_id] = gpio;
    if (this->steppers_[motor_id] == nullptr && gpio >= 0) {
      FastAccelStepper* stepper = this->engine_.stepperConnectToPin(static_cast<uint8_t>(gpio));
      this->steppers_[motor_id] = stepper;
      if (stepper != nullptr) {
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

  bool startMoveAbs(uint8_t motor_id,
                    long target,
                    int speed,
                    int accel) override  // NOLINT(readability-convert-member-functions-to-static)
  {
    if (motor_id >= kMotorSlots) {
      return false;
    }
    FastAccelStepper* stepper = this->steppers_[motor_id];
    if (stepper == nullptr) {
      return false;
    }
    if (speed != this->last_speed_[motor_id]) {
      stepper->setSpeedInHz(static_cast<uint32_t>(speed));
      this->last_speed_[motor_id] = speed;
    }
    if (accel != this->last_accel_[motor_id]) {
      stepper->setAcceleration(static_cast<int32_t>(accel));
      this->last_accel_[motor_id] = accel;
    }
    return stepper->moveTo(target) == MOVE_OK;
  }

  [[nodiscard]] bool isMoving(
      uint8_t motor_id) const override  // NOLINT(readability-convert-member-functions-to-static)
  {
    if (motor_id >= kMotorSlots) {
      return false;
    }
    FastAccelStepper* stepper = this->steppers_[motor_id];
    return (stepper != nullptr) && stepper->isRunning();
  }

  [[nodiscard]] long currentPosition(
      uint8_t motor_id) const override  // NOLINT(readability-convert-member-functions-to-static)
  {
    if (motor_id >= kMotorSlots) {
      return 0;
    }
    FastAccelStepper* stepper = this->steppers_[motor_id];
    return (stepper != nullptr) ? stepper->getCurrentPosition() : 0;
  }

  void
  setCurrentPosition(uint8_t motor_id,
                     long pos) override  // NOLINT(readability-convert-member-functions-to-static)
  {
    if (motor_id >= kMotorSlots) {
      return;
    }
    FastAccelStepper* stepper = this->steppers_[motor_id];
    if (stepper != nullptr) {
      stepper->setCurrentPosition(pos);
    }
  }

  void attachShiftRegister(IShift595* drv) override {
    g_shift = drv;
  }
  void setAutoEnable(uint8_t motor_id, bool auto_enable)
      override  // NOLINT(readability-convert-member-functions-to-static)
  {
    if (motor_id >= kMotorSlots) {
      return;
    }
    auto* stepper = this->steppers_[motor_id];
    if (stepper == nullptr) {
      return;
    }
    stepper->setAutoEnable(auto_enable);
  }
  void enableOutputs(
      uint8_t motor_id) override  // NOLINT(readability-convert-member-functions-to-static)
  {
    if (motor_id >= kMotorSlots) {
      return;
    }
    auto* stepper = this->steppers_[motor_id];
    if (stepper == nullptr) {
      return;
    }
    stepper->enableOutputs();
  }
  void disableOutputs(
      uint8_t motor_id) override  // NOLINT(readability-convert-member-functions-to-static)
  {
    if (motor_id >= kMotorSlots) {
      return;
    }
    auto* stepper = this->steppers_[motor_id];
    if (stepper == nullptr) {
      return;
    }
    stepper->disableOutputs();
  }

  void pollLatch() override { FasAdapterEsp32::pollLatchStatic(); }

private:
  FastAccelStepperEngine engine_{};
  std::array<FastAccelStepper*, kMotorSlots> steppers_{};
  std::array<int, kMotorSlots> step_pins_{};
  std::array<int, kMotorSlots> last_speed_{};
  std::array<int, kMotorSlots> last_accel_{};
};

void FasAdapterEsp32::begin() {}  // NOLINT(readability-convert-member-functions-to-static)
void FasAdapterEsp32::configureStepPin(uint8_t /*motor_id*/, int /*gpio*/) {
}  // NOLINT(readability-convert-member-functions-to-static)
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
bool FasAdapterEsp32::startMoveAbs(uint8_t /*motor_id*/,
                                   long /*target*/,
                                   int /*speed*/,
                                   int /*accel*/) {
  return false;
}
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
bool FasAdapterEsp32::isMoving(uint8_t /*motor_id*/) const {
  return false;
}
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
long FasAdapterEsp32::currentPosition(uint8_t /*motor_id*/) const {
  return 0;
}
void FasAdapterEsp32::setCurrentPosition(uint8_t /*motor_id*/, long /*position*/) {
}  // NOLINT(readability-convert-member-functions-to-static)

IFasAdapter* createEsp32FasAdapter() {
  return new FasAdapterEsp32Impl();
}

// static
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
bool FasAdapterEsp32::externalPinHandler(
    uint8_t pin_identifier,
    uint8_t value)
{
  uint8_t pin_index = static_cast<uint8_t>(pin_identifier & ~PIN_EXTERNAL_FLAG);
  const bool is_high = (value != 0);
  if (pin_index >= SLEEP_BASE) {
    uint8_t motor_id = pin_index - SLEEP_BASE;
    const uint8_t mask = static_cast<uint8_t>(1U << motor_id);
    // NOLINTNEXTLINE(bugprone-branch-clone) - false positive: fetch_or vs fetch_and are different
    if (is_high) {
      g_sleep_bits.fetch_or(mask, std::memory_order_relaxed);
    } else {
      g_sleep_bits.fetch_and(static_cast<uint8_t>(~mask), std::memory_order_relaxed);
    }
  } else {
    uint8_t motor_id = pin_index - DIR_BASE;
    const uint8_t mask = static_cast<uint8_t>(1U << motor_id);
    // NOLINTNEXTLINE(bugprone-branch-clone) - false positive: fetch_or vs fetch_and are different
    if (is_high) {
      g_dir_bits.fetch_or(mask, std::memory_order_relaxed);
    } else {
      g_dir_bits.fetch_and(static_cast<uint8_t>(~mask), std::memory_order_relaxed);
    }
  }
  // Mark dirty for main-loop latch; DO NOT call SPI from ISR context
  g_latch_dirty.store(true, std::memory_order_release);
  return is_high;
}

void FasAdapterEsp32::pollLatchStatic() {
  if (!g_latch_dirty.load(std::memory_order_acquire)) {
    return;
  }
  uint8_t dir = g_dir_bits.load(std::memory_order_relaxed);
  uint8_t sleep = g_sleep_bits.load(std::memory_order_relaxed);
  // Only latch if bits actually changed (reduces SPI traffic)
  if (dir != g_last_latched_dir || sleep != g_last_latched_sleep) {
    if (g_shift != nullptr) {
      g_shift->setDirSleep(dir, sleep);
    }
    g_last_latched_dir = dir;
    g_last_latched_sleep = sleep;
  }
  g_latch_dirty.store(false, std::memory_order_release);
}

#endif
