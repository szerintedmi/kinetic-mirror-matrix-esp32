#include "mqtt/MqttConfigStore.h"

#include <cctype>
#include <cstdlib>
#include <map>

#if defined(ARDUINO)
#include <Arduino.h>
#endif
#include "secrets.h"
#if (defined(ARDUINO) && (defined(ESP32) || defined(ARDUINO_ARCH_ESP32)) && __has_include(<Preferences.h>))
  #include <Preferences.h>
  #define MQTT_CONFIG_USE_PREFERENCES 1
#else
  #define MQTT_CONFIG_USE_PREFERENCES 0
#endif

namespace mqtt {

namespace {

constexpr const char *kNamespace = "mqtt";

using LockGuard = std::lock_guard<std::mutex>;

struct DefaultsProvider {
  static std::string Host() {
#ifdef MQTT_BROKER_HOST
    return MQTT_BROKER_HOST;
#else
    return "127.0.0.1";
#endif
  }

  static uint16_t Port() {
#ifdef MQTT_BROKER_PORT
    return static_cast<uint16_t>(MQTT_BROKER_PORT);
#else
    return 1883;
#endif
  }

  static std::string User() {
#ifdef MQTT_BROKER_USER
    return MQTT_BROKER_USER;
#else
    return "";
#endif
  }

  static std::string Pass() {
#ifdef MQTT_BROKER_PASS
    return MQTT_BROKER_PASS;
#else
    return "";
#endif
  }
};

class PreferencesAdapter {
public:
  PreferencesAdapter() = default;
  ~PreferencesAdapter() { end(); }

  bool begin(bool read_only) {
    read_only_ = read_only;
#if MQTT_CONFIG_USE_PREFERENCES
    return prefs_.begin(kNamespace, read_only);
#else
    (void)read_only;
    began_ = true;
    return true;
#endif
  }

  void end() {
#if MQTT_CONFIG_USE_PREFERENCES
    prefs_.end();
#else
    began_ = false;
#endif
  }

  bool putString(const std::string &key, const std::string &value) {
#if MQTT_CONFIG_USE_PREFERENCES
    if (read_only_) {
      return false;
    }
    return prefs_.putString(key.c_str(), value.c_str()) > 0;
#else
    if (!began_ || read_only_) {
      return false;
    }
    store()[key] = value;
    return true;
#endif
  }

  std::string getString(const std::string &key, const std::string &def) {
#if MQTT_CONFIG_USE_PREFERENCES
    return std::string(prefs_.getString(key.c_str(), def.c_str()).c_str());
#else
    const auto &map = store();
    auto it = map.find(key);
    if (it == map.end()) {
      return def;
    }
    return it->second;
#endif
  }

  bool remove(const std::string &key) {
#if MQTT_CONFIG_USE_PREFERENCES
    if (read_only_) {
      return false;
    }
    prefs_.remove(key.c_str());
    return true;
#else
    if (!began_ || read_only_) {
      return false;
    }
    store().erase(key);
    return true;
#endif
  }

  bool hasKey(const std::string &key) {
#if MQTT_CONFIG_USE_PREFERENCES
    return prefs_.isKey(key.c_str());
#else
    const auto &map = store();
    return map.find(key) != map.end();
#endif
  }

#if !MQTT_CONFIG_USE_PREFERENCES
  static void ResetStore() {
    store().clear();
  }
#endif

private:
#if !MQTT_CONFIG_USE_PREFERENCES
  static std::map<std::string, std::string> &store() {
    static std::map<std::string, std::string> data;
    return data;
  }
  bool began_ = false;
#else
  Preferences prefs_;
#endif
  bool read_only_ = false;
};

uint16_t ParsePort(const std::string &value, uint16_t fallback) {
  if (value.empty()) {
    return fallback;
  }
  char *end = nullptr;
  long parsed = std::strtol(value.c_str(), &end, 10);
  if (!end || *end != '\0' || parsed <= 0 || parsed > 65535) {
    return fallback;
  }
  return static_cast<uint16_t>(parsed);
}

void ApplyDefaultFlags(const BrokerConfig &defaults, BrokerConfig &config) {
  config.host_overridden = (config.host != defaults.host);
  config.port_overridden = (config.port != defaults.port);
  config.user_overridden = (config.user != defaults.user);
  config.pass_overridden = (config.pass != defaults.pass);
}

} // namespace

ConfigStore &ConfigStore::Instance() {
  static ConfigStore store;
  return store;
}

ConfigStore::ConfigStore() {
  defaults_.host = DefaultsProvider::Host();
  defaults_.port = DefaultsProvider::Port();
  defaults_.user = DefaultsProvider::User();
  defaults_.pass = DefaultsProvider::Pass();
  defaults_.host_overridden = false;
  defaults_.port_overridden = false;
  defaults_.user_overridden = false;
  defaults_.pass_overridden = false;
  current_ = defaults_;
  revision_ = 1;
}

BrokerConfig ConfigStore::Defaults() const {
  LockGuard lock(mutex_);
  return defaults_;
}

uint32_t ConfigStore::Revision() const {
  LockGuard lock(mutex_);
  return revision_;
}

BrokerConfig ConfigStore::Current() {
  LockGuard lock(mutex_);
  ensureLoadedLocked();
  return current_;
}

bool ConfigStore::Reload() {
  LockGuard lock(mutex_);
  loaded_ = false;
  ensureLoadedLocked();
  return true;
}

bool ConfigStore::ApplyUpdate(const ConfigUpdate &update, std::string *error) {
  LockGuard lock(mutex_);
  ensureLoadedLocked();

  if (!validateUpdate(update, error)) {
    return false;
  }

  BrokerConfig merged = mergeUpdate(update, error);
  if (error && !error->empty()) {
    return false;
  }

  if (!persistLocked(merged)) {
    if (error) {
      *error = "failed to persist mqtt config";
    }
    return false;
  }

  current_ = merged;
  ++revision_;
  return true;
}

bool ConfigStore::validateUpdate(const ConfigUpdate &update, std::string *error) const {
  auto setError = [&](const std::string &msg) {
    if (error) *error = msg;
  };
  if (update.host_set && !update.host_use_default) {
    if (update.host.empty()) {
      setError("host cannot be empty");
      return false;
    }
    if (update.host.size() > 190) {
      setError("host too long");
      return false;
    }
  }
  if (update.port_set && !update.port_use_default) {
    if (update.port == 0 || update.port > 65535) {
      setError("port out of range");
      return false;
    }
  }
  if (update.user_set && !update.user_use_default) {
    if (update.user.size() > 64) {
      setError("user too long");
      return false;
    }
  }
  if (update.pass_set && !update.pass_use_default) {
    if (update.pass.size() > 128) {
      setError("pass too long");
      return false;
    }
  }
  if (error) error->clear();
  return true;
}

BrokerConfig ConfigStore::mergeUpdate(const ConfigUpdate &update, std::string *error) const {
  BrokerConfig merged = current_;

  if (update.host_set) {
    if (update.host_use_default) {
      merged.host = defaults_.host;
    } else {
      merged.host = update.host;
    }
  }

  if (update.port_set) {
    if (update.port_use_default) {
      merged.port = defaults_.port;
    } else {
      merged.port = update.port;
    }
  }

  if (update.user_set) {
    if (update.user_use_default) {
      merged.user = defaults_.user;
    } else {
      merged.user = update.user;
    }
  }

  if (update.pass_set) {
    if (update.pass_use_default) {
      merged.pass = defaults_.pass;
    } else {
      merged.pass = update.pass;
    }
  }

  ApplyDefaultFlags(defaults_, merged);
  if (error) error->clear();
  return merged;
}

void ConfigStore::ensureLoadedLocked() {
  if (loaded_) {
    return;
  }
  BrokerConfig cfg = loadLocked();
  current_ = cfg;
  loaded_ = true;
  ++revision_;
}

BrokerConfig ConfigStore::loadLocked() {
  PreferencesAdapter prefs;
  if (!prefs.begin(true)) {
    BrokerConfig cfg = defaults_;
    ApplyDefaultFlags(defaults_, cfg);
    return cfg;
  }

  BrokerConfig cfg = defaults_;

  if (prefs.hasKey("host")) {
    cfg.host = prefs.getString("host", defaults_.host);
  } else {
    cfg.host = defaults_.host;
  }

  if (prefs.hasKey("port")) {
    cfg.port = ParsePort(prefs.getString("port", ""), defaults_.port);
  } else {
    cfg.port = defaults_.port;
  }

  if (prefs.hasKey("user")) {
    cfg.user = prefs.getString("user", defaults_.user);
  } else {
    cfg.user = defaults_.user;
  }

  if (prefs.hasKey("pass")) {
    cfg.pass = prefs.getString("pass", defaults_.pass);
  } else {
    cfg.pass = defaults_.pass;
  }

  prefs.end();

  ApplyDefaultFlags(defaults_, cfg);
  return cfg;
}

bool ConfigStore::persistLocked(const BrokerConfig &config) {
  PreferencesAdapter prefs;
  if (!prefs.begin(false)) {
    return false;
  }

  auto writeString = [&](const char *key, bool overridden, const std::string &value) {
    if (overridden) {
      prefs.putString(key, value);
    } else {
      prefs.remove(key);
    }
  };

  writeString("host", config.host_overridden, config.host);

  if (config.port_overridden) {
    prefs.putString("port", std::to_string(static_cast<unsigned long long>(config.port)));
  } else {
    prefs.remove("port");
  }

  writeString("user", config.user_overridden, config.user);
  writeString("pass", config.pass_overridden, config.pass);

  prefs.end();
  return true;
}

void ConfigStore::ResetForTests() {
  LockGuard lock(mutex_);
  current_ = defaults_;
  loaded_ = false;
  revision_ = 1;
#if !MQTT_CONFIG_USE_PREFERENCES
  PreferencesAdapter::ResetStore();
#endif
}

} // namespace mqtt
