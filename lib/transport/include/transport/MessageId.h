#pragma once

#include <functional>
#include <string>

namespace transport {
namespace message_id {

// UUID-backed message identifiers shared across transports.
std::string Next();
void SetActive(const std::string &msg_id);
bool HasActive();
std::string Active();
void ClearActive();

// Test-only hooks to override UUID generation.
void SetGenerator(std::function<std::string()> generator);
void ResetGenerator();

} // namespace message_id
} // namespace transport

