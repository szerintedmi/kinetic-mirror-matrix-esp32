#if defined(ARDUINO)
#include "drivers/Esp32/AdapterFactory.h"
#include "MotorControl/BuildConfig.h"

// FAS adapter factory is provided by FasAdapterEsp32.cpp
extern IFasAdapter* createEsp32FasAdapter();

#if (USE_SHARED_STEP)
#include "drivers/Esp32/SharedStepAdapterEsp32.h"
#endif

IFasAdapter* createEsp32MotionAdapter() {
#if (USE_SHARED_STEP)
  return new SharedStepAdapterEsp32();
#else
  return createEsp32FasAdapter();
#endif
}

#endif // ARDUINO

