#include "net_onboarding/NetSingleton.h"

namespace net_onboarding {

static NetOnboarding g_singleton;

NetOnboarding& Net() {
  return g_singleton;
}

}  // namespace net_onboarding
