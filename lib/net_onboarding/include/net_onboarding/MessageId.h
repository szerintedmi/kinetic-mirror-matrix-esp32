#pragma once

#include <functional>
#include <string>

namespace net_onboarding {

// UUID-backed message identifiers shared between MQTT and serial transports.
std::string NextMsgId();
void SetActiveMsgId(const std::string &msg_id);
bool HasActiveMsgId();
std::string ActiveMsgId();
void ClearActiveMsgId();

// Test-only hooks to override UUID generation.
void SetMsgIdGenerator(std::function<std::string()> generator);
void ResetMsgIdGenerator();

} // namespace net_onboarding
