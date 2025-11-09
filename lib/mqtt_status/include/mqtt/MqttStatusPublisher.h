#pragma once

#include "MotorControl/MotorController.h"
#include "mqtt/MqttPresenceClient.h"
#include "net_onboarding/NetOnboarding.h"

#include <cstdint>
#include <functional>
#include <string>

namespace mqtt {

class MqttStatusPublisher {
public:
  using PublishFn = std::function<bool(const PublishMessage&)>;

  struct Config {
    uint32_t idle_interval_ms = 1000;   // 1 Hz when idle
    uint32_t motion_interval_ms = 200;  // 5 Hz during motion
    size_t max_motors = 8;
  };

  MqttStatusPublisher(PublishFn publish, net_onboarding::NetOnboarding& net);
  MqttStatusPublisher(PublishFn publish, net_onboarding::NetOnboarding& net, Config cfg);

  void setTopic(const std::string& topic);
  void forceImmediate();

  void loop(const MotorController& controller, uint32_t now_ms);

  const std::string& lastPayload() const {
    return last_payload_;
  }
  uint32_t lastPublishMs() const {
    return last_publish_ms_;
  }

private:
  bool buildSnapshot(const MotorController& controller, bool& out_motion_active);
  bool publish();
  void appendMotorJson(const MotorState& state,
                       int32_t budget_tenths,
                       int32_t ttfc_tenths,
                       bool include_actual_ms,
                       std::string& out);
  static void appendFixedTenths(int32_t tenths, std::string& out);

private:
  PublishFn publish_;
  net_onboarding::NetOnboarding& net_;
  Config cfg_;

  std::string topic_;
  std::string scratch_;
  std::string last_payload_;
  std::size_t last_hash_ = 0;
  bool has_last_hash_ = false;
  uint32_t last_publish_ms_ = 0;
  bool force_immediate_ = true;
};

}  // namespace mqtt
