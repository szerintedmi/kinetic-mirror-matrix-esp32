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

transport::command::ResponseLine MakeErrorLine(const std::string &code,
                                               const std::string &reason,
                                               const std::string &detail = std::string()) {
  transport::command::ResponseLine line;
  line.type = transport::command::ResponseLineType::kError;
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
    respondWithError(cmd_id,
                     "UNKNOWN",
                     MakeErrorLine("MQTT_BAD_PAYLOAD", "INVALID", parse_error),
                     now_ms);
    return;
  }

  const char *action_c = doc["action"].as<const char *>();
  if (!action_c) {
    const std::string cmd_id = transport::message_id::Next();
    respondWithError(cmd_id,
                     "UNKNOWN",
                     MakeErrorLine("MQTT_BAD_PAYLOAD", "MISSING_FIELDS"),
                     now_ms);
    return;
  }

  const char *cmd_id_c = doc["cmd_id"].as<const char *>();
  std::string cmd_id = cmd_id_c && cmd_id_c[0] ? std::string(cmd_id_c) : transport::message_id::Next();
  std::string action = ToUpper(std::string(action_c));

  if (handleDuplicateCommand(cmd_id, now_ms)) {
    return;
  }

  ArduinoJson::JsonVariantConst params = doc["params"];

  std::vector<uint8_t> targets;
  std::string command_line;
  std::string build_error;
  bool unsupported_action = false;
  if (!buildCommandLine(action, params, command_line, targets, build_error, unsupported_action)) {
    const char *code = unsupported_action ? "MQTT_UNSUPPORTED_ACTION" : "MQTT_BAD_PAYLOAD";
    const char *reason = unsupported_action ? "UNSUPPORTED" : "INVALID";
    respondWithError(cmd_id,
                     action,
                     MakeErrorLine(code, reason, build_error),
                     now_ms);
    return;
  }

  uint32_t mask = maskForTargets(targets);
  CommandDispatch dispatch = makeDispatch(cmd_id, action, std::move(command_line), std::move(targets), mask);
  ensureStream(dispatch.cmd_id, dispatch.action, dispatch.mask, now_ms);
  DispatchStream *stream_ptr = findStream(dispatch.cmd_id);
  if (stream_ptr && stream_ptr->action.empty()) {
    stream_ptr->action = dispatch.action;
  }

  motor::command::CommandResult result = processor_.execute(dispatch.command_line, now_ms);
  if (!result.hasStructuredResponse()) {
    respondWithError(dispatch.cmd_id,
                     dispatch.action,
                     MakeErrorLine("MQTT_NO_STRUCTURED_RESPONSE", "NO_RESPONSE"),
                     now_ms);
    streams_.erase(dispatch.cmd_id);
    return;
  }

  const auto &response = result.structuredResponse();
  transport::response::CommandResponse contract =
      transport::response::BuildCommandResponse(response, dispatch.action);

  executeDispatch(dispatch, response, contract, stream_ptr, now_ms);
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

bool MqttCommandServer::handleDuplicateCommand(const std::string &cmd_id, uint32_t now_ms) {
  if (!isDuplicate(cmd_id)) {
    return false;
  }
  logDuplicate(cmd_id, now_ms);
  if (DispatchStream *stream = findStream(cmd_id)) {
    if (!stream->ack_payload.empty()) {
      publishAck(stream->ack_payload);
      return true;
    }
  }
  if (transport::response::ResponseDispatcher::Instance().Replay(
          cmd_id, [this](const transport::response::Event &evt) { this->handleDispatcherEvent(evt); })) {
    return true;
  }
  for (const auto &pending : pending_) {
    if (pending.cmd_id == cmd_id) {
      if (!pending.ack_payload.empty()) {
        publishAck(pending.ack_payload);
      }
      return true;
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
      return true;
    }
  }
  return true;
}

void MqttCommandServer::respondWithError(const std::string &cmd_id,
                                         const std::string &action,
                                         const transport::command::ResponseLine &error_line,
                                         uint32_t now_ms) {
  transport::command::Response empty;
  std::string completion_payload = buildCompletionPayload(cmd_id,
                                                          action.empty() ? "UNKNOWN" : action,
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

MqttCommandServer::CommandDispatch MqttCommandServer::makeDispatch(const std::string &cmd_id,
                                                                   const std::string &action,
                                                                   std::string command_line,
                                                                   std::vector<uint8_t> targets,
                                                                   uint32_t mask) const {
  CommandDispatch dispatch;
  dispatch.cmd_id = cmd_id;
  dispatch.action = action;
  dispatch.command_line = std::move(command_line);
  dispatch.targets = std::move(targets);
  dispatch.mask = mask;
  return dispatch;
}

MqttCommandServer::DispatchStream *MqttCommandServer::findStream(const std::string &cmd_id) {
  auto it = streams_.find(cmd_id);
  if (it == streams_.end()) {
    return nullptr;
  }
  return &it->second;
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

bool MqttCommandServer::streamConsumesResponse(DispatchStream *stream_ptr,
                                              const CommandDispatch &dispatch,
                                              const transport::command::Response &response,
                                              const transport::response::CommandResponse &contract,
                                              const transport::command::ResponseLine *ack_line) {
  if (!stream_ptr) {
    return false;
  }
  if (stream_ptr->action.empty()) {
    stream_ptr->action = dispatch.action;
  }
  if (stream_ptr->response.lines.empty() && !response.lines.empty()) {
    stream_ptr->response = response;
  }
  if (ack_line && !ack_line->msg_id.empty() && stream_ptr->msg_id.empty()) {
    bindStreamToMessageId(*stream_ptr, ack_line->msg_id);
  } else if (stream_ptr->msg_id.empty() && !contract.cmd_id.empty()) {
    bindStreamToMessageId(*stream_ptr, contract.cmd_id);
  }
  bool saw_done = false;
  bool saw_error = false;
  for (const auto &evt : contract.events) {
    if (evt.type == transport::response::EventType::kDone) {
      bindStreamToMessageId(*stream_ptr, evt.cmd_id);
      saw_done = true;
      break;
    }
  }
  for (const auto &evt : contract.events) {
    if (evt.type == transport::response::EventType::kError) {
      saw_error = true;
      break;
    }
  }
  if (saw_done) {
    publishAckFromStream(*stream_ptr);
    return true;
  }
  if (!saw_error && stream_ptr->saw_event) {
    publishAckFromStream(*stream_ptr);
    return true;
  }
  return false;
}

void MqttCommandServer::executeDispatch(const CommandDispatch &dispatch,
                                        const transport::command::Response &response,
                                        const transport::response::CommandResponse &contract,
                                        DispatchStream *stream_ptr,
                                        uint32_t now_ms) {
  const transport::command::ResponseLine *ack_line = transport::command::FindAckLine(response);
  bool handled_by_dispatcher = streamConsumesResponse(stream_ptr, dispatch, response, contract, ack_line);
  if (!handled_by_dispatcher) {
    for (const auto &cached : recent_) {
      if (cached.cmd_id == dispatch.cmd_id) {
        handled_by_dispatcher = true;
        break;
      }
    }
  }
  if (handled_by_dispatcher) {
    return;
  }

  streams_.erase(dispatch.cmd_id);
  auto warnings = transport::command::CollectWarnings(response);
  auto errors = collectErrors(response);
  auto data_lines = collectDataLines(response);
  bool accepted = (ack_line != nullptr) && errors.empty();
  transport::command::CompletionStatus status = transport::command::DeriveCompletionStatus(response);

  std::string ack_payload;
  if (accepted) {
    ack_payload = buildAckPayload(dispatch.cmd_id, dispatch.action, response, warnings);
    publishAck(ack_payload);
  }

  bool awaiting = false;
  if (status == transport::command::CompletionStatus::kOk && dispatch.action == "MOVE") {
    if (dispatch.mask != 0 && processor_.controller().isAnyMovingForMask(dispatch.mask)) {
      awaiting = true;
    }
  }

  if (awaiting) {
    PendingCompletion pending;
    pending.cmd_id = dispatch.cmd_id;
    pending.action = dispatch.action;
    pending.command_line = dispatch.command_line;
    pending.response = response;
    pending.ack_payload = ack_payload;
    pending.mask = dispatch.mask;
    pending.started_ms = now_ms;
    pending.awaiting_motor_finish = true;
    pending.targets = dispatch.targets;
    pending_.push_back(std::move(pending));
    return;
  }

  int32_t completion_actual_ms = -1;
  for (const auto &evt : contract.events) {
    if (evt.type == transport::response::EventType::kDone) {
      auto attr = evt.attributes.find("actual_ms");
      if (attr != evt.attributes.end()) {
        completion_actual_ms = static_cast<int32_t>(ParseLong(attr->second, -1));
      }
      break;
    }
  }
  if (completion_actual_ms < 0 && ack_line) {
    int32_t parsed = 0;
    if (ExtractIntField(ack_line->fields, "actual_ms", parsed)) {
      completion_actual_ms = parsed;
    }
  }
  if (status == transport::command::CompletionStatus::kOk && completion_actual_ms < 0) {
    completion_actual_ms = 0;
  }

std::string completion_payload = buildCompletionPayload(dispatch.cmd_id,
                                                          dispatch.action,
                                                          response,
                                                          status,
                                                          warnings,
                                                          errors,
                                                          data_lines,
                                                          dispatch.mask,
                                                          now_ms,
                                                          false,
                                                          completion_actual_ms);
  publishCompletion(completion_payload);
  recordCompleted(dispatch.cmd_id, ack_payload, completion_payload);
}

bool MqttCommandServer::parseIntegerField(ArduinoJson::JsonVariantConst field,
                                          const char *field_name,
                                          bool required,
                                          long &value,
                                          std::string &error) const {
  if (field.isNull()) {
    if (required) {
      error = std::string(field_name) + " required";
      return false;
    }
    return true;
  }
  if (field.is<long>() || field.is<int>()) {
    value = field.as<long>();
    return true;
  }
  if (field.is<const char *>()) {
    std::string raw = Trim(field.as<const char *>());
    if (!IsInteger(raw)) {
      error = std::string(field_name) + " must be integer";
      return false;
    }
    value = ParseLong(raw);
    return true;
  }
  error = std::string(field_name) + " must be integer";
  return false;
}

bool MqttCommandServer::parseMotorTargetSelector(ArduinoJson::JsonVariantConst selector,
                                                 std::vector<uint8_t> &targets,
                                                 std::string &token,
                                                 std::string &error,
                                                 bool required,
                                                 const char *default_token) const {
  targets.clear();
  auto appendAllTargets = [&](void) -> void {
    uint32_t mask = 0;
    motor::command::ParseIdMask("ALL", mask, processor_.controller().motorCount());
    targets = TargetsFromMask(mask, processor_.controller().motorCount());
    token = "ALL";
  };
  auto appendSingleTarget = [&](long id) -> bool {
    if (id < 0 || id >= static_cast<long>(processor_.controller().motorCount())) {
      error = "target out of range";
      return false;
    }
    targets.clear();
    targets.push_back(static_cast<uint8_t>(id));
    token = std::to_string(id);
    return true;
  };

  if (selector.isNull()) {
    if (required) {
      error = "target_ids required";
      return false;
    }
    if (default_token) {
      std::string def = Trim(ToUpper(std::string(default_token)));
      if (def == "ALL") {
        appendAllTargets();
        return true;
      }
      if (!IsInteger(def)) {
        error = "target_ids must be string or int";
        return false;
      }
      long id = ParseLong(def, -1);
      return appendSingleTarget(id);
    }
    token.clear();
    return true;
  }

  if (selector.is<const char *>()) {
    std::string raw = Trim(ToUpper(std::string(selector.as<const char *>())));
    if (raw == "ALL") {
      appendAllTargets();
      return true;
    }
    if (!IsInteger(raw)) {
      error = "target_ids must be string or int";
      return false;
    }
    long id = ParseLong(raw, -1);
    return appendSingleTarget(id);
  }

  if (selector.is<long>() || selector.is<int>()) {
    long id = selector.as<long>();
    return appendSingleTarget(id);
  }

  error = "target_ids must be string or int";
  return false;
}

bool MqttCommandServer::buildMoveCommand(ArduinoJson::JsonVariantConst params,
                                         std::string &out,
                                         std::vector<uint8_t> &targets,
                                         std::string &error) const {
  if (!params.is<ArduinoJson::JsonObjectConst>()) {
    error = "params must be object";
    return false;
  }
  auto obj = params.as<ArduinoJson::JsonObjectConst>();

  long position = 0;
  if (!parseIntegerField(obj["position_steps"], "position_steps", true, position, error)) {
    return false;
  }

  std::string target_token;
  if (!parseMotorTargetSelector(obj["target_ids"], targets, target_token, error, false, "0")) {
    return false;
  }

  out = "MOVE:" + target_token + "," + std::to_string(position);

  long speed = 0;
  if (!obj["speed_sps"].isNull()) {
    if (!parseIntegerField(obj["speed_sps"], "speed_sps", false, speed, error)) {
      return false;
    }
    out.append(",");
    out.append(std::to_string(speed));

    long accel = 0;
    if (!obj["accel_sps2"].isNull()) {
      if (!parseIntegerField(obj["accel_sps2"], "accel_sps2", false, accel, error)) {
        return false;
      }
      out.append(",");
      out.append(std::to_string(accel));
    }
  }

  return true;
}

bool MqttCommandServer::buildHomeCommand(ArduinoJson::JsonVariantConst params,
                                         std::string &out,
                                         std::vector<uint8_t> &targets,
                                         std::string &error) const {
  if (!params.is<ArduinoJson::JsonObjectConst>()) {
    error = "params must be object";
    return false;
  }
  auto obj = params.as<ArduinoJson::JsonObjectConst>();

  std::string target_token;
  if (!parseMotorTargetSelector(obj["target_ids"], targets, target_token, error, true)) {
    return false;
  }

  std::array<std::string, 5> optionals;
  std::array<bool, 5> present{};
  const std::array<const char *, 5> keys = {
      "overshoot_steps",
      "backoff_steps",
      "speed_sps",
      "accel_sps2",
      "full_range_steps"};

  for (size_t idx = 0; idx < keys.size(); ++idx) {
    auto field = obj[keys[idx]];
    long value = 0;
    if (!parseIntegerField(field, keys[idx], false, value, error)) {
      return false;
    }
    if (!field.isNull()) {
      optionals[idx] = std::to_string(value);
      present[idx] = true;
    }
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

bool MqttCommandServer::buildWakeSleepCommand(const std::string &action,
                                              ArduinoJson::JsonVariantConst params,
                                              std::string &out,
                                              std::vector<uint8_t> &targets,
                                              std::string &error) const {
  if (!params.is<ArduinoJson::JsonObjectConst>()) {
    error = "params must be object";
    return false;
  }
  auto obj = params.as<ArduinoJson::JsonObjectConst>();
  std::string token;
  if (!parseMotorTargetSelector(obj["target_ids"], targets, token, error, true)) {
    return false;
  }
  out = action + ":" + token;
  return true;
}

bool MqttCommandServer::buildNetCommand(const std::string &action,
                                        ArduinoJson::JsonVariantConst params,
                                        std::string &out,
                                        std::string &error,
                                        bool &unsupported) const {
  std::string sub = action.substr(4);
  if (sub == "STATUS" || sub == "RESET" || sub == "LIST") {
    if (!params.isNull() && params.size() > 0) {
      error = "params must be empty";
      return false;
    }
    out = action;
    return true;
  }
  if (sub == "SET") {
    if (!params.is<ArduinoJson::JsonObjectConst>()) {
      error = "params must be object";
      return false;
    }
    auto obj = params.as<ArduinoJson::JsonObjectConst>();
    auto ssid_field = obj["ssid"];
    if (ssid_field.isNull() || !ssid_field.is<const char *>()) {
      error = "ssid required";
      return false;
    }
    std::string ssid = ssid_field.as<const char *>();
    auto pass_field = obj["pass"];
    std::string pass;
    if (pass_field.isNull()) {
      pass.clear();
    } else if (pass_field.is<const char *>()) {
      pass = pass_field.as<const char *>();
    } else {
      error = "pass must be string";
      return false;
    }
    out = action + "," + motor::command::QuoteString(ssid) + "," + motor::command::QuoteString(pass);
    return true;
  }
  unsupported = true;
  error = "action not supported";
  return false;
}

bool MqttCommandServer::buildGetCommand(ArduinoJson::JsonVariantConst params,
                                        std::string &out,
                                        std::string &error,
                                        bool &unsupported) const {
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
      if (!resource_field.is<const char *>()) {
        error = "resource must be string";
        return false;
      }
      resource = Trim(ToUpper(std::string(resource_field.as<const char *>())));
    }
    if (!resource.empty() && resource == "LAST_OP_TIMING") {
      auto selector = obj["target_ids"];
      std::vector<uint8_t> tmp_targets;
      if (!selector.isNull()) {
        std::string token;
        if (!parseMotorTargetSelector(selector, tmp_targets, token, error, false)) {
          return false;
        }
        target_token = token;
      }
    } else if (!resource.empty()) {
      if (!obj["target_ids"].isNull()) {
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

bool MqttCommandServer::buildSetCommand(ArduinoJson::JsonVariantConst params,
                                        std::string &out,
                                        std::string &error,
                                        bool &unsupported) const {
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

bool MqttCommandServer::buildCommandLine(const std::string &action,
                                         ArduinoJson::JsonVariantConst params,
                                         std::string &out,
                                         std::vector<uint8_t> &targets,
                                         std::string &error,
                                         bool &unsupported) const {
  targets.clear();
  unsupported = false;
  if (action == "MOVE") {
    return buildMoveCommand(params, out, targets, error);
  }
  if (action == "HOME") {
    return buildHomeCommand(params, out, targets, error);
  }
  if (action == "WAKE" || action == "SLEEP") {
    return buildWakeSleepCommand(action, params, out, targets, error);
  }
  if (action.rfind("NET:", 0) == 0) {
    return buildNetCommand(action, params, out, error, unsupported);
  }
  if (action == "GET") {
    return buildGetCommand(params, out, error, unsupported);
  }
  if (action == "SET") {
    return buildSetCommand(params, out, error, unsupported);
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
  if (!event.action.empty() && stream.action.empty()) {
    stream.action = event.action;
  }
  transport::command::ResponseLine line = transport::response::EventToLine(event);
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

std::vector<transport::command::ResponseLine> MqttCommandServer::collectErrors(const transport::command::Response &response) const {
  std::vector<transport::command::ResponseLine> out;
  for (const auto &line : response.lines) {
    if (line.type == transport::command::ResponseLineType::kError) {
      out.push_back(line);
    }
  }
  return out;
}

std::vector<transport::command::ResponseLine> MqttCommandServer::collectDataLines(const transport::command::Response &response) const {
  std::vector<transport::command::ResponseLine> out;
  for (const auto &line : response.lines) {
    if (line.type == transport::command::ResponseLineType::kData) {
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
                                               std::vector<transport::command::ResponseLine> warnings) const {
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
                                                      const std::vector<transport::command::ResponseLine> &warnings,
                                                      const std::vector<transport::command::ResponseLine> &errors,
                                                      const std::vector<transport::command::ResponseLine> &data_lines,
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

  transport::response::CommandResponse contract =
      transport::response::BuildCommandResponse(response, action);
  const transport::response::Event *done_event = nullptr;
  for (const auto &evt : contract.events) {
    if (evt.type == transport::response::EventType::kDone) {
      done_event = &evt;
      break;
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
  } else if (done_event) {
    auto result_obj = doc["result"].to<ArduinoJson::JsonObject>();
    for (const auto &kv : done_event->attributes) {
      if (kv.first == "status") {
        continue;
      }
      std::string value = Unquote(kv.second);
      if (IsInteger(value)) {
        result_obj[kv.first] = ParseLong(value);
      } else {
        result_obj[kv.first] = value;
      }
    }
  }

  if (doc["result"].is<ArduinoJson::JsonObject>()) {
    auto result_obj = doc["result"].as<ArduinoJson::JsonObject>();
    if (!result_obj.isNull() && result_obj.size() == 0) {
      doc.remove("result");
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
