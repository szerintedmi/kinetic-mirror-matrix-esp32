#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string>

namespace net_onboarding {

/// Credential slot identifiers for multi-network support.
enum class CredentialSlot : uint8_t {
  PRIMARY = 0,
  SECONDARY = 1,
  COUNT = 2  // Number of supported slots
};

/// Value object representing Wi-Fi credentials for a single network.
struct WifiCredentials {
  std::string ssid;
  std::string password;

  /// Returns true if the SSID is non-empty and within valid length (1-32 chars).
  bool isValid() const { return !ssid.empty() && ssid.size() <= 32; }

  /// Returns true if password is valid for WPA2 (empty for open networks, or 8-63 chars).
  bool isPasswordValid() const {
    return password.empty() || (password.size() >= 8 && password.size() <= 63);
  }

  /// Returns true if credentials are empty (slot not configured).
  bool isEmpty() const { return ssid.empty(); }

  /// Clear credentials.
  void clear() {
    ssid.clear();
    password.clear();
  }
};

}  // namespace net_onboarding
