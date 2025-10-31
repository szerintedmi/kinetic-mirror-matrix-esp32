#include "mqtt/MqttCommandServer.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <utility>

#include "MotorControl/command/CommandUtils.h"
#include "transport/MessageId.h"
#include "transport/ResponseDispatcher.h"
#include "transport/ResponseModel.h"
#include "transport/CompletionTracker.h"

namespace mqtt {

namespace {

std::string ToUpper(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::toupper(c));
  });
  return value;
}

bool IsInteger(const std::string &value) {
  if (value.empty()) {
    return false;
  }
  size_t idx = 0;
  if (value[0] == '-' || value[0] == '+') {
    if (value.size() == 1) {
      return false;
    }
    idx = 1;
  }
  for (; idx < value.size(); ++idx) {
    if (!std::isdigit(static_cast<unsigned char>(value[idx]))) {
      return false;
    }
  }
  return true;
}

long ParseLong(const std::string &value, long fallback = 0) {
  if (!IsInteger(value)) {
    return fallback;
  }
  try {
    return std::stol(value);
  } catch (...) {
    return fallback;
  }
}

std::string Trim(const std::string &value) {
  size_t start = 0;
  while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }
  size_t end = value.size();
  while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }
  return value.substr(start, end - start);
}

std::string Unquote(const std::string &value) {
  if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
    std::string out;
    out.reserve(value.size() - 2);
    bool escape = false;
    for (size_t i = 1; i + 1 < value.size(); ++i) {
      char c = value[i];
      if (escape) {
        out.push_back(c);
        escape = false;
      } else if (c == '\\') {
        escape = true;
      } else {
        out.push_back(c);
      }
    }
    return out;
  }
  return value;
}

void AppendFields(ArduinoJson::JsonObject obj,
                  const std::vector<transport::command::Field> &fields) {
  for (const auto &field : fields) {
    if (field.key == "detail") {
      std::string value = Unquote(field.value);
      if (value.empty()) {
        continue;
      }
      const char *existing = obj["message"].isNull() ? nullptr : obj["message"].as<const char *>();
      if (!existing || existing[0] == '\0') {
        obj["message"] = value;
      } else {
        std::string combined(existing);
        combined.append("; ");
        combined.append(value);
        obj["message"] = combined;
      }
      continue;
    }
    const std::string &key = field.key;
    const std::string value = Unquote(field.value);
    if (IsInteger(value)) {
      obj[key] = ParseLong(value);
    } else {
      obj[key] = value;
    }
  }
}

bool ExtractIntField(const std::vector<transport::command::Field> &fields,
                     const std::string &key,
                     int32_t &out_value) {
  for (const auto &field : fields) {
    if (field.key == key) {
      out_value = ParseLong(Unquote(field.value));
      return true;
    }
  }
  return false;
}

transport::command::Line MakeErrorLine(const std::string &code,
                                       const std::string &reason,
                                       const std::string &detail = std::string()) {
  transport::command::Line line;
  line.type = transport::command::LineType::kError;
  line.code = code;
  line.reason = reason;
  if (!detail.empty()) {
    line.fields.push_back({"detail", detail});
  }
  line.raw = "CTRL:ERR";
  return line;
}

std::vector<uint8_t> TargetsFromMask(uint32_t mask, size_t limit) {
  std::vector<uint8_t> targets;
  for (size_t idx = 0; idx < limit; ++idx) {
    if (mask & (1u << idx)) {
      targets.push_back(static_cast<uint8_t>(idx));
    }
  }
  return targets;
}

} // namespace

MqttCommandServer::MqttCommandServer(MotorCommandProcessor &processor,
                                     PublishFn publish,
                                     SubscribeFn subscribe,
                                     LogFn log,
                                     ClockFn clock,
                                     Config cfg)
    : processor_(processor),
      publish_(std::move(publish)),
      subscribe_(std::move(subscribe)),
      log_(std::move(log)),
      clock_(std::move(clock)),
      config_(cfg) {
  if (!log_) {
    log_ = [](const std::string &) {};
  }
  if (!clock_) {
    clock_ = []() -> uint32_t { return 0; };
  }
  dispatcher_token_ = transport::response::ResponseDispatcher::Instance().RegisterSink(
      [this](const transport::response::Event &evt) { this->handleDispatcherEvent(evt); });
}

MqttCommandServer::~MqttCommandServer() {
  if (dispatcher_token_) {
    transport::response::ResponseDispatcher::Instance().UnregisterSink(dispatcher_token_);
    dispatcher_token_ = 0;
  }
}

bool MqttCommandServer::begin(const std::string &device_topic) {
  constexpr const char *kStatusSuffix = "/status";
  std::string base = device_topic;
  if (base.size() >= std::strlen(kStatusSuffix)) {
    auto suffix_pos = base.rfind(kStatusSuffix);
    if (suffix_pos != std::string::npos && suffix_pos + std::strlen(kStatusSuffix) == base.size()) {
      base = base.substr(0, suffix_pos);
    }
  }
  while (!base.empty() && base.back() == '/') {
    base.pop_back();
  }
  if (base.empty()) {
    if (log_) {
      log_("CTRL:WARN MQTT_CMD_TOPIC_INVALID base=empty");
    }
    return false;
  }
  auto last_slash = base.rfind('/');
  if (last_slash == std::string::npos || last_slash + 1 >= base.size()) {
    if (log_) {
      log_("CTRL:WARN MQTT_CMD_TOPIC_INVALID base=" + base);
    }
    return false;
  }
  command_topic_ = base + "/cmd";
  response_topic_ = base + "/cmd/resp";
  if (!subscribe_) {
    return false;
  }
  auto callback = [this](const std::string &topic, const std::string &payload) {
    this->handleIncoming(topic, payload);
  };
  subscribed_ = subscribe_(command_topic_, 1, std::move(callback));
  return subscribed_;
}

void MqttCommandServer::loop(uint32_t now_ms) {
  finalizeCompleted(now_ms);
  transport::response::CompletionTracker::Instance().Tick(now_ms);
}

void MqttCommandServer::handleIncoming(const std::string &topic, const std::string &payload) {
  if (topic != command_topic_) {
    return;
  }
  uint32_t now_ms = clock_ ? clock_() : 0;

  ArduinoJson::JsonDocument doc;
  std::string parse_error;
  if (!parsePayload(payload, doc, parse_error)) {
    const std::string cmd_id = transport::message_id::Next();
    const std::string action = "UNKNOWN";
    transport::command::Line error_line = MakeErrorLine("MQTT_BAD_PAYLOAD", "INVALID", parse_error);
    std::vector<transport::command::Line> errors{error_line};
    transport::command::Response empty;
    std::string completion_payload = buildCompletionPayload(cmd_id,
                                                            action,
                                                            empty,
                                                            transport::command::CompletionStatus::kError,
                                                            {},
                                                            errors,
                                                            {},
                                                            0,
                                                            now_ms,
                                                            false);
    publishCompletion(completion_payload);
    recordCompleted(cmd_id, "", completion_payload);
    return;
  }

  const char *action_c = doc["action"].as<const char *>();
  if (!action_c) {
    const std::string cmd_id = transport::message_id::Next();
    transport::command::Line error_line = MakeErrorLine("MQTT_BAD_PAYLOAD", "MISSING_FIELDS");
    transport::command::Response empty;
    std::string completion_payload = buildCompletionPayload(cmd_id,
                                                            "UNKNOWN",
                                                            empty,
                                                            transport::command::CompletionStatus::kError,
                                                            {},
                                                            {error_line},
                                                            {},
                                                            0,
                                                            now_ms,
                                                            false);
    publishCompletion(completion_payload);
    recordCompleted(cmd_id, "", completion_payload);
    return;
  }

  const char *cmd_id_c = doc["cmd_id"].as<const char *>();
  std::string cmd_id = cmd_id_c && cmd_id_c[0] ? std::string(cmd_id_c) : transport::message_id::Next();
  std::string action = ToUpper(std::string(action_c));

  if (isDuplicate(cmd_id)) {
    logDuplicate(cmd_id, now_ms);
    auto stream_it = streams_.find(cmd_id);
    if (stream_it != streams_.end() && !stream_it->second.ack_payload.empty()) {
      publishAck(stream_it->second.ack_payload);
      return;
    }
    if (transport::response::ResponseDispatcher::Instance().Replay(
            cmd_id, [this](const transport::response::Event &evt) { this->handleDispatcherEvent(evt); })) {
      return;
    }
    for (const auto &pending : pending_) {
      if (pending.cmd_id == cmd_id) {
        if (!pending.ack_payload.empty()) {
          publishAck(pending.ack_payload);
        }
        return;
      }
    }
    for (const auto &cached : recent_) {
      if (cached.cmd_id == cmd_id) {
        if (!cached.ack_payload.empty()) {
          publishAck(cached.ack_payload);
        }
        if (!cached.completion_payload.empty()) {
          publishCompletion(cached.completion_payload);
        }
        return;
      }
    }
    return;
  }

  ArduinoJson::JsonVariantConst params = doc["params"];

  std::vector<uint8_t> targets;
  std::string command_line;
  std::string build_error;
  bool unsupported_action = false;
  if (!buildCommandLine(action, params, command_line, targets, build_error, unsupported_action)) {
    transport::command::Line error_line;
    if (unsupported_action) {
      error_line = MakeErrorLine("MQTT_UNSUPPORTED_ACTION", "UNSUPPORTED", build_error);
    } else {
      error_line = MakeErrorLine("MQTT_BAD_PAYLOAD", "INVALID", build_error);
    }
    transport::command::Response empty;
    std::string completion_payload = buildCompletionPayload(cmd_id,
                                                            action,
                                                            empty,
                                                            transport::command::CompletionStatus::kError,
                                                            {},
                                                            {error_line},
                                                            {},
                                                            0,
                                                            now_ms,
                                                            false);
    publishCompletion(completion_payload);
    recordCompleted(cmd_id, "", completion_payload);
    return;
  }

  uint32_t mask = maskForTargets(targets);
  ensureStream(cmd_id, action, mask, now_ms);

  motor::command::CommandResult result = processor_.execute(command_line, now_ms);

  DispatchStream *stream_ptr = nullptr;
  auto stream_it = streams_.find(cmd_id);
  if (stream_it != streams_.end()) {
    stream_ptr = &stream_it->second;
    if (stream_ptr->action.empty()) {
      stream_ptr->action = action;
    }
  }

  transport::command::Response parsed;
  const transport::command::Response *response_ptr = nullptr;
  if (result.hasStructuredResponse()) {
    response_ptr = &result.structuredResponse();
  } else {
    std::string normalization_error;
    transport::command::ParseCommandResponse(result.lines, parsed, normalization_error);
    response_ptr = &parsed;
  }
  const auto &response = *response_ptr;

  const auto *ack_line = transport::command::FindAckLine(response);
  if (stream_ptr && ack_line && !ack_line->msg_id.empty() && stream_ptr->msg_id.empty()) {
    bindStreamToMessageId(*stream_ptr, ack_line->msg_id);
  }

  bool bound_done = false;
  auto bind_done_msg_id = [&](DispatchStream &stream) {
    if (!stream.msg_id.empty()) {
      return;
    }
    for (const auto &line : response.lines) {
      if (!line.msg_id.empty() && line.raw.rfind("CTRL:DONE", 0) == 0) {
        bindStreamToMessageId(stream, line.msg_id);
        bound_done = true;
        break;
      }
    }
  };

  if (stream_ptr) {
    bind_done_msg_id(*stream_ptr);
    if (bound_done) {
      stream_ptr = nullptr;
    }
  } else {
    auto pending_stream = streams_.find(cmd_id);
    if (pending_stream != streams_.end()) {
      bind_done_msg_id(pending_stream->second);
      if (!bound_done) {
        stream_ptr = &pending_stream->second;
      } else {
        stream_ptr = nullptr;
      }
    }
  }

  bool handled_by_dispatcher = bound_done;
  if (!handled_by_dispatcher && stream_ptr) {
    handled_by_dispatcher = stream_ptr->saw_event;
  }
  if (!handled_by_dispatcher) {
    for (const auto &cached : recent_) {
      if (cached.cmd_id == cmd_id) {
        handled_by_dispatcher = true;
        break;
      }
    }
  }
  if (handled_by_dispatcher) {
    if (stream_ptr) {
      if (stream_ptr->response.lines.empty() && !response.lines.empty()) {
        stream_ptr->response = response;
      }
      if (!stream_ptr->ack_published) {
        publishAckFromStream(*stream_ptr);
      }
    }
    return;
  }
  streams_.erase(cmd_id);
  auto warnings = transport::command::CollectWarnings(response);
  auto errors = collectErrors(response);
  auto data_lines = collectDataLines(response);
  bool accepted = (ack_line != nullptr) && errors.empty();
  transport::command::CompletionStatus status = transport::command::DeriveCompletionStatus(response);

  std::string ack_payload;
  if (accepted) {
    ack_payload = buildAckPayload(cmd_id, action, response, warnings);
    publishAck(ack_payload);
  }

  bool awaiting = false;
  if (status == transport::command::CompletionStatus::kOk && action == "MOVE") {
    if (mask != 0 && processor_.controller().isAnyMovingForMask(mask)) {
      awaiting = true;
    }
  }

  if (awaiting) {
    PendingCompletion pending;
    pending.cmd_id = cmd_id;
    pending.action = action;
    pending.command_line = command_line;
    pending.response = response;
    pending.ack_payload = ack_payload;
    pending.mask = mask;
    pending.started_ms = now_ms;
    pending.awaiting_motor_finish = true;
    pending.targets = targets;
    pending_.push_back(std::move(pending));
    return;
  }

  int32_t completion_actual_ms = -1;
  if (status == transport::command::CompletionStatus::kOk && ack_line) {
    int32_t parsed = 0;
    if (ExtractIntField(ack_line->fields, "actual_ms", parsed)) {
      completion_actual_ms = parsed;
    }
  }
  if (status == transport::command::CompletionStatus::kOk && completion_actual_ms < 0 && !awaiting) {
    completion_actual_ms = 0;
  }

  std::string completion_payload = buildCompletionPayload(cmd_id,
                                                          action,
                                                          response,
                                                          status,
                                                          warnings,
                                                          errors,
                                                          data_lines,
                                                          mask,
                                                          now_ms,
                                                          false,
                                                          completion_actual_ms);
  publishCompletion(completion_payload);
  recordCompleted(cmd_id, ack_payload, completion_payload);
}

bool MqttCommandServer::isDuplicate(const std::string &cmd_id) const {
  if (streams_.find(cmd_id) != streams_.end()) {
    return true;
  }
  for (const auto &pending : pending_) {
    if (pending.cmd_id == cmd_id) {
      return true;
    }
  }
  for (const auto &cached : recent_) {
    if (cached.cmd_id == cmd_id) {
      return true;
    }
  }
  return false;
}

void MqttCommandServer::recordCompleted(const std::string &cmd_id,
                                        const std::string &ack_payload,
                                        const std::string &completion_payload) {
  if (config_.duplicate_cache == 0) {
    return;
  }
  recent_.push_back(CachedResponse{cmd_id, ack_payload, completion_payload});
  while (recent_.size() > config_.duplicate_cache) {
    recent_.pop_front();
  }
}

bool MqttCommandServer::publishAck(const std::string &payload) {
  if (!publish_) {
    return false;
  }
  PublishMessage msg;
  msg.topic = response_topic_;
  msg.payload = payload;
  msg.qos = 1;
  msg.retain = false;
  return publish_(msg);
}

bool MqttCommandServer::publishCompletion(const std::string &payload) {
  if (!publish_) {
    return false;
  }
  PublishMessage msg;
  msg.topic = response_topic_;
  msg.payload = payload;
  msg.qos = 1;
  msg.retain = false;
  return publish_(msg);
}

bool MqttCommandServer::parsePayload(const std::string &payload,
                                     ArduinoJson::JsonDocument &doc,
                                     std::string &error) const {
  auto err = ArduinoJson::deserializeJson(doc, payload);
  if (err) {
    error = err.c_str();
    return false;
  }
  return true;
}

bool MqttCommandServer::buildCommandLine(const std::string &action,
                                         ArduinoJson::JsonVariantConst params,
                                         std::string &out,
                                         std::vector<uint8_t> &targets,
                                         std::string &error,
                                         bool &unsupported) const {
  unsupported = false;
  if (action == "MOVE") {
    if (!params.is<ArduinoJson::JsonObjectConst>()) {
      error = "params must be object";
      return false;
    }
    auto obj = params.as<ArduinoJson::JsonObjectConst>();
    auto position_field = obj["position_steps"];
    if (position_field.isNull()) {
      error = "position_steps required";
      return false;
    }
    long position = position_field.as<long>();
    std::string target_token;
    auto target_field = obj["target_ids"];
    if (!target_field.isNull()) {
      auto target = target_field;
      if (target.is<const char *>()) {
        std::string token = Trim(ToUpper(std::string(target.as<const char *>())));
        if (token == "ALL") {
          uint32_t mask = 0;
          motor::command::ParseIdMask("ALL", mask, processor_.controller().motorCount());
          targets = TargetsFromMask(mask, processor_.controller().motorCount());
          target_token = "ALL";
        } else {
          long id = ParseLong(token, -1);
          if (id < 0 || id >= static_cast<long>(processor_.controller().motorCount())) {
            error = "target out of range";
            return false;
          }
          targets.push_back(static_cast<uint8_t>(id));
          target_token = std::to_string(id);
        }
      } else if (target.is<long>()) {
        long id = target.as<long>();
        if (id < 0 || id >= static_cast<long>(processor_.controller().motorCount())) {
          error = "target out of range";
          return false;
        }
        targets.push_back(static_cast<uint8_t>(id));
        target_token = std::to_string(id);
      } else {
        error = "target_ids must be string or int";
        return false;
      }
    } else {
      targets.push_back(0);
      target_token = "0";
    }

    out = "MOVE:" + target_token + "," + std::to_string(position);
    auto speed_field = obj["speed_sps"];
    if (!speed_field.isNull()) {
      out.append(",");
      out.append(std::to_string(speed_field.as<long>()));
      auto accel_field = obj["accel_sps2"];
      if (!accel_field.isNull()) {
        out.append(",");
        out.append(std::to_string(accel_field.as<long>()));
      }
    }
    return true;
  }

  if (action == "HOME") {
    if (!params.is<ArduinoJson::JsonObjectConst>()) {
      error = "params must be object";
      return false;
    }
    auto obj = params.as<ArduinoJson::JsonObjectConst>();
    auto target_field = obj["target_ids"];
    if (target_field.isNull()) {
      error = "target_ids required";
      return false;
    }

    std::string target_token;
    auto appendTarget = [&](uint8_t id) {
      targets.push_back(id);
      target_token = std::to_string(id);
    };

    if (target_field.is<const char *>()) {
      std::string token = Trim(ToUpper(std::string(target_field.as<const char *>())));
      if (token == "ALL") {
        uint32_t mask = 0;
        motor::command::ParseIdMask("ALL", mask, processor_.controller().motorCount());
        targets = TargetsFromMask(mask, processor_.controller().motorCount());
        target_token = "ALL";
      } else {
        long id = ParseLong(token, -1);
        if (id < 0 || id >= static_cast<long>(processor_.controller().motorCount())) {
          error = "target out of range";
          return false;
        }
        appendTarget(static_cast<uint8_t>(id));
      }
    } else if (target_field.is<int>() || target_field.is<long>()) {
      long id = target_field.as<long>();
      if (id < 0 || id >= static_cast<long>(processor_.controller().motorCount())) {
        error = "target out of range";
        return false;
      }
      appendTarget(static_cast<uint8_t>(id));
    } else {
      error = "target_ids must be string or int";
      return false;
    }

    std::array<std::string, 5> optionals;
    std::array<bool, 5> present{};
    auto storeOptional = [&](const char *key, size_t idx) -> bool {
      auto field = obj[key];
      if (field.isNull()) {
        return true;
      }
      long value = 0;
      if (field.is<long>() || field.is<int>()) {
        value = field.as<long>();
      } else if (field.is<const char *>()) {
        std::string raw = field.as<const char *>();
        if (!IsInteger(raw)) {
          error = std::string(key) + " must be integer";
          return false;
        }
        value = ParseLong(raw);
      } else {
        error = std::string(key) + " must be integer";
        return false;
      }
      optionals[idx] = std::to_string(value);
      present[idx] = true;
      return true;
    };

    if (!storeOptional("overshoot_steps", 0) ||
        !storeOptional("backoff_steps", 1) ||
        !storeOptional("speed_sps", 2) ||
        !storeOptional("accel_sps2", 3) ||
        !storeOptional("full_range_steps", 4)) {
      return false;
    }

    out = "HOME:" + target_token;
    int max_idx = -1;
    for (int i = 0; i < static_cast<int>(present.size()); ++i) {
      if (present[i]) {
        max_idx = i;
      }
    }
    for (int i = 0; i <= max_idx; ++i) {
      out.append(",");
      if (present[i]) {
        out.append(optionals[i]);
      }
    }
    return true;
  }

  if (action == "GET") {
    std::string resource;
    std::string target_token;
    if (!params.isNull()) {
      if (!params.is<ArduinoJson::JsonObjectConst>()) {
        error = "params must be object";
        return false;
      }
      auto obj = params.as<ArduinoJson::JsonObjectConst>();
      auto resource_field = obj["resource"];
      if (!resource_field.isNull()) {
        if (resource_field.is<const char *>()) {
          resource = Trim(ToUpper(std::string(resource_field.as<const char *>())));
        } else {
          error = "resource must be string";
          return false;
        }
      }
      if (!resource.empty() && resource == "LAST_OP_TIMING") {
        auto target_field = obj["target_ids"];
        if (!target_field.isNull()) {
          if (target_field.is<const char *>()) {
            std::string token = Trim(ToUpper(std::string(target_field.as<const char *>())));
            if (token == "ALL") {
              target_token = "ALL";
            } else {
              if (!IsInteger(token)) {
                error = "target_ids must be string or int";
                return false;
              }
              long id = ParseLong(token, -1);
              if (id < 0 || id >= static_cast<long>(processor_.controller().motorCount())) {
                error = "target out of range";
                return false;
              }
              target_token = std::to_string(id);
            }
          } else if (target_field.is<long>() || target_field.is<int>()) {
            long id = target_field.as<long>();
            if (id < 0 || id >= static_cast<long>(processor_.controller().motorCount())) {
              error = "target out of range";
              return false;
            }
            target_token = std::to_string(id);
          } else {
            error = "target_ids must be string or int";
            return false;
          }
        }
      } else if (!resource.empty()) {
        auto target_field = obj["target_ids"];
        if (!target_field.isNull()) {
        error = "target_ids only valid for LAST_OP_TIMING";
        return false;
        }
      }
    }

    if (resource.empty()) {
      out = "GET";
      return true;
    }

    if (resource == "ALL" || resource == "SPEED" || resource == "ACCEL" ||
        resource == "DECEL" || resource == "THERMAL_LIMITING") {
      out = "GET " + resource;
      return true;
    }
    if (resource == "LAST_OP_TIMING") {
      out = "GET LAST_OP_TIMING";
      if (!target_token.empty()) {
        out.append(":");
        out.append(target_token);
      }
      return true;
    }
    unsupported = true;
    error = "unsupported resource";
    return false;
  }

  if (action == "SET") {
    if (!params.is<ArduinoJson::JsonObjectConst>()) {
      error = "params must be object";
      return false;
    }
    auto obj = params.as<ArduinoJson::JsonObjectConst>();
    std::string key;
    std::string value;
    bool recognized = false;
    for (auto kv : obj) {
      if (recognized) {
        error = "only one field allowed";
        return false;
      }
      std::string name = Trim(ToUpper(std::string(kv.key().c_str())));
      if (name == "THERMAL_LIMITING") {
        if (!kv.value().is<const char *>()) {
          error = "THERMAL_LIMITING must be string";
          return false;
        }
        std::string val = Trim(ToUpper(std::string(kv.value().as<const char *>())));
        if (val != "ON" && val != "OFF") {
          error = "THERMAL_LIMITING must be ON or OFF";
          return false;
        }
        key = "THERMAL_LIMITING";
        value = val;
        recognized = true;
      } else if (name == "SPEED_SPS" || name == "ACCEL_SPS2" || name == "DECEL_SPS2") {
        if (!(kv.value().is<long>() || kv.value().is<int>())) {
          error = name + " must be integer";
          return false;
        }
        long val = kv.value().as<long>();
        if ((name == "DECEL_SPS2" && val < 0) || (name != "DECEL_SPS2" && val <= 0)) {
          error = name + " out of range";
          return false;
        }
        if (name == "SPEED_SPS") {
          key = "SPEED";
        } else if (name == "ACCEL_SPS2") {
          key = "ACCEL";
        } else {
          key = "DECEL";
        }
        value = std::to_string(val);
        recognized = true;
      } else {
        unsupported = true;
        error = "unsupported field";
        return false;
      }
    }
    if (!recognized) {
      error = "missing field";
      return false;
    }
    out = "SET " + key + "=" + value;
    return true;
  }

  unsupported = true;
  error = "action not supported";
  return false;
}

uint32_t MqttCommandServer::maskForTargets(const std::vector<uint8_t> &targets) const {
  if (targets.empty()) {
    return 0;
  }
  uint32_t mask = 0;
  for (uint8_t id : targets) {
    mask |= (1u << id);
  }
  return mask;
}

void MqttCommandServer::ensureStream(const std::string &cmd_id,
                                     const std::string &action,
                                     uint32_t mask,
                                     uint32_t started_ms) {
  DispatchStream stream;
  stream.cmd_id = cmd_id;
  stream.msg_id.clear();
  stream.action = action;
  stream.mask = mask;
  stream.started_ms = started_ms;
  stream.saw_event = false;
  stream.ack_published = false;
  stream.done_published = false;
  stream.response.lines.clear();
  streams_[cmd_id] = std::move(stream);
}

void MqttCommandServer::handleDispatcherEvent(const transport::response::Event &event) {
  if (event.cmd_id.empty()) {
    return;
  }
  DispatchStream *stream = nullptr;
  std::string key;
  auto direct = streams_.find(event.cmd_id);
  if (direct != streams_.end()) {
    stream = &direct->second;
    key = direct->first;
  } else {
    for (auto &entry : streams_) {
      if (!entry.second.msg_id.empty() && entry.second.msg_id == event.cmd_id) {
        stream = &entry.second;
        key = entry.first;
        break;
      }
    }
  }
  if (!stream) {
    orphan_events_[event.cmd_id].push_back(event);
    return;
  }
  processStreamEvent(*stream, event);
  if (event.type == transport::response::EventType::kDone) {
    streams_.erase(key);
  }
}

void MqttCommandServer::publishAckFromStream(DispatchStream &stream) {
  if (stream.ack_published) {
    return;
  }
  if (stream.response.lines.empty()) {
    return;
  }
  std::string action = stream.action.empty() ? "UNKNOWN" : stream.action;
  auto warnings = transport::command::CollectWarnings(stream.response);
  std::string payload = buildAckPayload(stream.cmd_id, action, stream.response, std::move(warnings));
  if (payload.empty()) {
    return;
  }
  publishAck(payload);
  stream.ack_payload = std::move(payload);
  stream.ack_published = true;
}

int32_t MqttCommandServer::extractActualMs(const transport::response::Event *done_event) const {
  if (!done_event) {
    return -1;
  }
  auto it = done_event->attributes.find("actual_ms");
  if (it == done_event->attributes.end()) {
    return -1;
  }
  return static_cast<int32_t>(ParseLong(it->second, -1));
}

void MqttCommandServer::publishCompletionFromStream(DispatchStream &stream,
                                                    const transport::response::Event *done_event) {
  if (stream.done_published) {
    return;
  }
  auto warnings = transport::command::CollectWarnings(stream.response);
  auto errors = collectErrors(stream.response);
  auto data_lines = collectDataLines(stream.response);
  transport::command::CompletionStatus status = done_event
      ? transport::command::CompletionStatus::kOk
      : transport::command::DeriveCompletionStatus(stream.response);
  if (!done_event && status == transport::command::CompletionStatus::kUnknown && errors.empty()) {
    status = transport::command::CompletionStatus::kOk;
  }
  int32_t actual_ms = extractActualMs(done_event);
  if (actual_ms < 0) {
    if (const auto *ack_line = transport::command::FindAckLine(stream.response)) {
      int32_t parsed = 0;
      if (ExtractIntField(ack_line->fields, "actual_ms", parsed)) {
        actual_ms = parsed;
      }
    }
  }
  std::string action = stream.action.empty() ? "UNKNOWN" : stream.action;
  bool include_snapshot = (stream.mask != 0 && stream.started_ms != 0);
  std::string completion_payload = buildCompletionPayload(stream.cmd_id,
                                                          action,
                                                          stream.response,
                                                          status,
                                                          warnings,
                                                          errors,
                                                          data_lines,
                                                          stream.mask,
                                                          stream.started_ms,
                                                          include_snapshot,
                                                          actual_ms);
  publishCompletion(completion_payload);
  recordCompleted(stream.cmd_id, stream.ack_payload, completion_payload);
  stream.done_published = true;
}

void MqttCommandServer::bindStreamToMessageId(DispatchStream &stream,
                                              const std::string &msg_id) {
  if (msg_id.empty()) {
    return;
  }
  stream.msg_id = msg_id;
  auto it = orphan_events_.find(msg_id);
  if (it == orphan_events_.end()) {
    return;
  }
  for (const auto &evt : it->second) {
    processStreamEvent(stream, evt);
  }
  stream.saw_event = true;
  if (stream.done_published) {
    streams_.erase(stream.cmd_id);
  }
  orphan_events_.erase(it);
}

void MqttCommandServer::processStreamEvent(DispatchStream &stream,
                                           const transport::response::Event &event) {
  stream.saw_event = true;
  if (!event.action.empty()) {
    stream.action = event.action;
  }
  transport::command::Line line = transport::response::EventToLine(event);
  stream.response.lines.push_back(line);
  switch (event.type) {
  case transport::response::EventType::kAck:
    publishAckFromStream(stream);
    break;
  case transport::response::EventType::kDone:
    publishCompletionFromStream(stream, &event);
    break;
  case transport::response::EventType::kWarn:
  case transport::response::EventType::kError:
  case transport::response::EventType::kInfo:
  case transport::response::EventType::kData:
    break;
  }
}

std::vector<transport::command::Line> MqttCommandServer::collectErrors(const transport::command::Response &response) const {
  std::vector<transport::command::Line> out;
  for (const auto &line : response.lines) {
    if (line.type == transport::command::LineType::kError) {
      out.push_back(line);
    }
  }
  return out;
}

std::vector<transport::command::Line> MqttCommandServer::collectDataLines(const transport::command::Response &response) const {
  std::vector<transport::command::Line> out;
  for (const auto &line : response.lines) {
    if (line.type == transport::command::LineType::kData) {
      out.push_back(line);
    }
  }
  return out;
}

void MqttCommandServer::finalizeCompleted(uint32_t now_ms) {
  for (auto it = pending_.begin(); it != pending_.end();) {
    if (!it->awaiting_motor_finish) {
      ++it;
      continue;
    }
    if (processor_.controller().isAnyMovingForMask(it->mask)) {
      ++it;
      continue;
    }

    auto warnings = transport::command::CollectWarnings(it->response);
    auto errors = collectErrors(it->response);
    auto data_lines = collectDataLines(it->response);
    int32_t actual_ms = -1;
    if (it->mask != 0) {
      actual_ms = 0;
      bool any_actual = false;
      size_t count = processor_.controller().motorCount();
      for (size_t idx = 0; idx < count; ++idx) {
        if (!(it->mask & (1u << idx))) {
          continue;
        }
        const MotorState &state = processor_.controller().state(idx);
        if (!state.last_op_ongoing) {
          any_actual = true;
          if (state.last_op_last_ms > actual_ms) {
            actual_ms = state.last_op_last_ms;
          }
        }
      }
      if (!any_actual) {
        actual_ms = -1;
      }
    }
    std::string completion_payload = buildCompletionPayload(it->cmd_id,
                                                            it->action,
                                                            it->response,
                                                            transport::command::CompletionStatus::kOk,
                                                            warnings,
                                                            errors,
                                                            data_lines,
                                                            it->mask,
                                                            it->started_ms,
                                                            true,
                                                            actual_ms);
    publishCompletion(completion_payload);
    recordCompleted(it->cmd_id, it->ack_payload, completion_payload);
    it = pending_.erase(it);
  }
}

std::string MqttCommandServer::buildAckPayload(const std::string &cmd_id,
                                               const std::string &action,
                                               const transport::command::Response &response,
                                               std::vector<transport::command::Line> warnings) const {
  ArduinoJson::JsonDocument doc;
  doc["cmd_id"] = cmd_id;
  doc["action"] = action;
  doc["status"] = "ack";

  if (const auto *ack = transport::command::FindAckLine(response)) {
    auto result = doc["result"].to<ArduinoJson::JsonObject>();
    AppendFields(result, ack->fields);
  }

  if (!warnings.empty()) {
    auto arr = doc["warnings"].to<ArduinoJson::JsonArray>();
    for (const auto &warn : warnings) {
      auto obj = arr.add<ArduinoJson::JsonObject>();
      obj["code"] = warn.code;
      if (!warn.reason.empty()) {
        obj["reason"] = warn.reason;
      }
      AppendFields(obj, warn.fields);
    }
  }

  std::string out;
  ArduinoJson::serializeJson(doc, out);
  return out;
}

std::string MqttCommandServer::buildCompletionPayload(const std::string &cmd_id,
                                                      const std::string &action,
                                                      const transport::command::Response &response,
                                                      transport::command::CompletionStatus status,
                                                      const std::vector<transport::command::Line> &warnings,
                                                      const std::vector<transport::command::Line> &errors,
                                                      const std::vector<transport::command::Line> &data_lines,
                                                      uint32_t mask,
                                                      uint32_t started_ms,
                                                      bool include_motor_snapshot,
                                                      int32_t actual_ms) {
  ArduinoJson::JsonDocument doc;
  doc["cmd_id"] = cmd_id;
  doc["action"] = action;
  doc["status"] = statusToString(status);

  if (!warnings.empty()) {
    auto arr = doc["warnings"].to<ArduinoJson::JsonArray>();
    for (const auto &warn : warnings) {
      auto obj = arr.add<ArduinoJson::JsonObject>();
      obj["code"] = warn.code;
      if (!warn.reason.empty()) {
        obj["reason"] = warn.reason;
      }
      AppendFields(obj, warn.fields);
    }
  }

  if (!errors.empty()) {
    auto arr = doc["errors"].to<ArduinoJson::JsonArray>();
    for (const auto &err : errors) {
      auto obj = arr.add<ArduinoJson::JsonObject>();
      obj["code"] = err.code;
      if (!err.reason.empty()) {
        obj["reason"] = err.reason;
      } else if (const auto *desc = transport::command::LookupError(err.code)) {
        if (desc->reason) {
          obj["reason"] = desc->reason;
        }
      }
      if (const auto *desc = transport::command::LookupError(err.code)) {
        if (desc->description) {
          obj["message"] = desc->description;
        }
      }
      AppendFields(obj, err.fields);
    }
  }

  if (actual_ms >= 0) {
    doc["result"]["actual_ms"] = actual_ms;
  }
  if (include_motor_snapshot && mask != 0) {
    doc["result"]["started_ms"] = started_ms;
  } else if (!data_lines.empty()) {
    auto lines = doc["result"]["lines"].to<ArduinoJson::JsonArray>();
    for (const auto &line : data_lines) {
      lines.add(line.raw);
    }
  }

  std::string out;
  ArduinoJson::serializeJson(doc, out);
  return out;
}

std::string MqttCommandServer::statusToString(transport::command::CompletionStatus status) const {
  switch (status) {
  case transport::command::CompletionStatus::kOk:
    return "done";
  case transport::command::CompletionStatus::kError:
    return "error";
  default:
    return "unknown";
  }
}

void MqttCommandServer::logDuplicate(const std::string &cmd_id, uint32_t now_ms) {
  if (!log_) {
    return;
  }
  if (now_ms - last_duplicate_log_ms_ < config_.duplicate_log_interval_ms) {
    return;
  }
  last_duplicate_log_ms_ = now_ms;
  log_("CTRL:INFO MQTT_DUPLICATE cmd_id=" + cmd_id);
}

} // namespace mqtt
