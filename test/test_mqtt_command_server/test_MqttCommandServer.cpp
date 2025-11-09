#include "MotorControl/MotorCommandProcessor.h"
#include "MotorControl/command/HelpText.h"
#include "mqtt/MqttCommandServer.h"
#include "mqtt/MqttConfigStore.h"
#include "mqtt/MqttStatusPublisher.h"
#include "net_onboarding/NetOnboarding.h"
#include "transport/MessageId.h"

#include <ArduinoJson.h>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <map>
#include <string>
#include <unity.h>
#include <utility>
#include <vector>

namespace {

struct PublishedMessage {
  std::string topic;
  std::string payload;
};

struct Harness {
  MotorCommandProcessor processor;
  std::vector<PublishedMessage> messages;
  std::vector<std::string> logs;
  mqtt::MqttCommandServer::SubscribeCallback callback;
  uint32_t now_ms = 0;
  mqtt::MqttCommandServer server;

  Harness()
      : server(
            processor,
            [this](const mqtt::PublishMessage& msg) {
              messages.push_back({msg.topic, msg.payload});
              return true;
            },
            [this](const std::string& /*topic*/,
                   uint8_t /*qos*/,
                   mqtt::MqttCommandServer::SubscribeCallback cb) {
              callback = std::move(cb);
              return true;
            },
            [this](const std::string& line) { logs.push_back(line); },
            [this]() -> uint32_t { return now_ms; }) {
    bool bound = server.begin("devices/test/status");
    (void)bound;
  }

  void send(const std::string& payload) {
    TEST_ASSERT_TRUE_MESSAGE(static_cast<bool>(callback), "Server not subscribed");
    callback("devices/test/cmd", payload);
  }

  void advance(uint32_t delta) {
    now_ms += delta;
    processor.tick(now_ms);
    server.loop(now_ms);
  }

  void clearMessages() {
    messages.clear();
  }

  ArduinoJson::JsonDocument parse(size_t idx) const {
    ArduinoJson::JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(idx < messages.size(), "Message index out of range");
    auto err = deserializeJson(doc, messages[idx].payload);
    TEST_ASSERT_FALSE_MESSAGE(err, err.c_str());
    return doc;
  }
};

std::string makeMovePayload(const std::string& cmd_id, int target, int position) {
  ArduinoJson::JsonDocument doc;
  doc["cmd_id"] = cmd_id;
  doc["action"] = "MOVE";
  doc["params"]["target_ids"] = target;
  doc["params"]["position_steps"] = position;
  std::string out;
  serializeJson(doc, out);
  return out;
}

std::string makeMovePayloadWithoutId(int target, int position) {
  ArduinoJson::JsonDocument doc;
  doc["action"] = "MOVE";
  doc["params"]["target_ids"] = target;
  doc["params"]["position_steps"] = position;
  std::string out;
  serializeJson(doc, out);
  return out;
}

std::string makeHomePayload(const std::string& cmd_id, const std::string& target = "ALL") {
  ArduinoJson::JsonDocument doc;
  doc["cmd_id"] = cmd_id;
  doc["action"] = "HOME";
  if (target == "ALL") {
    doc["params"]["target_ids"] = "ALL";
  } else {
    doc["params"]["target_ids"] = std::atoi(target.c_str());
  }
  std::string out;
  serializeJson(doc, out);
  return out;
}

std::string makeHelpPayload(const std::string& cmd_id) {
  ArduinoJson::JsonDocument doc;
  doc["cmd_id"] = cmd_id;
  doc["action"] = "HELP";
  std::string out;
  serializeJson(doc, out);
  return out;
}

std::string makeGetPayload(const std::string& cmd_id,
                           const std::string& resource,
                           std::function<void(ArduinoJson::JsonObject&)> builder = {}) {
  ArduinoJson::JsonDocument doc;
  if (!cmd_id.empty()) {
    doc["cmd_id"] = cmd_id;
  }
  doc["action"] = "GET";
  auto params = doc["params"].to<ArduinoJson::JsonObject>();
  if (!resource.empty()) {
    params["resource"] = resource;
  }
  if (builder) {
    builder(params);
  }
  std::string out;
  serializeJson(doc, out);
  return out;
}

std::string makeSetPayload(const std::string& cmd_id,
                           std::function<void(ArduinoJson::JsonObject&)> builder) {
  ArduinoJson::JsonDocument doc;
  if (!cmd_id.empty()) {
    doc["cmd_id"] = cmd_id;
  }
  doc["action"] = "SET";
  auto params = doc["params"].to<ArduinoJson::JsonObject>();
  builder(params);
  std::string out;
  serializeJson(doc, out);
  return out;
}

std::string makeWakePayload(const std::string& cmd_id, const std::string& target = "ALL") {
  ArduinoJson::JsonDocument doc;
  if (!cmd_id.empty()) {
    doc["cmd_id"] = cmd_id;
  }
  doc["action"] = "WAKE";
  auto params = doc["params"].to<ArduinoJson::JsonObject>();
  if (target == "ALL") {
    params["target_ids"] = "ALL";
  } else {
    params["target_ids"] = std::atoi(target.c_str());
  }
  std::string out;
  serializeJson(doc, out);
  return out;
}

std::string makeSleepPayload(const std::string& cmd_id, const std::string& target = "ALL") {
  ArduinoJson::JsonDocument doc;
  if (!cmd_id.empty()) {
    doc["cmd_id"] = cmd_id;
  }
  doc["action"] = "SLEEP";
  auto params = doc["params"].to<ArduinoJson::JsonObject>();
  if (target == "ALL") {
    params["target_ids"] = "ALL";
  } else {
    params["target_ids"] = std::atoi(target.c_str());
  }
  std::string out;
  serializeJson(doc, out);
  return out;
}

std::string makeNetPayload(const std::string& cmd_id,
                           const std::string& net_action,
                           std::function<void(ArduinoJson::JsonObject&)> builder = {}) {
  ArduinoJson::JsonDocument doc;
  if (!cmd_id.empty()) {
    doc["cmd_id"] = cmd_id;
  }
  doc["action"] = net_action;
  if (builder) {
    auto params = doc["params"].to<ArduinoJson::JsonObject>();
    builder(params);
  }
  std::string out;
  serializeJson(doc, out);
  return out;
}

std::string makeMqttSetConfigPayload(const std::string& cmd_id,
                                     const std::string& host,
                                     int port,
                                     const std::string& user,
                                     const std::string& pass) {
  ArduinoJson::JsonDocument doc;
  if (!cmd_id.empty()) {
    doc["cmd_id"] = cmd_id;
  }
  doc["action"] = "MQTT:SET_CONFIG";
  auto params = doc["params"].to<ArduinoJson::JsonObject>();
  params["host"] = host.c_str();
  params["port"] = port;
  params["user"] = user.c_str();
  params["pass"] = pass.c_str();
  std::string out;
  serializeJson(doc, out);
  return out;
}

std::string makeMqttGetConfigPayload(const std::string& cmd_id) {
  ArduinoJson::JsonDocument doc;
  if (!cmd_id.empty()) {
    doc["cmd_id"] = cmd_id;
  }
  doc["action"] = "MQTT:GET_CONFIG";
  doc["params"].to<ArduinoJson::JsonObject>();
  std::string out;
  serializeJson(doc, out);
  return out;
}

}  // namespace

void tearDown() {
  transport::message_id::ResetGenerator();
  transport::message_id::ClearActive();
}

void test_begin_requires_device_id() {
  MotorCommandProcessor processor;
  bool subscribed = false;
  mqtt::MqttCommandServer server(
      processor,
      [](const mqtt::PublishMessage&) { return true; },
      [&](const std::string&, uint8_t, mqtt::MqttCommandServer::SubscribeCallback) {
        subscribed = true;
        return true;
      },
      [](const std::string&) {},
      []() -> uint32_t { return 0; });

  TEST_ASSERT_FALSE(server.begin("devices//status"));
  TEST_ASSERT_FALSE(subscribed);
  TEST_ASSERT_FALSE(server.begin("devices/"));
  TEST_ASSERT_FALSE(server.begin("devices/status"));

  TEST_ASSERT_TRUE(server.begin("devices/valid/status"));
  TEST_ASSERT_TRUE(subscribed);
}

void test_invalid_payload_rejected() {
  Harness h;
  h.send("not-json");
  TEST_ASSERT_EQUAL_UINT(1, h.messages.size());

  auto completion = h.parse(0);
  TEST_ASSERT_EQUAL_STRING("error", completion["status"]);
  TEST_ASSERT_TRUE(completion["errors"].is<JsonArray>());
}

void test_move_missing_param_reports_bad_payload() {
  Harness h;
  ArduinoJson::JsonDocument doc;
  doc["cmd_id"] = "missing-pos";
  doc["action"] = "MOVE";
  doc["params"]["target_ids"] = "ALL";
  std::string payload;
  serializeJson(doc, payload);
  h.send(payload);

  TEST_ASSERT_TRUE(h.messages.size() >= 1);
  size_t completion_idx = h.messages.size() - 1;
  auto completion = h.parse(completion_idx);
  TEST_ASSERT_EQUAL_STRING("error", completion["status"]);
  auto errors = completion["errors"].as<ArduinoJson::JsonArray>();
  TEST_ASSERT_FALSE(errors.isNull());
  TEST_ASSERT_TRUE(errors.size() > 0);
  TEST_ASSERT_EQUAL_STRING("MQTT_BAD_PAYLOAD", errors[0]["code"]);
  TEST_ASSERT_EQUAL_STRING("INVALID", errors[0]["reason"]);
  const char* message = nullptr;
  if (errors[0]["message"].is<const char*>()) {
    message = errors[0]["message"].as<const char*>();
  }
  TEST_ASSERT_NOT_NULL(message);
  TEST_ASSERT_NOT_EQUAL(-1, std::string(message).find("position_steps"));

  TEST_ASSERT_EQUAL_UINT(1, h.messages.size());  // No ACK expected when validation fails
}

void test_move_command_success() {
  Harness h;
  h.send(makeMovePayload("cmd-1", 0, 100));

  TEST_ASSERT_EQUAL_UINT(1, h.messages.size());
  auto ack = h.parse(0);
  TEST_ASSERT_EQUAL_STRING("ack", ack["status"]);

  h.advance(2000);
  TEST_ASSERT_EQUAL_UINT(2, h.messages.size());
  auto completion = h.parse(1);
  TEST_ASSERT_EQUAL_STRING("done", completion["status"]);
  TEST_ASSERT_TRUE(completion["result"]["actual_ms"].is<long>());
}

void test_home_command_success() {
  Harness h;
  h.send(makeHomePayload("home-1"));

  TEST_ASSERT_EQUAL_UINT(1, h.messages.size());
  auto ack = h.parse(0);
  TEST_ASSERT_EQUAL_STRING("ack", ack["status"]);
  TEST_ASSERT_EQUAL_STRING("HOME", ack["action"]);

  h.advance(2500);
  TEST_ASSERT_EQUAL_UINT(2, h.messages.size());
  auto completion = h.parse(1);
  TEST_ASSERT_EQUAL_STRING("done", completion["status"]);
  TEST_ASSERT_EQUAL_STRING("HOME", completion["action"]);
  TEST_ASSERT_TRUE(completion["result"]["actual_ms"].is<long>());
}

void test_get_all_command_success() {
  Harness h;
  h.send(makeGetPayload("cmd-get-all", "all"));

  TEST_ASSERT_EQUAL_UINT(1, h.messages.size());
  auto completion = h.parse(0);
  TEST_ASSERT_EQUAL_STRING("done", completion["status"]);
  TEST_ASSERT_TRUE(completion["result"]["SPEED"].is<long>());
  TEST_ASSERT_TRUE(completion["result"]["ACCEL"].is<long>());
  TEST_ASSERT_TRUE(completion["result"]["THERMAL_LIMITING"].is<const char*>());
}

void test_get_last_op_single_command_success() {
  Harness h;
  h.send(makeGetPayload("cmd-get-last", "last_op_timing", [](ArduinoJson::JsonObject& obj) {
    obj["target_ids"] = 0;
  }));

  TEST_ASSERT_EQUAL_UINT(1, h.messages.size());
  auto completion = h.parse(0);
  TEST_ASSERT_EQUAL_STRING("done", completion["status"]);
  TEST_ASSERT_FALSE(completion["result"]["LAST_OP_TIMING"].isNull());
  TEST_ASSERT_FALSE(completion["result"]["id"].isNull());
}

void test_help_command_success() {
  Harness h;
  h.send(makeHelpPayload("cmd-help"));

  TEST_ASSERT_EQUAL_UINT(1, h.messages.size());
  auto completion = h.parse(0);
  TEST_ASSERT_EQUAL_STRING("done", completion["status"]);
  TEST_ASSERT_EQUAL_STRING("HELP", completion["action"]);
  const char* text = completion["result"]["text"].as<const char*>();
  TEST_ASSERT_NOT_NULL(text);
  TEST_ASSERT_EQUAL_STRING(motor::command::HelpText().c_str(), text);
}

void test_set_speed_command_success() {
  Harness h;
  h.send(makeSetPayload("cmd-set-speed",
                        [](ArduinoJson::JsonObject& obj) { obj["speed_sps"] = 1500; }));

  TEST_ASSERT_EQUAL_UINT(1, h.messages.size());
  auto completion = h.parse(0);
  TEST_ASSERT_EQUAL_STRING("done", completion["status"]);
  TEST_ASSERT_TRUE(completion["result"].isNull());

  h.clearMessages();
  h.send(makeGetPayload("cmd-get-speed", "speed"));
  TEST_ASSERT_EQUAL_UINT(1, h.messages.size());
  auto get_completion = h.parse(0);
  TEST_ASSERT_EQUAL_STRING("done", get_completion["status"]);
  TEST_ASSERT_EQUAL(1500, get_completion["result"]["SPEED"].as<long>());
}

void test_missing_cmd_id_generates_uuid() {
  Harness h;
  transport::message_id::SetGenerator([idx = 0]() mutable -> std::string {
    char buffer[37];
    std::snprintf(buffer, sizeof(buffer), "aaaaaaaa-aaaa-4aaa-8aaa-%012u", idx++);
    return std::string(buffer);
  });
  transport::message_id::ClearActive();
  std::string payload = makeMovePayloadWithoutId(0, 120);
  h.send(payload);

  TEST_ASSERT_EQUAL_UINT(1, h.messages.size());
  auto ack = h.parse(0);
  TEST_ASSERT_EQUAL_STRING("ack", ack["status"]);
  TEST_ASSERT_EQUAL_STRING("MOVE", ack["action"]);
  TEST_ASSERT_TRUE_MESSAGE(ack["cmd_id"].is<const char*>(), "cmd_id missing from ACK");
  std::string generated_id = ack["cmd_id"].as<const char*>();
  TEST_ASSERT_EQUAL_UINT_MESSAGE(
      36, static_cast<unsigned int>(generated_id.size()), "cmd_id must be UUID length");

  h.advance(2000);
  TEST_ASSERT_EQUAL_UINT(2, h.messages.size());
  auto completion = h.parse(1);
  TEST_ASSERT_EQUAL_STRING(generated_id.c_str(), completion["cmd_id"]);
  TEST_ASSERT_EQUAL_STRING("MOVE", completion["action"]);
  TEST_ASSERT_EQUAL_STRING("done", completion["status"]);

  h.clearMessages();
  h.send(payload);
  TEST_ASSERT_EQUAL_UINT(1, h.messages.size());
  auto ack_replay = h.parse(0);
  TEST_ASSERT_TRUE(ack_replay["cmd_id"].is<const char*>());
  std::string second_id = ack_replay["cmd_id"].as<const char*>();
  TEST_ASSERT_EQUAL_UINT_MESSAGE(
      36, static_cast<unsigned int>(second_id.size()), "cmd_id must be UUID length");
  TEST_ASSERT_TRUE_MESSAGE(generated_id != second_id,
                           "Generated cmd_id should be unique per request");

  h.advance(2000);
  TEST_ASSERT_EQUAL_UINT(2, h.messages.size());
  auto completion_replay = h.parse(1);
  TEST_ASSERT_EQUAL_STRING(second_id.c_str(), completion_replay["cmd_id"]);
}

void test_wake_command_success() {
  Harness h;
  h.send(makeWakePayload("cmd-wake"));

  TEST_ASSERT_EQUAL_UINT(1, h.messages.size());
  auto completion = h.parse(0);
  TEST_ASSERT_EQUAL_STRING("done", completion["status"]);
  TEST_ASSERT_EQUAL_STRING("WAKE", completion["action"]);
}

void test_wake_missing_target_ids() {
  Harness h;
  ArduinoJson::JsonDocument doc;
  doc["cmd_id"] = "cmd-wake-missing";
  doc["action"] = "WAKE";
  doc["params"].to<ArduinoJson::JsonObject>();
  std::string payload;
  serializeJson(doc, payload);
  h.send(payload);

  TEST_ASSERT_EQUAL_UINT(1, h.messages.size());
  auto completion = h.parse(0);
  TEST_ASSERT_EQUAL_STRING("error", completion["status"]);
  auto errors = completion["errors"].as<ArduinoJson::JsonArray>();
  TEST_ASSERT_FALSE(errors.isNull());
  TEST_ASSERT_TRUE(errors.size() > 0);
  TEST_ASSERT_EQUAL_STRING("MQTT_BAD_PAYLOAD", errors[0]["code"]);
}

void test_sleep_command_success() {
  Harness h;
  h.send(makeWakePayload("cmd-prewake"));
  h.advance(0);
  h.clearMessages();

  h.send(makeSleepPayload("cmd-sleep"));
  TEST_ASSERT_EQUAL_UINT(1, h.messages.size());
  auto completion = h.parse(0);
  TEST_ASSERT_EQUAL_STRING("done", completion["status"]);
  TEST_ASSERT_EQUAL_STRING("SLEEP", completion["action"]);
}

void test_net_status_command_success() {
  Harness h;
  h.send(makeNetPayload("cmd-net-status", "NET:STATUS"));

  TEST_ASSERT_EQUAL_UINT(1, h.messages.size());
  auto completion = h.parse(0);
  TEST_ASSERT_EQUAL_STRING("done", completion["status"]);
  TEST_ASSERT_EQUAL_STRING("NET:STATUS", completion["action"]);
  TEST_ASSERT_TRUE(completion["result"]["state"].is<const char*>());
}

void test_net_set_command_success() {
  Harness h;
  h.send(makeNetPayload("cmd-net-set", "NET:SET", [](ArduinoJson::JsonObject& obj) {
    obj["ssid"] = "TestNet";
    obj["pass"] = "password123";
  }));

  TEST_ASSERT_EQUAL_UINT(1, h.messages.size());
  auto ack = h.parse(0);
  TEST_ASSERT_EQUAL_STRING("ack", ack["status"]);
  TEST_ASSERT_EQUAL_STRING("NET:SET", ack["action"]);
}

void test_duplicate_command_logs() {
  Harness h;
  h.send(makeMovePayload("cmd-dup", 0, 100));
  h.advance(2000);
  TEST_ASSERT_EQUAL_UINT(2, h.messages.size());

  h.clearMessages();
  h.send(makeMovePayload("cmd-dup", 0, 100));
  TEST_ASSERT_FALSE(h.logs.empty());
  bool saw_duplicate = false;
  for (const auto& line : h.logs) {
    if (line.find("MQTT_DUPLICATE") != std::string::npos) {
      saw_duplicate = true;
      break;
    }
  }
  TEST_ASSERT_TRUE(saw_duplicate);
  TEST_ASSERT_EQUAL_UINT(2, h.messages.size());
}

void test_busy_rejection() {
  Harness h;
  h.send(makeMovePayload("cmd-main", 0, 200));
  TEST_ASSERT_EQUAL_UINT(1, h.messages.size());

  h.clearMessages();
  h.send(makeMovePayload("cmd-busy", 0, 150));

  TEST_ASSERT_EQUAL_UINT(1, h.messages.size());
  auto completion = h.parse(0);
  TEST_ASSERT_EQUAL_STRING("error", completion["status"]);
  auto errors = completion["errors"].as<ArduinoJson::JsonArray>();
  TEST_ASSERT_FALSE(errors.isNull());
  TEST_ASSERT_TRUE(errors.size() > 0);
  TEST_ASSERT_EQUAL_STRING("E04", errors[0]["code"]);
}

void test_status_parity_matches_status_publisher() {
  transport::message_id::ResetGenerator();
  transport::message_id::ClearActive();
  Harness h;

  h.send(makeWakePayload("wake-all"));
  h.advance(0);
  h.clearMessages();

  h.send(makeMovePayload("move-cmd", 0, 180));
  h.advance(600);
  h.clearMessages();

  motor::command::CommandResult status = h.processor.execute("STATUS", h.now_ms);
  const auto& response = status.structuredResponse();

  std::map<int, std::map<std::string, std::string>> serial_fields;
  for (const auto& line : response.lines) {
    if (line.type != transport::command::ResponseLineType::kData) {
      continue;
    }
    int id = -1;
    for (const auto& field : line.fields) {
      if (field.key == "id") {
        id = std::stoi(field.value);
        break;
      }
    }
    TEST_ASSERT_TRUE_MESSAGE(id >= 0, "STATUS data missing id field");
    auto& entry = serial_fields[id];
    for (const auto& field : line.fields) {
      entry[field.key] = field.value;
    }
  }
  TEST_ASSERT_FALSE_MESSAGE(serial_fields.empty(), "STATUS command produced no motor data");

  net_onboarding::NetOnboarding net;
#if defined(USE_STUB_BACKEND)
  net.setTestSimulation(true, 0);
#endif
  net.begin();

  std::string payload;
  mqtt::MqttStatusPublisher publisher(
      [&](const mqtt::PublishMessage& msg) {
        payload = msg.payload;
        return true;
      },
      net);
  publisher.setTopic("devices/test/status");
  publisher.forceImmediate();
  publisher.loop(h.processor.controller(), h.now_ms);
  TEST_ASSERT_FALSE_MESSAGE(payload.empty(), "status publisher did not emit payload");

  ArduinoJson::JsonDocument doc;
  auto err = ArduinoJson::deserializeJson(doc, payload);
  TEST_ASSERT_FALSE_MESSAGE(err, err.c_str());
  auto motors = doc["motors"].as<ArduinoJson::JsonObject>();
  TEST_ASSERT_FALSE_MESSAGE(motors.isNull(), "motors object missing");
  TEST_ASSERT_EQUAL_UINT(serial_fields.size(), motors.size());

  auto parse_bool_flag = [](const std::string& value) -> bool { return value == "1"; };

  auto parse_long = [](const std::string& value) -> long { return std::stol(value); };

  auto parse_double = [](const std::string& value) -> double { return std::stod(value); };

  for (auto kv : motors) {
    int id = std::stoi(kv.key().c_str());
    auto serial_it = serial_fields.find(id);
    TEST_ASSERT_TRUE_MESSAGE(serial_it != serial_fields.end(), "missing STATUS data for motor");
    auto obj = kv.value().as<ArduinoJson::JsonObject>();
    TEST_ASSERT_FALSE_MESSAGE(obj.isNull(), "motor entry missing");

    const auto& fields = serial_it->second;
    auto require_field = [&](const char* name) -> const std::string& {
      auto it = fields.find(name);
      TEST_ASSERT_TRUE_MESSAGE(it != fields.end(), (std::string("missing field ") + name).c_str());
      return it->second;
    };

    TEST_ASSERT_EQUAL_INT(parse_long(require_field("pos")), obj["position"].as<long>());
    TEST_ASSERT_EQUAL_INT(parse_bool_flag(require_field("moving")) ? 1 : 0,
                          obj["moving"].as<bool>() ? 1 : 0);
    TEST_ASSERT_EQUAL_INT(parse_bool_flag(require_field("awake")) ? 1 : 0,
                          obj["awake"].as<bool>() ? 1 : 0);
    TEST_ASSERT_EQUAL_INT(parse_bool_flag(require_field("homed")) ? 1 : 0,
                          obj["homed"].as<bool>() ? 1 : 0);
    TEST_ASSERT_EQUAL_INT(parse_long(require_field("steps_since_home")),
                          obj["steps_since_home"].as<long>());
    float budget_expected = static_cast<float>(parse_double(require_field("budget_s")));
    float budget_actual = obj["budget_s"].as<float>();
    TEST_ASSERT_FLOAT_WITHIN(0.05f, budget_expected, budget_actual);
    float ttfc_expected = static_cast<float>(parse_double(require_field("ttfc_s")));
    float ttfc_actual = obj["ttfc_s"].as<float>();
    TEST_ASSERT_FLOAT_WITHIN(0.05f, ttfc_expected, ttfc_actual);
    TEST_ASSERT_EQUAL_INT(parse_long(require_field("speed")), obj["speed"].as<long>());
    TEST_ASSERT_EQUAL_INT(parse_long(require_field("accel")), obj["accel"].as<long>());
    TEST_ASSERT_EQUAL_INT(parse_long(require_field("est_ms")), obj["est_ms"].as<long>());
    TEST_ASSERT_EQUAL_INT(parse_long(require_field("started_ms")), obj["started_ms"].as<long>());

    bool serial_has_actual = fields.find("actual_ms") != fields.end();
    bool json_has_actual = !obj["actual_ms"].isNull();
    TEST_ASSERT_EQUAL_INT(serial_has_actual ? 1 : 0, json_has_actual ? 1 : 0);
    if (serial_has_actual && json_has_actual) {
      TEST_ASSERT_EQUAL_INT(parse_long(fields.at("actual_ms")), obj["actual_ms"].as<long>());
    }
  }
}

void test_mqtt_config_json_persists() {
  mqtt::ConfigStore::Instance().ResetForTests();
  transport::message_id::ResetGenerator();
  transport::message_id::ClearActive();
  Harness h;

  h.send(makeMqttSetConfigPayload("cfg-set", "mqtt.lab", 1885, "ops", "secret"));
  TEST_ASSERT_EQUAL_UINT(1, h.messages.size());
  auto set_completion = h.parse(0);
  TEST_ASSERT_EQUAL_STRING("done", set_completion["status"]);

  auto cfg = mqtt::ConfigStore::Instance().Current();
  TEST_ASSERT_EQUAL_STRING("mqtt.lab", cfg.host.c_str());
  TEST_ASSERT_EQUAL_UINT16(1885, cfg.port);
  TEST_ASSERT_EQUAL_STRING("ops", cfg.user.c_str());
  TEST_ASSERT_EQUAL_STRING("secret", cfg.pass.c_str());
  TEST_ASSERT_TRUE(cfg.host_overridden);
  TEST_ASSERT_TRUE(cfg.port_overridden);
  TEST_ASSERT_TRUE(cfg.user_overridden);
  TEST_ASSERT_TRUE(cfg.pass_overridden);

  mqtt::ConfigStore::Instance().Reload();

  h.clearMessages();
  h.send(makeMqttGetConfigPayload("cfg-get"));
  TEST_ASSERT_EQUAL_UINT(1, h.messages.size());
  auto get_completion = h.parse(0);
  TEST_ASSERT_EQUAL_STRING("done", get_completion["status"]);

  auto result = get_completion["result"].as<ArduinoJson::JsonObject>();
  TEST_ASSERT_FALSE(result.isNull());
  TEST_ASSERT_EQUAL_STRING("mqtt.lab", result["host"].as<const char*>());
  TEST_ASSERT_EQUAL_INT(1885, result["port"].as<int>());
  TEST_ASSERT_EQUAL_STRING("ops", result["user"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("secret", result["pass"].as<const char*>());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_begin_requires_device_id);
  RUN_TEST(test_invalid_payload_rejected);
  RUN_TEST(test_move_missing_param_reports_bad_payload);
  RUN_TEST(test_move_command_success);
  RUN_TEST(test_home_command_success);
  RUN_TEST(test_get_all_command_success);
  RUN_TEST(test_get_last_op_single_command_success);
  RUN_TEST(test_help_command_success);
  RUN_TEST(test_set_speed_command_success);
  RUN_TEST(test_missing_cmd_id_generates_uuid);
  RUN_TEST(test_wake_command_success);
  RUN_TEST(test_wake_missing_target_ids);
  RUN_TEST(test_sleep_command_success);
  RUN_TEST(test_net_status_command_success);
  RUN_TEST(test_net_set_command_success);
  RUN_TEST(test_duplicate_command_logs);
  RUN_TEST(test_busy_rejection);
  RUN_TEST(test_status_parity_matches_status_publisher);
  RUN_TEST(test_mqtt_config_json_persists);
  return UNITY_END();
}
