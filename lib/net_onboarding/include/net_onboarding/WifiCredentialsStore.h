#pragma once

#include "net_onboarding/Platform.h"
#include "net_onboarding/WifiCredentials.h"

#include <array>
#include <memory>

namespace net_onboarding {

/// Multi-slot credential persistence.
///
/// NVS Schema (namespace: "net"):
///   Primary: "ssid", "psk" (backward compatible with legacy single-SSID schema)
///   Secondary: "ssid_1", "psk_1"
class WifiCredentialsStore {
public:
  /// Construct with NVS adapter. Typically pass MakeNvs().
  explicit WifiCredentialsStore(std::unique_ptr<INvs> nvs);

  /// Load all credentials from NVS. Returns true if at least primary exists.
  bool load();

  /// Save credentials to a specific slot.
  bool save(CredentialSlot slot, const WifiCredentials& creds);

  /// Clear a specific slot.
  void clear(CredentialSlot slot);

  /// Clear all slots.
  void clearAll();

  /// Get credentials for a slot (returns empty credentials if not set).
  const WifiCredentials& get(CredentialSlot slot) const;

  /// Check if a slot has valid credentials.
  bool hasCredentials(CredentialSlot slot) const;

  /// Returns true if any slot has valid credentials.
  bool hasAnyCredentials() const;

  /// Get the last successfully connected slot (persisted across reboots).
  CredentialSlot lastConnectedSlot() const { return last_connected_slot_; }

  /// Set the last connected slot. Only writes to NVS if value changed.
  void setLastConnectedSlot(CredentialSlot slot);

private:
  std::unique_ptr<INvs> nvs_;
  std::array<WifiCredentials, static_cast<size_t>(CredentialSlot::COUNT)> credentials_;
  bool loaded_ = false;
  CredentialSlot last_connected_slot_ = CredentialSlot::PRIMARY;

  /// Get NVS keys for a slot.
  static const char* ssidKey_(CredentialSlot slot);
  static const char* pskKey_(CredentialSlot slot);
};

}  // namespace net_onboarding
