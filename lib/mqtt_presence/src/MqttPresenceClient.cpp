#include "mqtt/MqttPresenceClient.h"

#include "mqtt/PublishQueuePolicy.h"

#include "mqtt/MqttConfigStore.h"
#include "net_onboarding/NetOnboarding.h"
#include "net_onboarding/SerialImmediate.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <deque>
#include <string>
#include <utility>
#include <vector>

#if defined(ARDUINO)
#include "secrets.h"

#include <Arduino.h>
#include <AsyncMqttClient.h>
#endif

namespace mqtt {

namespace {

constexpr const char* kTopicPrefix = "devices/";
constexpr const char* kTopicSuffix = "/status";
constexpr uint16_t kMqttKeepAliveSeconds = 30;
constexpr const char* kOfflinePayloadJson = "{\"node_state\":\"offline\",\"motors\":{}}";
constexpr uint32_t kInitialReconnectDelayMs = 1000;
constexpr uint32_t kMaxReconnectDelayMs = 30000;
constexpr size_t kMaxQueuedMessages = 32;

}  // namespace

MqttPresenceClient::MqttPresenceClient(net_onboarding::NetOnboarding& net,
                                       PublishFn publish,
                                       LogFn log)
    : MqttPresenceClient(net, std::move(publish), std::move(log), Config{}) {}

MqttPresenceClient::MqttPresenceClient(net_onboarding::NetOnboarding& net,
                                       PublishFn publish,
                                       LogFn log,
                                       Config cfg)
    : net_(net), publish_(std::move(publish)), log_(std::move(log)), cfg_(cfg) {
  if (!publish_) {
    publish_ = [](const PublishMessage&) { return false; };
  }
  if (!log_) {
    log_ = [](const std::string& line) { net_onboarding::PrintCtrlLineImmediate(line); };
  }

  std::array<char, 18> mac{};
  net_.deviceMac(mac);
  mac_topic_ = NormalizeMacToTopic(mac);
  topic_ = std::string(kTopicPrefix) + mac_topic_ + kTopicSuffix;
  ready_publish_.topic = topic_;
  ready_publish_.retain = false;
  ready_publish_.qos = 0;
  ready_publish_.is_status = true;
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

  if (publish_pending_ || immediate_requested_) {
    if (!publishReady(now_ms)) {
      publish_pending_ = true;
    }
  }
}

void MqttPresenceClient::handleConnected(uint32_t now_ms, const std::string& broker_info) {
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

void MqttPresenceClient::handleConnectFailure(const std::string& details) {
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

std::string MqttPresenceClient::NormalizeMacToTopic(const std::array<char, 18>& mac) {
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

std::string MqttPresenceClient::BuildReadyPayload(const std::string& ip) {
  const char* ip_cstr = ip.empty() ? "0.0.0.0" : ip.c_str();
  size_t ip_len = ip.empty() ? 7 : ip.size();
  std::string payload;
  payload.reserve(32 + ip_len);
  payload.append("{\"node_state\":\"ready\",\"ip\":\"");
  payload.append(ip_cstr, ip_len);
  payload.append("\",\"motors\":{}}");
  return payload;
}

std::string MqttPresenceClient::BuildOfflinePayload() {
  return kOfflinePayloadJson;
}

bool MqttPresenceClient::publishReady(uint32_t now_ms) {
  if (!publish_) {
    return false;
  }
  ready_publish_.topic = topic_;
  std::string& payload = ready_publish_.payload;
  payload.clear();
  payload.reserve(32 + last_ip_.size());
  payload.append("{\"node_state\":\"ready\",\"ip\":\"");
  payload.append(last_ip_);
  payload.append("\",\"motors\":{}}");

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
      ready_publish_.is_status = true;
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
using ::std::string;
using ::std::strlen;

namespace {

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

string DeriveClientIdFromTopic(const string& topic) {
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

}  // namespace

class AsyncMqttPresenceClient::Impl {
public:
  Impl(net_onboarding::NetOnboarding& net, LogFn log)
      : net_(net), log_fn_(move(log)),
        logic_(
            net_,
            [this](const PublishMessage& pub) { return publish(pub); },
            [this](const string& line) { this->log(line); }) {
    if (!log_fn_) {
      log_fn_ = [](const string& line) { net_onboarding::PrintCtrlLineImmediate(line); };
    }
    setupCallbacks();
  }

  void begin() {
    logic_.refreshIdentity();
    mqtt::BrokerConfig cfg = mqtt::ConfigStore::Instance().Current();
    applyBrokerConfig(cfg, true);
    last_config_revision_ = mqtt::ConfigStore::Instance().Revision();
    refreshClientIdentity();
    client_.setCleanSession(true);
    client_.setKeepAlive(kMqttKeepAliveSeconds);
  }

  void loop(uint32_t now_ms) {
    maybeRefreshBrokerConfig();
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
      drainPublishQueue();
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

  bool enqueue(const PublishMessage& msg) {
    return EnqueuePublishMessage(publish_queue_, kMaxQueuedMessages, msg);
  }

  bool
  subscribe(const std::string& topic, uint8_t qos, AsyncMqttPresenceClient::MessageCallback cb) {
    if (topic.empty() || !cb) {
      return false;
    }
    subscriptions_.emplace_back(topic, qos, std::move(cb));
    if (client_.connected()) {
      client_.subscribe(topic.c_str(), qos);
    }
    return true;
  }

  const std::string& topic() const {
    return logic_.topic();
  }

  const std::string& offlinePayload() const {
    return logic_.offlinePayload();
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

  bool publish(const PublishMessage& pub) {
    if (!client_.connected()) {
      return false;
    }
    auto payload_len = static_cast<uint16_t>(pub.payload.size());
    uint16_t packet_id =
        client_.publish(pub.topic.c_str(), pub.qos, pub.retain, pub.payload.c_str(), payload_len);
    return packet_id != 0;
  }

  void log(const string& line) {
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
      for (const auto& sub : subscriptions_) {
        client_.subscribe(sub.topic.c_str(), sub.qos);
      }
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
    client_.onMessage([this](char* topic,
                             char* payload,
                             AsyncMqttClientMessageProperties /*properties*/,
                             size_t len,
                             size_t index,
                             size_t total) {
      if (index + len != total) {
        return;  // wait for final chunk
      }
      std::string topic_str(topic ? topic : "");
      std::string payload_str(payload && len ? std::string(payload, payload + len) : std::string());
      for (const auto& sub : subscriptions_) {
        if (topic_str == sub.topic && sub.callback) {
          sub.callback(topic_str, payload_str);
        }
      }
    });
  }

  void logConnectAttempt(const net_onboarding::Status& status) {
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
    const std::string& topic = logic_.topic();
    if (!topic.empty() && topic != last_will_topic_) {
      last_will_topic_ = topic;
      const std::string& offline = logic_.offlinePayload();
      client_.setWill(last_will_topic_.c_str(),
                      0,
                      false,
                      offline.c_str(),
                      static_cast<uint16_t>(offline.size()));
    }
    string base_client_id = DeriveClientIdFromTopic(topic);
    if (base_client_id.empty()) {
      base_client_id = "mqtt-presence";
    }
    const char* prefix = MQTT_CLIENT_PREFIX;
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

  void applyBrokerConfig(const mqtt::BrokerConfig& cfg, bool force_reconnect) {
    bool host_changed = (cfg.host != broker_host_) || (cfg.port != broker_port_);
    bool user_changed = (cfg.user != broker_user_);
    bool pass_changed = (cfg.pass != broker_pass_);
    if (!host_changed && !user_changed && !pass_changed && !force_reconnect) {
      return;
    }

    broker_host_ = cfg.host;
    broker_port_ = cfg.port;
    broker_user_ = cfg.user;
    broker_pass_ = cfg.pass;

    client_.setServer(broker_host_.c_str(), broker_port_);
    if (!broker_user_.empty() || !broker_pass_.empty()) {
      const char* user_ptr = broker_user_.empty() ? "" : broker_user_.c_str();
      const char* pass_ptr = broker_pass_.empty() ? nullptr : broker_pass_.c_str();
      client_.setCredentials(user_ptr, pass_ptr);
    } else {
      client_.setCredentials("", nullptr);
    }

    if (host_changed || user_changed || pass_changed || force_reconnect) {
      if (client_.connected()) {
        client_.disconnect();
      }
      connect_attempted_ = false;
      connect_succeeded_ = false;
      next_reconnect_ms_ = 0;
      reconnect_backoff_ms_ = kInitialReconnectDelayMs;
    }
  }

  void maybeRefreshBrokerConfig() {
    uint32_t revision = mqtt::ConfigStore::Instance().Revision();
    if (revision != last_config_revision_) {
      mqtt::BrokerConfig cfg = mqtt::ConfigStore::Instance().Current();
      applyBrokerConfig(cfg, true);
      last_config_revision_ = revision;
    }
  }

  void scheduleReconnect(uint32_t now_ms) {
    uint32_t delay = reconnect_backoff_ms_;
    next_reconnect_ms_ = now_ms + delay;
    logReconnectDelay(delay);
    reconnect_backoff_ms_ = std::min(reconnect_backoff_ms_ * 2, kMaxReconnectDelayMs);
  }

  void drainPublishQueue() {
    if (!client_.connected()) {
      return;
    }
    while (!publish_queue_.empty()) {
      PublishMessage msg = std::move(publish_queue_.front());
      publish_queue_.pop_front();
      auto payload_len = static_cast<uint16_t>(msg.payload.size());
      uint16_t packet_id =
          client_.publish(msg.topic.c_str(), msg.qos, msg.retain, msg.payload.c_str(), payload_len);
      if (packet_id == 0) {
        publish_queue_.push_front(std::move(msg));
        break;
      }
    }
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
  net_onboarding::NetOnboarding& net_;
  LogFn log_fn_;
  AsyncMqttClient client_;
  MqttPresenceClient logic_;
  std::deque<PublishMessage> publish_queue_;
  struct Subscription {
    Subscription() = default;
    Subscription(std::string t, uint8_t q, AsyncMqttPresenceClient::MessageCallback cb)
        : topic(std::move(t)), qos(q), callback(std::move(cb)) {}
    std::string topic;
    uint8_t qos = 0;
    AsyncMqttPresenceClient::MessageCallback callback;
  };
  std::vector<Subscription> subscriptions_;
  string last_will_topic_;
  string broker_host_;
  string broker_user_;
  string broker_pass_;
  uint16_t broker_port_ = 0;
  string client_id_;
  uint32_t last_config_revision_ = 0;
  uint32_t next_reconnect_ms_ = 0;
  uint32_t reconnect_backoff_ms_ = kInitialReconnectDelayMs;
  bool connect_attempted_ = false;
  bool connect_succeeded_ = false;
};

AsyncMqttPresenceClient::AsyncMqttPresenceClient(net_onboarding::NetOnboarding& net, LogFn log)
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

bool AsyncMqttPresenceClient::enqueuePublish(const PublishMessage& msg) {
  if (!impl_) {
    return false;
  }
  return impl_->enqueue(msg);
}

const std::string& AsyncMqttPresenceClient::statusTopic() const {
  static const std::string kEmpty;
  if (!impl_) {
    return kEmpty;
  }
  return impl_->topic();
}

const std::string& AsyncMqttPresenceClient::offlinePayload() const {
  static const std::string kEmpty;
  if (!impl_) {
    return kEmpty;
  }
  return impl_->offlinePayload();
}

bool AsyncMqttPresenceClient::subscribe(const std::string& topic, uint8_t qos, MessageCallback cb) {
  if (!impl_) {
    return false;
  }
  return impl_->subscribe(topic, qos, std::move(cb));
}

#endif  // ARDUINO

}  // namespace mqtt
