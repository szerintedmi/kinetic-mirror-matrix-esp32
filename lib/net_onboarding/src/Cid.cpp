#include "net_onboarding/Cid.h"

namespace net_onboarding {

static uint32_t g_next_cid = 1;
static uint32_t g_active_cid = 0;

uint32_t NextCid() {
  uint32_t id = g_next_cid++;
  if (g_next_cid == 0) g_next_cid = 1; // avoid 0 for next
  return id;
}

void SetActiveCid(uint32_t cid) { g_active_cid = cid; }
bool HasActiveCid() { return g_active_cid != 0; }
uint32_t ActiveCid() { return g_active_cid; }
void ClearActiveCid() { g_active_cid = 0; }

} // namespace net_onboarding

