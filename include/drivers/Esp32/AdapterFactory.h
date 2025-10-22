#pragma once
#include "Hal/FasAdapter.h"

// Factory that returns the motion adapter for ESP32 based on build flags.
// When USE_SHARED_STEP=1, returns the SharedStep adapter; otherwise returns
// the FastAccelStepper-based adapter.

IFasAdapter* createEsp32MotionAdapter();

