#pragma once

#include "MotorControl/MotorCommandProcessor.h"
#include "MotorControl/MotorControlConstants.h"
#include "mqtt/MqttPresenceClient.h"

#include <cstdint>
#include <functional>
#include <string>

namespace mqtt {

/**
 * Publishes device configuration to MQTT topic devices/<node_id>/config.
 *
 * Unlike MqttStatusPublisher which publishes at regular intervals,
 * this publisher only publishes when config values change (hash-based dedup).
 * Uses retain=true so new subscribers get current config immediately.
 *
 * Payload includes: thermal_limiting, max_budget_s, microstep, microstep_mult,
 * speed, accel, decel, motor_count, uptime_ms, firmware_version, firmware_date.
 *
 * Note: uptime_ms is a volatile field excluded from hash comparison to avoid
 * continuous republishing. It's appended just before publishing.
 */
class MqttConfigPublisher {
public:
  using PublishFn = std::function<bool(const PublishMessage&)>;

  MqttConfigPublisher(PublishFn publish, const MotorCommandProcessor& processor);

  void setTopic(const std::string& topic);
  void forceImmediate();

  void loop(uint32_t now_ms);

  const std::string& lastPayload() const {
    return last_payload_;
  }
  uint32_t lastPublishMs() const {
    return last_publish_ms_;
  }

private:
  bool buildSnapshot();
  void appendVolatileFields();
  bool publish();
  static const char* microstepToString(uint8_t multiplier);

private:
  PublishFn publish_;
  const MotorCommandProcessor& processor_;

  std::string topic_;
  std::string scratch_;
  std::string last_payload_;
  std::size_t last_hash_ = 0;
  bool has_last_hash_ = false;
  uint32_t last_publish_ms_ = 0;
  bool force_immediate_ = true;
};

}  // namespace mqtt
