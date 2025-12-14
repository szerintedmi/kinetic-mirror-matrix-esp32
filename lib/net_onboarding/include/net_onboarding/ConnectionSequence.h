#pragma once

#include "net_onboarding/WifiCredentials.h"

#include <stdint.h>

namespace net_onboarding {

/// Manages the connection attempt sequence: primary → secondary → exhausted.
///
/// This class encapsulates the retry logic for multi-network failover:
/// - On boot: try primary (5s), then secondary (5s), then give up
/// - On disconnect: retry current network first, then try other, then give up
///
/// The "stay on secondary until reboot" policy is enforced by markConnected().
class ConnectionSequence {
public:
  enum class State : uint8_t {
    IDLE,             // Not attempting connection
    TRYING_PRIMARY,   // Currently trying primary network
    TRYING_SECONDARY, // Currently trying secondary network
    EXHAUSTED         // All attempts failed, should enter AP mode
  };

  /// Construct with timeout per network (default 5000ms).
  explicit ConnectionSequence(uint32_t timeout_per_network_ms = 5000);

  /// Start connection sequence with available credentials.
  /// Starts with last_connected_slot if that slot is valid; otherwise tries primary first.
  /// If neither is valid, immediately goes to EXHAUSTED.
  void begin(const WifiCredentials& primary, const WifiCredentials& secondary, uint32_t now_ms);

  /// Check if current attempt has timed out and advance to next network.
  /// Returns true if state changed (caller should start new connection attempt).
  bool checkTimeout(uint32_t now_ms);

  /// Get current state.
  State state() const { return state_; }

  /// Get credentials for current attempt. Returns nullptr if IDLE or EXHAUSTED.
  const WifiCredentials* currentCredentials() const;

  /// Get the slot currently being attempted.
  CredentialSlot currentSlot() const;

  /// Mark connection as successful to a specific slot.
  void markConnected(CredentialSlot slot);

  /// Get the slot that was last successfully connected.
  CredentialSlot lastConnectedSlot() const { return last_connected_slot_; }

  /// Set the last connected slot (typically loaded from NVS on boot).
  /// Call before begin() to influence which network is tried first.
  void setLastConnectedSlot(CredentialSlot slot) { last_connected_slot_ = slot; }

  /// Handle disconnect event. Sets up retry sequence:
  /// 1. Retry current network first
  /// 2. Then try other network
  /// 3. Then EXHAUSTED
  void onDisconnect(uint32_t now_ms);

  /// Update secondary credentials without restarting sequence.
  /// Use when secondary network is configured while already connected.
  void updateSecondary(const WifiCredentials& secondary);

  /// Reset to IDLE state.
  void reset();

  /// Get timeout per network in ms.
  uint32_t timeoutPerNetworkMs() const { return timeout_per_network_ms_; }

  /// Set timeout per network in ms.
  void setTimeoutPerNetworkMs(uint32_t ms) { timeout_per_network_ms_ = ms; }

private:
  State state_ = State::IDLE;
  uint32_t timeout_per_network_ms_;
  uint32_t attempt_start_ms_ = 0;

  WifiCredentials primary_;
  WifiCredentials secondary_;

  CredentialSlot last_connected_slot_ = CredentialSlot::PRIMARY;
  bool tried_primary_this_cycle_ = false;
  bool tried_secondary_this_cycle_ = false;

  void advanceToNextNetwork_(uint32_t now_ms);
};

}  // namespace net_onboarding
