#include "mqtt/MqttConfigPublisher.h"

#include <functional>
#include <utility>

#include "version.h"

#if defined(ARDUINO)
#include <Arduino.h>
#endif

namespace mqtt {

namespace {

unsigned long GetUptimeMs() {
#if defined(ARDUINO)
  return millis();
#else
  return 0;
#endif
}

void AppendUnsignedDigits(std::string& out, unsigned long long value) {
  char buffer[21];
  size_t len = 0;
  do {
    buffer[len++] = static_cast<char>('0' + (value % 10ULL));
    value /= 10ULL;
  } while (value != 0ULL);
  while (len > 0) {
    out.push_back(buffer[--len]);
  }
}

void AppendSignedDigits(std::string& out, long long value) {
  unsigned long long magnitude;
  if (value < 0) {
    out.push_back('-');
    magnitude = static_cast<unsigned long long>(-(value + 1LL)) + 1ULL;
  } else {
    magnitude = static_cast<unsigned long long>(value);
  }
  AppendUnsignedDigits(out, magnitude);
}

}  // namespace

MqttConfigPublisher::MqttConfigPublisher(PublishFn publish,
                                         const MotorCommandProcessor& processor)
    : publish_(std::move(publish)), processor_(processor) {
  if (!publish_) {
    publish_ = [](const PublishMessage&) { return false; };
  }
  scratch_.reserve(256);
  last_payload_.reserve(256);
}

void MqttConfigPublisher::setTopic(const std::string& topic) {
  if (topic != topic_) {
    topic_ = topic;
    force_immediate_ = true;
  }
}

void MqttConfigPublisher::forceImmediate() {
  force_immediate_ = true;
}

void MqttConfigPublisher::loop(uint32_t now_ms) {
  if (topic_.empty()) {
    return;
  }
  if (!buildSnapshot()) {
    return;
  }

  // Hash only the stable part (before uptime_ms is appended)
  std::size_t hash = std::hash<std::string>{}(scratch_);
  bool changed = !has_last_hash_ || hash != last_hash_;

  // Only publish on change or when forced (no interval-based publishing)
  if (!force_immediate_ && !changed) {
    return;
  }

  // Append volatile fields (uptime) just before publishing
  appendVolatileFields();

  if (!publish()) {
    return;
  }

  last_hash_ = hash;
  has_last_hash_ = true;
  last_payload_ = scratch_;
  last_publish_ms_ = now_ms;
  force_immediate_ = false;
}

bool MqttConfigPublisher::buildSnapshot() {
  scratch_.clear();
  scratch_.reserve(256);

  // Build JSON payload (stable fields only - used for hash comparison)
  scratch_.append("{\"thermal_limiting\":\"");
  scratch_.append(processor_.thermalLimitsEnabled() ? "ON" : "OFF");
  scratch_.append("\",\"max_budget_s\":");
  AppendSignedDigits(scratch_, static_cast<int>(MotorControlConstants::MAX_RUNNING_TIME_S));
  scratch_.append(",\"microstep\":\"");
  scratch_.append(microstepToString(processor_.microstepMultiplier()));
  scratch_.append("\",\"microstep_mult\":");
  AppendUnsignedDigits(scratch_, processor_.microstepMultiplier());
  scratch_.append(",\"speed\":");
  AppendSignedDigits(scratch_, processor_.defaultSpeedSps());
  scratch_.append(",\"accel\":");
  AppendSignedDigits(scratch_, processor_.defaultAccelSps2());
  scratch_.append(",\"decel\":");
  AppendSignedDigits(scratch_, processor_.defaultDecelSps2());
  scratch_.append(",\"motor_count\":");
  AppendUnsignedDigits(scratch_, processor_.controller().motorCount());
#ifdef GIT_COMMIT_HASH
  scratch_.append(",\"firmware_version\":\"");
  scratch_.append(GIT_COMMIT_HASH);
  scratch_.push_back('"');
#endif
#ifdef GIT_COMMIT_DATE
  scratch_.append(",\"firmware_date\":\"");
  scratch_.append(GIT_COMMIT_DATE);
  scratch_.push_back('"');
#endif
  // Note: closing brace added by appendVolatileFields()

  return true;
}

void MqttConfigPublisher::appendVolatileFields() {
  // Append volatile fields that change frequently (not included in hash)
  scratch_.append(",\"uptime_ms\":");
  AppendUnsignedDigits(scratch_, GetUptimeMs());
  scratch_.push_back('}');
}

bool MqttConfigPublisher::publish() {
  if (!publish_) {
    return false;
  }
  PublishMessage msg;
  msg.topic = topic_;
  msg.payload = scratch_;
  msg.qos = 0;
  msg.retain = true;  // Retained so new subscribers get current config
  msg.is_status = false;
  return publish_(msg);
}

const char* MqttConfigPublisher::microstepToString(uint8_t multiplier) {
  switch (multiplier) {
  case 1:
    return "FULL";
  case 2:
    return "HALF";
  case 4:
    return "1/4";
  case 8:
    return "1/8";
  case 16:
    return "1/16";
  case 32:
    return "1/32";
  default:
    return "FULL";
  }
}

}  // namespace mqtt
