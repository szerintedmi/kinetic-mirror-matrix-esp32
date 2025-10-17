#if defined(ARDUINO)

#include "drivers/Esp32/FasAdapterEsp32.h"
#include <Arduino.h>
#include <FastAccelStepper.h>

// External pin integration state
static IShift595 *g_shift = nullptr;
static uint8_t g_dir_bits = 0;
static uint8_t g_sleep_bits = 0;
static constexpr uint8_t DIR_BASE = 0;    // virtual range [0..7]
static constexpr uint8_t SLEEP_BASE = 32; // virtual range [32..39]

// Concrete adapter using FastAccelStepper library, step pin only
class FasAdapterEsp32Impl : public FasAdapterEsp32
{
public:
  FasAdapterEsp32Impl() : engine_()
  {
    for (uint8_t i = 0; i < 8; ++i)
    {
      steppers_[i] = nullptr;
      step_pins_[i] = -1;
      last_speed_[i] = -1;
      last_accel_[i] = -1;
    }
  }

  void begin() override
  {
    engine_.init();
    engine_.setExternalCallForPin(&FasAdapterEsp32::externalPinHandler);
  }

  void configureStepPin(uint8_t id, int gpio) override
  {
    if (id >= 8)
      return;
    step_pins_[id] = gpio;
    if (steppers_[id] == nullptr && gpio >= 0)
    {
      FastAccelStepper *st = engine_.stepperConnectToPin((uint8_t)gpio);
      steppers_[id] = st;
      if (st)
      {
        uint8_t vdir = (uint8_t)(DIR_BASE + id) | PIN_EXTERNAL_FLAG;
        uint8_t vsleep = (uint8_t)(SLEEP_BASE + id) | PIN_EXTERNAL_FLAG;
        st->setDirectionPin(vdir, true /*HighCountsUp*/, 200 /*us delay*/);
        st->setEnablePin(vsleep, false /*low_active_enables_stepper?*/);
        st->setAutoEnable(true);
        st->setDelayToEnable(2000);
      }
    }
  }

  bool startMoveAbs(uint8_t id, long target, int speed, int accel) override
  {
    if (id >= 8)
      return false;
    FastAccelStepper *st = steppers_[id];
    if (!st)
      return false;
    // Configure speed/accel only if changed
    if (speed != last_speed_[id])
    {
      st->setSpeedInHz((uint32_t)speed);
      last_speed_[id] = speed;
    }
    if (accel != last_accel_[id])
    {
      st->setAcceleration((int32_t)accel);
      last_accel_[id] = accel;
    }
    // Move absolute
    return st->moveTo(target) == MOVE_OK;
  }

  bool isMoving(uint8_t id) const override
  {
    if (id >= 8)
      return false;
    FastAccelStepper *st = steppers_[id];
    return st ? st->isRunning() : false;
  }

  long currentPosition(uint8_t id) const override
  {
    if (id >= 8)
      return 0;
    FastAccelStepper *st = steppers_[id];
    return st ? st->getCurrentPosition() : 0;
  }

  void setCurrentPosition(uint8_t id, long pos) override
  {
    if (id >= 8)
      return;
    FastAccelStepper *st = steppers_[id];
    if (st)
      st->setCurrentPosition(pos);
  }

  void attachShiftRegister(IShift595 *drv) override { g_shift = drv; }
  void setAutoEnable(uint8_t id, bool auto_enable) override
  {
    if (id >= 8)
      return;
    auto *st = steppers_[id];
    if (!st)
      return;
    st->setAutoEnable(auto_enable);
  }
  void enableOutputs(uint8_t id) override
  {
    if (id >= 8)
      return;
    auto *st = steppers_[id];
    if (!st)
      return;
    st->enableOutputs();
  }
  void disableOutputs(uint8_t id) override
  {
    if (id >= 8)
      return;
    auto *st = steppers_[id];
    if (!st)
      return;
    st->disableOutputs();
  }

private:
  FastAccelStepperEngine engine_;
  FastAccelStepper *steppers_[8];
  int step_pins_[8];
  int last_speed_[8];
  int last_accel_[8];
};

void FasAdapterEsp32::begin() {}
void FasAdapterEsp32::configureStepPin(uint8_t, int) {}
bool FasAdapterEsp32::startMoveAbs(uint8_t, long, int, int) { return false; }
bool FasAdapterEsp32::isMoving(uint8_t) const { return false; }
long FasAdapterEsp32::currentPosition(uint8_t) const { return 0; }
void FasAdapterEsp32::setCurrentPosition(uint8_t, long) {}

IFasAdapter *createEsp32FasAdapter()
{
  return new FasAdapterEsp32Impl();
}

// static
bool FasAdapterEsp32::externalPinHandler(uint8_t pin, uint8_t value)
{
  uint8_t p = (uint8_t)(pin & ~PIN_EXTERNAL_FLAG);
  bool high = (value != 0);
  if (p >= SLEEP_BASE)
  {
    uint8_t id = p - SLEEP_BASE;
    if (high)
      g_sleep_bits |= (1u << id);
    else
      g_sleep_bits &= (uint8_t)~(1u << id);
  }
  else
  {
    uint8_t id = p - DIR_BASE;
    if (high)
      g_dir_bits |= (1u << id);
    else
      g_dir_bits &= (uint8_t)~(1u << id);
  }
  if (g_shift)
  {
    g_shift->setDirSleep(g_dir_bits, g_sleep_bits);
  }
  return high;
}

#endif
