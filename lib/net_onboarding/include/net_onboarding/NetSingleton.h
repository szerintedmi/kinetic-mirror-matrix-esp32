#pragma once

#include "net_onboarding/NetOnboarding.h"

namespace net_onboarding {

// Accessor for the single NetOnboarding instance used across the firmware.
// Implemented in src/NetSingleton.cpp
NetOnboarding& Net();

}  // namespace net_onboarding
