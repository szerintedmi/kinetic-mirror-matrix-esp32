#include "net_onboarding/WifiCredentialsStore.h"

#include <cstdio>
#include <cstdlib>

namespace net_onboarding {

namespace {
constexpr const char* kNamespace = "net";
constexpr const char* kLastSlotKey = "last_slot";
}  // namespace

WifiCredentialsStore::WifiCredentialsStore(std::unique_ptr<INvs> nvs) : nvs_(std::move(nvs)) {}

const char* WifiCredentialsStore::ssidKey_(CredentialSlot slot) {
  switch (slot) {
  case CredentialSlot::PRIMARY:
    return "ssid";  // Use legacy key for backward compatibility
  case CredentialSlot::SECONDARY:
    return "ssid_1";
  default:
    return "ssid";
  }
}

const char* WifiCredentialsStore::pskKey_(CredentialSlot slot) {
  switch (slot) {
  case CredentialSlot::PRIMARY:
    return "psk";  // Use legacy key for backward compatibility
  case CredentialSlot::SECONDARY:
    return "psk_1";
  default:
    return "psk";
  }
}

bool WifiCredentialsStore::load() {
  if (!nvs_)
    return false;

  if (!nvs_->begin(kNamespace, true))
    return false;

  // Load all slots
  for (size_t i = 0; i < static_cast<size_t>(CredentialSlot::COUNT); ++i) {
    auto slot = static_cast<CredentialSlot>(i);
    credentials_[i].ssid = nvs_->getString(ssidKey_(slot), "");
    credentials_[i].password = nvs_->getString(pskKey_(slot), "");
  }

  // Load last connected slot
  std::string slot_str = nvs_->getString(kLastSlotKey, "0");
  int slot_val = std::atoi(slot_str.c_str());
  if (slot_val >= 0 && slot_val < static_cast<int>(CredentialSlot::COUNT)) {
    last_connected_slot_ = static_cast<CredentialSlot>(slot_val);
  } else {
    last_connected_slot_ = CredentialSlot::PRIMARY;
  }

  nvs_->end();
  loaded_ = true;

  return hasCredentials(CredentialSlot::PRIMARY);
}

bool WifiCredentialsStore::save(CredentialSlot slot, const WifiCredentials& creds) {
  if (!nvs_)
    return false;

  size_t idx = static_cast<size_t>(slot);
  if (idx >= credentials_.size())
    return false;

  if (!nvs_->begin(kNamespace, false))
    return false;

  bool ok1 = nvs_->putString(ssidKey_(slot), creds.ssid.c_str());
  bool ok2 = nvs_->putString(pskKey_(slot), creds.password.c_str());
  nvs_->end();

  // Only update in-memory state if BOTH writes succeeded
  if (ok1 && ok2) {
    credentials_[idx] = creds;
  }

  return ok1 && ok2;
}

void WifiCredentialsStore::clear(CredentialSlot slot) {
  if (!nvs_)
    return;

  size_t idx = static_cast<size_t>(slot);
  if (idx >= credentials_.size())
    return;

  if (!nvs_->begin(kNamespace, false))
    return;

  nvs_->remove(ssidKey_(slot));
  nvs_->remove(pskKey_(slot));
  nvs_->end();

  credentials_[idx].clear();
}

void WifiCredentialsStore::clearAll() {
  if (!nvs_)
    return;

  if (!nvs_->begin(kNamespace, false))
    return;

  for (size_t i = 0; i < static_cast<size_t>(CredentialSlot::COUNT); ++i) {
    auto slot = static_cast<CredentialSlot>(i);
    nvs_->remove(ssidKey_(slot));
    nvs_->remove(pskKey_(slot));
    credentials_[i].clear();
  }

  nvs_->end();
}

const WifiCredentials& WifiCredentialsStore::get(CredentialSlot slot) const {
  size_t idx = static_cast<size_t>(slot);
  if (idx >= credentials_.size()) {
    static WifiCredentials empty;
    return empty;
  }
  return credentials_[idx];
}

bool WifiCredentialsStore::hasCredentials(CredentialSlot slot) const {
  return get(slot).isValid();
}

bool WifiCredentialsStore::hasAnyCredentials() const {
  for (size_t i = 0; i < static_cast<size_t>(CredentialSlot::COUNT); ++i) {
    if (credentials_[i].isValid()) {
      return true;
    }
  }
  return false;
}

void WifiCredentialsStore::setLastConnectedSlot(CredentialSlot slot) {
  // Only write to NVS if the value actually changed
  if (slot == last_connected_slot_) {
    return;
  }

  last_connected_slot_ = slot;

  if (!nvs_ || !nvs_->begin(kNamespace, false)) {
    return;
  }

  char buf[4];
  std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(slot));
  nvs_->putString(kLastSlotKey, buf);
  nvs_->end();
}

}  // namespace net_onboarding
