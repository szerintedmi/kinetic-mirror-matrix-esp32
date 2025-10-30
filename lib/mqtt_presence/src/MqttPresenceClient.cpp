#include "mqtt/MqttPresenceClient.h"

#include "net_onboarding/MessageId.h"
#include "net_onboarding/NetOnboarding.h"
#include "net_onboarding/SerialImmediate.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <utility>
#include <string>

#if defined(ARDUINO)
#include <Arduino.h>
#include <AsyncMqttClient.h>
#include "secrets.h"
#endif

namespace mqtt {

namespace {

constexpr const char *kTopicPrefix = "devices/";
constexpr const char *kTopicSuffix = "/state";
constexpr uint16_t kMqttKeepAliveSeconds = 30;
constexpr const char *kOfflinePayloadJson = "{\"state\":\"offline\"}";
constexpr uint32_t kInitialReconnectDelayMs = 1000;
constexpr uint32_t kMaxReconnectDelayMs = 30000;

} // namespace

MqttPresenceClient::MqttPresenceClient(net_onboarding::NetOnboarding &net,
                                       PublishFn publish,
                                       LogFn log)
    : MqttPresenceClient(net, std::move(publish), std::move(log), Config{}) {}

MqttPresenceClient::MqttPresenceClient(net_onboarding::NetOnboarding &net,
                                       PublishFn publish,
                                       LogFn log,
                                       Config cfg)
    : net_(net),
      publish_(std::move(publish)),
      log_(std::move(log)),
      cfg_(cfg) {
  if (!publish_) {
    publish_ = [](const PresencePublish &) { return false; };
  }
  if (!log_) {
    log_ = [](const std::string &line) { net_onboarding::PrintCtrlLineImmediate(line); };
  }

  std::array<char, 18> mac{};
  net_.deviceMac(mac);
  mac_topic_ = NormalizeMacToTopic(mac);
  topic_ = std::string(kTopicPrefix) + mac_topic_ + kTopicSuffix;
  ready_publish_.topic = topic_;
  ready_publish_.retain = true;
  offline_payload_ = BuildOfflinePayload();
  last_ip_ = "0.0.0.0";
  updateIdentityIfNeeded();
}

void MqttPresenceClient::setMotionActive(bool active) {
  motion_active_ = active;
}

void MqttPresenceClient::setPowerActive(bool active) {
  power_active_ = active;
}

void MqttPresenceClient::forceImmediate() {
  immediate_requested_ = true;
}

void MqttPresenceClient::loop(uint32_t now_ms) {
  updateIdentityIfNeeded();

  if (!connected_) {
    return;
  }

  bool motion_changed = (motion_active_ != last_motion_active_);
  bool power_changed = (power_active_ != last_power_active_);
  if (motion_changed || power_changed) {
    immediate_requested_ = true;
  }

  uint32_t elapsed = connected_ ? (now_ms - last_publish_ms_) : 0;
  bool time_due = !last_publish_ms_ || (elapsed >= cfg_.heartbeat_interval_ms);

  if (publish_pending_ || immediate_requested_ || time_due) {
    if (!publishReady(now_ms)) {
      publish_pending_ = true;
    }
  }
}

void MqttPresenceClient::handleConnected(uint32_t now_ms, const std::string &broker_info) {
  connected_ = true;
  failure_logged_ = false;
  if (!broker_info.empty()) {
    log_("CTRL: MQTT_CONNECTED broker=" + broker_info);
  }
  publish_pending_ = true;
  immediate_requested_ = true;
  if (!last_publish_ms_) {
    last_publish_ms_ = now_ms;
  }
}

void MqttPresenceClient::handleDisconnected() {
  connected_ = false;
}

void MqttPresenceClient::handleConnectFailure(const std::string &details) {
  if (failure_logged_) {
    return;
  }
  failure_logged_ = true;
  std::string line("CTRL:WARN MQTT_CONNECT_FAILED");
  if (!details.empty()) {
    line += " ";
    line += details;
  }
  log_(line);
}

std::string MqttPresenceClient::NormalizeMacToTopic(const std::array<char, 18> &mac) {
  std::string out;
  out.reserve(12);
  for (char c : mac) {
    if (c == '\0') {
      break;
    }
    if (c == ':' || c == '-' || std::isspace(static_cast<unsigned char>(c))) {
      continue;
    }
    out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  }
  return out;
}

std::string MqttPresenceClient::BuildReadyPayload(const std::string &ip,
                                                  const std::string &msg_id) {
  const char *ip_cstr = ip.empty() ? "0.0.0.0" : ip.c_str();
  size_t ip_len = ip.empty() ? 7 : ip.size();
  std::string payload;
  payload.reserve(40 + ip_len + msg_id.size());
  payload.append("{\"state\":\"ready\",\"ip\":\"");
  payload.append(ip_cstr, ip_len);
  payload.append("\",\"msg_id\":\"");
  payload.append(msg_id);
  payload.append("\"}");
  return payload;
}

std::string MqttPresenceClient::BuildOfflinePayload() {
  return kOfflinePayloadJson;
}

bool MqttPresenceClient::publishReady(uint32_t now_ms) {
  if (!publish_) {
    return false;
  }
  std::string msg_id = net_onboarding::NextMsgId();
  ready_publish_.topic = topic_;
  std::string &payload = ready_publish_.payload;
  payload.clear();
  payload.reserve(40 + last_ip_.size() + msg_id.size());
  payload.append("{\"state\":\"ready\",\"ip\":\"");
  payload.append(last_ip_);
  payload.append("\",\"msg_id\":\"");
  payload.append(msg_id);
  payload.append("\"}");

  if (!publish_(ready_publish_)) {
    return false;
  }

  last_payload_ = payload;
  last_publish_ms_ = now_ms;
  publish_pending_ = false;
  immediate_requested_ = false;
  last_motion_active_ = motion_active_;
  last_power_active_ = power_active_;
  return true;
}

void MqttPresenceClient::refreshIdentity() {
  updateIdentityIfNeeded();
}

void MqttPresenceClient::updateIdentityIfNeeded() {
  auto status = net_.status();
  std::string ip = status.ip.data();
  if (ip.empty()) {
    ip = "0.0.0.0";
  }
  if (ip != last_ip_) {
    last_ip_ = ip;
    if (connected_) {
      immediate_requested_ = true;
    }
  }

  if (status.mac[0] != '\0') {
    std::array<char, 18> mac{};
    std::snprintf(mac.data(), mac.size(), "%s", status.mac.data());
    std::string normalized = NormalizeMacToTopic(mac);
    if (!normalized.empty() && normalized != mac_topic_) {
      mac_topic_ = normalized;
      topic_ = std::string(kTopicPrefix) + mac_topic_ + kTopicSuffix;
      ready_publish_.topic = topic_;
      publish_pending_ = true;
      if (connected_) {
        immediate_requested_ = true;
      }
    }
  }
}

#if defined(ARDUINO)

using ::AsyncMqttClient;
using ::AsyncMqttClientDisconnectReason;
using ::std::move;
using ::std::strlen;
using ::std::string;

namespace {

const char *brokerHost() {
#ifdef MQTT_BROKER_HOST
  return MQTT_BROKER_HOST;
#else
  return "127.0.0.1";
#endif
}

uint16_t brokerPort() {
#ifdef MQTT_BROKER_PORT
  return static_cast<uint16_t>(MQTT_BROKER_PORT);
#else
  return 1883;
#endif
}

const char *brokerUser() {
#ifdef MQTT_BROKER_USER
  return MQTT_BROKER_USER;
#else
  return "";
#endif
}

const char *brokerPass() {
#ifdef MQTT_BROKER_PASS
  return MQTT_BROKER_PASS;
#else
  return "";
#endif
}

string DisconnectReasonToString(AsyncMqttClientDisconnectReason reason) {
  switch (reason) {
  case AsyncMqttClientDisconnectReason::TCP_DISCONNECTED:
    return "tcp-disconnected";
  case AsyncMqttClientDisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION:
    return "mqtt-unacceptable-protocol-version";
  case AsyncMqttClientDisconnectReason::MQTT_IDENTIFIER_REJECTED:
    return "mqtt-identifier-rejected";
  case AsyncMqttClientDisconnectReason::MQTT_SERVER_UNAVAILABLE:
    return "mqtt-server-unavailable";
  case AsyncMqttClientDisconnectReason::MQTT_MALFORMED_CREDENTIALS:
    return "mqtt-malformed-credentials";
  case AsyncMqttClientDisconnectReason::MQTT_NOT_AUTHORIZED:
    return "mqtt-not-authorized";
  case AsyncMqttClientDisconnectReason::ESP8266_NOT_ENOUGH_SPACE:
    return "esp8266-not-enough-space";
  case AsyncMqttClientDisconnectReason::TLS_BAD_FINGERPRINT:
    return "tls-bad-fingerprint";
  default:
    return "";
  }
}

string NetStateToString(net_onboarding::State state) {
  switch (state) {
  case net_onboarding::State::AP_ACTIVE:
    return "ap-active";
  case net_onboarding::State::CONNECTING:
    return "connecting";
  case net_onboarding::State::CONNECTED:
    return "connected";
  default:
    return "unknown";
  }
}

string DeriveClientIdFromTopic(const string &topic) {
  if (topic.rfind(kTopicPrefix, 0) == 0) {
    string rest = topic.substr(strlen(kTopicPrefix));
    auto slash = rest.find('/');
    if (slash != string::npos) {
      rest = rest.substr(0, slash);
    }
    return rest;
  }
  return topic;
}

} // namespace

class AsyncMqttPresenceClient::Impl {
public:
  Impl(net_onboarding::NetOnboarding &net, LogFn log)
      : net_(net),
        log_fn_(move(log)),
        logic_(net_,
               [this](const PresencePublish &pub) { return publish(pub); },
               [this](const string &line) { this->log(line); }) {
    if (!log_fn_) {
      log_fn_ = [](const string &line) {
        net_onboarding::PrintCtrlLineImmediate(line);
      };
    }
    setupCallbacks();
  }

  void begin() {
    logic_.refreshIdentity();
    const char *host_ptr = brokerHost();
    broker_host_ = host_ptr ? host_ptr : "";
    broker_port_ = brokerPort();
    client_.setServer(broker_host_.c_str(), broker_port_);
    const char *user = brokerUser();
    const char *pass = brokerPass();
    if (user && user[0] != '\0') {
      broker_user_ = user;
    } else {
      broker_user_.clear();
    }
    const char *password = (pass && pass[0] != '\0') ? pass : nullptr;
    if ((user && user[0] != '\0') || password) {
      const char *user_for_client = !broker_user_.empty() ? broker_user_.c_str() : (user ? user : "");
      client_.setCredentials(user_for_client, password);
    }
    refreshClientIdentity();
    client_.setCleanSession(true);
    client_.setKeepAlive(kMqttKeepAliveSeconds);
  }

  void loop(uint32_t now_ms) {
    logic_.loop(now_ms);
    refreshClientIdentity();
    auto status = net_.status();
    if (status.state != net_onboarding::State::CONNECTED) {
      connect_attempted_ = false;
      connect_succeeded_ = false;
      next_reconnect_ms_ = 0;
      reconnect_backoff_ms_ = kInitialReconnectDelayMs;
      return;
    }

    if (client_.connected()) {
      return;
    }

    if (!connect_attempted_) {
      if (!next_reconnect_ms_ || now_ms >= next_reconnect_ms_) {
        logConnectAttempt(status);
        client_.connect();
        connect_attempted_ = true;
        next_reconnect_ms_ = 0;
      }
    }
  }

  void updateMotionState(bool active) {
    logic_.setMotionActive(active);
  }

  void updatePowerState(bool active) {
    logic_.setPowerActive(active);
  }

private:
  void logDisconnected(AsyncMqttClientDisconnectReason reason) {
    std::string line("CTRL: MQTT_DISCONNECTED");
    std::string summary = connectionSummary();
    if (!summary.empty()) {
      line += " ";
      line += summary;
    }
    std::string reason_str = DisconnectReasonToString(reason);
    if (!reason_str.empty()) {
      if (!summary.empty()) {
        line += " ";
      } else {
        line += " ";
      }
      line += "reason=";
      line += reason_str;
    }
    log(line);
  }

  void logReconnectDelay(uint32_t delay_ms) {
    std::string line("CTRL: MQTT_RECONNECT");
    std::string summary = connectionSummary();
    if (!summary.empty()) {
      line += " ";
      line += summary;
    }
    line += " delay=";
    line += std::to_string(delay_ms);
    log(line);
  }

  bool publish(const PresencePublish &pub) {
    if (!client_.connected()) {
      return false;
    }
    auto payload_len = static_cast<uint16_t>(pub.payload.size());
    uint16_t packet_id = client_.publish(
        pub.topic.c_str(),
        1,
        pub.retain,
        pub.payload.c_str(),
        payload_len);
    return packet_id != 0;
  }

  void log(const string &line) {
    log_fn_(line);
  }

  void setupCallbacks() {
    client_.onConnect([this](bool /*sessionPresent*/) {
      connect_succeeded_ = true;
      connect_attempted_ = true;
      reconnect_backoff_ms_ = kInitialReconnectDelayMs;
      next_reconnect_ms_ = 0;
      std::string broker_info;
      if (!broker_host_.empty()) {
        broker_info = broker_host_;
      }
      broker_info += ":" + std::to_string(broker_port_);
      logic_.handleConnected(millis(), broker_info);
    });
    client_.onDisconnect([this](AsyncMqttClientDisconnectReason reason) {
      bool was_connected = connect_succeeded_;
      logic_.handleDisconnected();
      connect_attempted_ = false;
      connect_succeeded_ = false;
      logDisconnected(reason);

      auto status = net_.status();
      if (status.state == net_onboarding::State::CONNECTED) {
        scheduleReconnect(millis());
      } else {
        next_reconnect_ms_ = 0;
        reconnect_backoff_ms_ = kInitialReconnectDelayMs;
      }

      std::string context = connectionSummary();
      std::string reason_str = DisconnectReasonToString(reason);
      if (!reason_str.empty()) {
        if (!context.empty()) {
          context += " ";
        }
        context += "reason=";
        context += reason_str;
      }
      (void)was_connected;
      logic_.handleConnectFailure(context);
    });
  }

  void logConnectAttempt(const net_onboarding::Status &status) {
    std::string line("CTRL: MQTT_CONNECTING");
    std::string summary = connectionSummary();
    if (!summary.empty()) {
      line += " ";
      line += summary;
    }
    line += " wifi_state=";
    line += NetStateToString(status.state);
    if (status.ip[0] != '\0') {
      line += " ip=";
      line += status.ip.data();
    }
    log(line);
  }

  void refreshClientIdentity() {
    if (client_.connected()) {
      return;
    }
    const std::string &topic = logic_.topic();
    if (!topic.empty() && topic != last_will_topic_) {
      last_will_topic_ = topic;
      const std::string &offline = logic_.offlinePayload();
      client_.setWill(last_will_topic_.c_str(), 1, true,
                      offline.c_str(), static_cast<uint16_t>(offline.size()));
    }
    string base_client_id = DeriveClientIdFromTopic(topic);
    if (base_client_id.empty()) {
      base_client_id = "mqtt-presence";
    }
    const char *prefix = MQTT_CLIENT_PREFIX;
    string desired_client_id;
    if (prefix && prefix[0] != '\0') {
      desired_client_id.reserve(std::strlen(prefix) + base_client_id.size());
      desired_client_id.assign(prefix);
      desired_client_id.append(base_client_id);
    } else {
      desired_client_id = base_client_id;
    }
    if (desired_client_id != client_id_) {
      client_id_ = std::move(desired_client_id);
      client_.setClientId(client_id_.c_str());
    }
  }

  void scheduleReconnect(uint32_t now_ms) {
    uint32_t delay = reconnect_backoff_ms_;
    next_reconnect_ms_ = now_ms + delay;
    logReconnectDelay(delay);
    reconnect_backoff_ms_ = std::min(reconnect_backoff_ms_ * 2, kMaxReconnectDelayMs);
  }

  std::string connectionSummary() const {
    std::string summary;
    if (!broker_host_.empty() || broker_port_ != 0) {
      summary += "broker=";
      if (!broker_host_.empty()) {
        summary += broker_host_;
      }
      summary += ":";
      summary += std::to_string(broker_port_);
    }
    if (!broker_user_.empty()) {
      if (!summary.empty()) {
        summary += " ";
      }
      summary += "user=";
      summary += broker_user_;
    }
    if (!client_id_.empty()) {
      if (!summary.empty()) {
        summary += " ";
      }
      summary += "client_id=";
      summary += client_id_;
    }
    return summary;
  }

private:
  net_onboarding::NetOnboarding &net_;
  LogFn log_fn_;
  AsyncMqttClient client_;
  MqttPresenceClient logic_;
  string last_will_topic_;
  string broker_host_;
  string broker_user_;
  uint16_t broker_port_ = 0;
  string client_id_;
  uint32_t next_reconnect_ms_ = 0;
  uint32_t reconnect_backoff_ms_ = kInitialReconnectDelayMs;
  bool connect_attempted_ = false;
  bool connect_succeeded_ = false;
};

AsyncMqttPresenceClient::AsyncMqttPresenceClient(net_onboarding::NetOnboarding &net,
                                                 LogFn log)
    : impl_(new Impl(net, move(log))) {}

AsyncMqttPresenceClient::~AsyncMqttPresenceClient() {
  delete impl_;
}

void AsyncMqttPresenceClient::begin() {
  impl_->begin();
}

void AsyncMqttPresenceClient::loop(uint32_t now_ms) {
  impl_->loop(now_ms);
}

void AsyncMqttPresenceClient::updateMotionState(bool active) {
  impl_->updateMotionState(active);
}

void AsyncMqttPresenceClient::updatePowerState(bool active) {
  impl_->updatePowerState(active);
}

#endif // ARDUINO

} // namespace mqtt
