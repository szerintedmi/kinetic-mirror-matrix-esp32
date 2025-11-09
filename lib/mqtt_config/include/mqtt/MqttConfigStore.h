#pragma once

#include <cstdint>
#include <mutex>
#include <string>

namespace mqtt {

struct BrokerConfig {
  std::string host;
  uint16_t port = 0;
  std::string user;
  std::string pass;
  bool host_overridden = false;
  bool port_overridden = false;
  bool user_overridden = false;
  bool pass_overridden = false;
};

struct ConfigUpdate {
  bool host_set = false;
  bool host_use_default = false;
  std::string host;

  bool port_set = false;
  bool port_use_default = false;
  uint16_t port = 0;

  bool user_set = false;
  bool user_use_default = false;
  std::string user;

  bool pass_set = false;
  bool pass_use_default = false;
  std::string pass;
};

class ConfigStore {
public:
  static ConfigStore& Instance();

  BrokerConfig Current();
  BrokerConfig Defaults() const;
  uint32_t Revision() const;

  // Applies the provided update and persists it. Returns false on validation or persistence
  // failure.
  bool ApplyUpdate(const ConfigUpdate& update, std::string* error = nullptr);

  // Reloads preferences from storage, discarding cached overrides (used for test reset / reboot
  // simulation).
  bool Reload();

  void ResetForTests();

private:
  ConfigStore();

  BrokerConfig loadLocked();
  bool persistLocked(const BrokerConfig& config);
  bool writeSlotLocked(char slot, const BrokerConfig& config);
  bool readSlotLocked(char slot, BrokerConfig& config_out);
  void ensureLoadedLocked();
  bool validateUpdate(const ConfigUpdate& update, std::string* error) const;
  BrokerConfig mergeUpdate(const ConfigUpdate& update, std::string* error) const;

  BrokerConfig defaults_;
  BrokerConfig current_;
  bool loaded_ = false;
  uint32_t revision_ = 0;
  mutable std::mutex mutex_;
};

}  // namespace mqtt
