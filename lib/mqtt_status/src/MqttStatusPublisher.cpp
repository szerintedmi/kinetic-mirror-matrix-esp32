#include "mqtt/MqttStatusPublisher.h"

#include "MotorControl/MotorControlConstants.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

namespace mqtt {

namespace {

constexpr const char* kDefaultIp = "0.0.0.0";

uint32_t clampInterval(uint32_t value, uint32_t fallback) {
  return value == 0 ? fallback : value;
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

MqttStatusPublisher::MqttStatusPublisher(PublishFn publish, net_onboarding::NetOnboarding& net)
    : MqttStatusPublisher(std::move(publish), net, Config{}) {}

MqttStatusPublisher::MqttStatusPublisher(PublishFn publish,
                                         net_onboarding::NetOnboarding& net,
                                         Config cfg)
    : publish_(std::move(publish)), net_(net), cfg_(cfg) {
  if (!publish_) {
    publish_ = [](const PublishMessage&) { return false; };
  }
  cfg_.idle_interval_ms = clampInterval(cfg_.idle_interval_ms, 1000);
  cfg_.motion_interval_ms = clampInterval(cfg_.motion_interval_ms, 200);
  scratch_.reserve(256);
  last_payload_.reserve(256);
}

void MqttStatusPublisher::setTopic(const std::string& topic) {
  if (topic != topic_) {
    topic_ = topic;
    force_immediate_ = true;
  }
}

void MqttStatusPublisher::forceImmediate() {
  force_immediate_ = true;
}

void MqttStatusPublisher::loop(const MotorController& controller, uint32_t now_ms) {
  if (topic_.empty()) {
    return;
  }
  bool motion_active = false;
  if (!buildSnapshot(controller, motion_active)) {
    return;
  }

  std::size_t hash = std::hash<std::string>{}(scratch_);
  bool changed = !has_last_hash_ || hash != last_hash_;
  uint32_t interval = motion_active ? cfg_.motion_interval_ms : cfg_.idle_interval_ms;
  bool due = false;
  if (!has_last_hash_) {
    due = true;
  } else {
    due = static_cast<uint32_t>(now_ms - last_publish_ms_) >= interval;
  }

  if (!force_immediate_ && !changed && !due) {
    return;
  }

  if (!publish()) {
    return;
  }

  last_hash_ = hash;
  has_last_hash_ = true;
  last_payload_ = scratch_;
  last_publish_ms_ = now_ms;
  force_immediate_ = false;
}

bool MqttStatusPublisher::buildSnapshot(const MotorController& controller,
                                        bool& out_motion_active) {
  const auto status = net_.status();
  const char* ip = status.ip[0] ? status.ip.data() : kDefaultIp;

  scratch_.clear();
  scratch_.reserve(128 + cfg_.max_motors * 160);
  scratch_.append("{\"node_state\":\"ready\",\"ip\":\"");
  scratch_.append(ip);
  scratch_.append("\",\"motors\":{");

  out_motion_active = false;
  bool first = true;
  size_t motor_count = controller.motorCount();
  for (size_t idx = 0; idx < motor_count; ++idx) {
    const MotorState& state = controller.state(idx);
    if (!first) {
      scratch_.push_back(',');
    }
    first = false;

    int32_t budget_t = state.budget_tenths;
    int32_t missing_t = MotorControlConstants::BUDGET_TENTHS_MAX - state.budget_tenths;
    if (missing_t < 0) {
      missing_t = 0;
    }
    int32_t ttfc_tenths =
        (missing_t <= 0) ? 0
                         : static_cast<int32_t>((static_cast<int64_t>(missing_t) * 10 +
                                                 MotorControlConstants::REFILL_TENTHS_PER_SEC - 1) /
                                                MotorControlConstants::REFILL_TENTHS_PER_SEC);
    const int32_t kTtfcMaxTenths = MotorControlConstants::MAX_COOL_DOWN_TIME_S * 10;
    if (ttfc_tenths > kTtfcMaxTenths) {
      ttfc_tenths = kTtfcMaxTenths;
    }

    scratch_.push_back('\"');
    AppendUnsignedDigits(scratch_, static_cast<unsigned long long>(state.id));
    scratch_.append("\":{");
    appendMotorJson(state, budget_t, ttfc_tenths, !state.last_op_ongoing, scratch_);
    scratch_.push_back('}');

    out_motion_active = out_motion_active || state.moving;
  }

  scratch_.append("}}");
  return true;
}

bool MqttStatusPublisher::publish() {
  if (!publish_) {
    return false;
  }
  PublishMessage msg;
  msg.topic = topic_;
  msg.payload = scratch_;
  msg.qos = 0;
  msg.retain = false;
  return publish_(msg);
}

void MqttStatusPublisher::appendMotorJson(const MotorState& state,
                                          int32_t budget_tenths,
                                          int32_t ttfc_tenths,
                                          bool include_actual_ms,
                                          std::string& out) {
  auto appendBool = [&out](const char* key, bool value) {
    out.push_back('\"');
    out.append(key);
    out.append("\":");
    out.append(value ? "true" : "false");
    out.push_back(',');
  };
  auto appendInt = [&out](const char* key, long value) {
    out.push_back('\"');
    out.append(key);
    out.append("\":");
    AppendSignedDigits(out, static_cast<long long>(value));
    out.push_back(',');
  };

  appendInt("id", state.id);
  appendInt("position", state.position);
  appendBool("moving", state.moving);
  appendBool("awake", state.awake);
  appendBool("homed", state.homed);
  appendInt("steps_since_home", state.steps_since_home);

  out.push_back('\"');
  out.append("budget_s");
  out.append("\":");
  appendFixedTenths(budget_tenths, out);
  out.push_back(',');

  out.push_back('\"');
  out.append("ttfc_s");
  out.append("\":");
  appendFixedTenths(ttfc_tenths, out);
  out.push_back(',');

  appendInt("speed", state.speed);
  appendInt("accel", state.accel);
  appendInt("est_ms", state.last_op_est_ms);
  appendInt("started_ms", state.last_op_started_ms);
  if (include_actual_ms) {
    out.push_back('\"');
    out.append("actual_ms");
    out.append("\":");
    AppendSignedDigits(out, static_cast<long long>(state.last_op_last_ms));
    out.push_back(',');
  }
  if (!out.empty() && out.back() == ',') {
    out.pop_back();
  }
}

void MqttStatusPublisher::appendFixedTenths(int32_t tenths, std::string& out) {
  bool neg = tenths < 0;
  int32_t abs_value = neg ? -tenths : tenths;
  int32_t whole = abs_value / 10;
  int32_t frac = abs_value % 10;
  if (neg) {
    out.push_back('-');
  }
  AppendUnsignedDigits(out, static_cast<unsigned long long>(whole));
  out.push_back('.');
  out.push_back(static_cast<char>('0' + frac));
}

}  // namespace mqtt
