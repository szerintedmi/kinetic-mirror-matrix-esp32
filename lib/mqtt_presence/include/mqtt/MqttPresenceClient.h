#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>

namespace net_onboarding {
class NetOnboarding;
}

namespace mqtt {

struct PublishMessage {
  std::string topic;
  std::string payload;
  uint8_t qos = 0;
  bool retain = false;
  bool is_status = false;
};

class MqttPresenceClient {
public:
  using PublishFn = std::function<bool(const PublishMessage&)>;
  using LogFn = std::function<void(const std::string&)>;

  struct Config {
    uint32_t heartbeat_interval_ms = 1000;
  };

  MqttPresenceClient(net_onboarding::NetOnboarding& net, PublishFn publish, LogFn log = {});
  MqttPresenceClient(net_onboarding::NetOnboarding& net, PublishFn publish, LogFn log, Config cfg);

  void setMotionActive(bool active);
  void setPowerActive(bool active);
  void forceImmediate();
  void loop(uint32_t now_ms);
  void refreshIdentity();

  void handleConnected(uint32_t now_ms, const std::string& broker_info = std::string());
  void handleDisconnected();
  void handleConnectFailure(const std::string& details = std::string());

  const std::string& topic() const {
    return topic_;
  }
  const std::string& offlinePayload() const {
    return offline_payload_;
  }
  const std::string& lastPublishedPayload() const {
    return last_payload_;
  }

  static std::string NormalizeMacToTopic(const std::array<char, 18>& mac);
  static std::string BuildReadyPayload(const std::string& ip);
  static std::string BuildOfflinePayload();

private:
  bool publishReady(uint32_t now_ms);
  void updateIdentityIfNeeded();

private:
  net_onboarding::NetOnboarding& net_;
  PublishFn publish_;
  LogFn log_;
  Config cfg_;

  std::string mac_topic_;
  std::string topic_;
  PublishMessage ready_publish_;
  std::string offline_payload_;
  std::string last_payload_;
  std::string last_ip_;

  bool motion_active_ = false;
  bool power_active_ = false;
  bool last_motion_active_ = false;
  bool last_power_active_ = false;
  bool connected_ = false;
  bool failure_logged_ = false;
  bool publish_pending_ = false;
  bool immediate_requested_ = false;

  uint32_t last_publish_ms_ = 0;
};

#if defined(ARDUINO)
class AsyncMqttPresenceClient {
public:
  using LogFn = MqttPresenceClient::LogFn;
  using PublishMessage = mqtt::PublishMessage;
  using MessageCallback = std::function<void(const std::string&, const std::string&)>;

  AsyncMqttPresenceClient(net_onboarding::NetOnboarding& net, LogFn log = {});
  ~AsyncMqttPresenceClient();
  void begin();
  void loop(uint32_t now_ms);
  void updateMotionState(bool active);
  void updatePowerState(bool active);
  bool enqueuePublish(const PublishMessage& msg);
  const std::string& statusTopic() const;
  const std::string& offlinePayload() const;
  bool subscribe(const std::string& topic, uint8_t qos, MessageCallback cb);

private:
  class Impl;
  Impl* impl_;
};
#endif

}  // namespace mqtt
