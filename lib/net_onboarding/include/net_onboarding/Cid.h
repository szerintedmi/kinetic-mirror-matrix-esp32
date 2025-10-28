#pragma once

#include <stdint.h>

namespace net_onboarding {

// Simple global command correlation ID tracker.
// Monotonic u32 (wrap allowed). Only one active CID at a time.
uint32_t NextCid();
void SetActiveCid(uint32_t cid);
bool HasActiveCid();
uint32_t ActiveCid();
void ClearActiveCid();

} // namespace net_onboarding

