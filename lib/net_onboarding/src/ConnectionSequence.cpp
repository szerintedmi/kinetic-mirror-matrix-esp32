#include "net_onboarding/ConnectionSequence.h"

namespace net_onboarding {

ConnectionSequence::ConnectionSequence(uint32_t timeout_per_network_ms)
    : timeout_per_network_ms_(timeout_per_network_ms) {}

void ConnectionSequence::begin(const WifiCredentials& primary,
                               const WifiCredentials& secondary,
                               uint32_t now_ms) {
  primary_ = primary;
  secondary_ = secondary;
  tried_primary_this_cycle_ = false;
  tried_secondary_this_cycle_ = false;

  // Start with last connected slot if it's valid, otherwise fall back to primary
  if (last_connected_slot_ == CredentialSlot::SECONDARY && secondary_.isValid()) {
    state_ = State::TRYING_SECONDARY;
    tried_secondary_this_cycle_ = true;
  } else if (primary_.isValid()) {
    state_ = State::TRYING_PRIMARY;
    tried_primary_this_cycle_ = true;
  } else if (secondary_.isValid()) {
    state_ = State::TRYING_SECONDARY;
    tried_secondary_this_cycle_ = true;
  } else {
    state_ = State::EXHAUSTED;
    return;
  }

  attempt_start_ms_ = now_ms;
}

bool ConnectionSequence::checkTimeout(uint32_t now_ms) {
  if (state_ == State::IDLE || state_ == State::EXHAUSTED) {
    return false;
  }

  uint32_t elapsed = now_ms - attempt_start_ms_;
  if (elapsed < timeout_per_network_ms_) {
    return false;
  }

  // Current attempt timed out, advance to next
  advanceToNextNetwork_(now_ms);
  return true;
}

void ConnectionSequence::advanceToNextNetwork_(uint32_t now_ms) {
  if (state_ == State::TRYING_PRIMARY) {
    tried_primary_this_cycle_ = true;
    if (secondary_.isValid() && !tried_secondary_this_cycle_) {
      state_ = State::TRYING_SECONDARY;
      tried_secondary_this_cycle_ = true;
      attempt_start_ms_ = now_ms;
      return;
    }
  } else if (state_ == State::TRYING_SECONDARY) {
    tried_secondary_this_cycle_ = true;
    if (primary_.isValid() && !tried_primary_this_cycle_) {
      state_ = State::TRYING_PRIMARY;
      tried_primary_this_cycle_ = true;
      attempt_start_ms_ = now_ms;
      return;
    }
  }

  // No more networks to try
  state_ = State::EXHAUSTED;
}

const WifiCredentials* ConnectionSequence::currentCredentials() const {
  switch (state_) {
  case State::TRYING_PRIMARY:
    return &primary_;
  case State::TRYING_SECONDARY:
    return &secondary_;
  default:
    return nullptr;
  }
}

CredentialSlot ConnectionSequence::currentSlot() const {
  switch (state_) {
  case State::TRYING_PRIMARY:
    return CredentialSlot::PRIMARY;
  case State::TRYING_SECONDARY:
    return CredentialSlot::SECONDARY;
  default:
    return last_connected_slot_;
  }
}

void ConnectionSequence::markConnected(CredentialSlot slot) {
  last_connected_slot_ = slot;
  state_ = State::IDLE;
}

void ConnectionSequence::onDisconnect(uint32_t now_ms) {
  // Reset cycle tracking
  tried_primary_this_cycle_ = false;
  tried_secondary_this_cycle_ = false;

  // Retry current network first
  if (last_connected_slot_ == CredentialSlot::PRIMARY && primary_.isValid()) {
    state_ = State::TRYING_PRIMARY;
    tried_primary_this_cycle_ = true;
  } else if (last_connected_slot_ == CredentialSlot::SECONDARY && secondary_.isValid()) {
    state_ = State::TRYING_SECONDARY;
    tried_secondary_this_cycle_ = true;
  } else if (primary_.isValid()) {
    // Fallback to primary if last connected slot isn't available
    state_ = State::TRYING_PRIMARY;
    tried_primary_this_cycle_ = true;
  } else if (secondary_.isValid()) {
    state_ = State::TRYING_SECONDARY;
    tried_secondary_this_cycle_ = true;
  } else {
    state_ = State::EXHAUSTED;
    return;
  }

  attempt_start_ms_ = now_ms;
}

void ConnectionSequence::updateSecondary(const WifiCredentials& secondary) {
  secondary_ = secondary;
}

void ConnectionSequence::reset() {
  state_ = State::IDLE;
  tried_primary_this_cycle_ = false;
  tried_secondary_this_cycle_ = false;
}

}  // namespace net_onboarding
