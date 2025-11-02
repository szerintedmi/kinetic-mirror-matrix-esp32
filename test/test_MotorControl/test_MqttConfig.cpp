#include <unity.h>

#include <string>

#include "MotorControl/MotorCommandProcessor.h"
#include "MotorControl/command/CommandUtils.h"
#include "mqtt/MqttConfigStore.h"
#include "transport/MessageId.h"

namespace {

std::string FieldValue(const transport::command::ResponseLine &line, const std::string &key) {
  for (const auto &field : line.fields) {
    if (field.key == key) {
      return field.value;
    }
  }
  return std::string();
}

} // namespace

void test_mqtt_get_config_defaults() {
  mqtt::ConfigStore::Instance().ResetForTests();
  transport::message_id::ResetGenerator();
  transport::message_id::ClearActive();
  MotorCommandProcessor proc;
  auto result = proc.execute("MQTT:GET_CONFIG", 0);
  TEST_ASSERT_FALSE(result.is_error);
  const auto &lines = result.structuredResponse().lines;
  TEST_ASSERT_EQUAL_UINT32(1, lines.size());
  auto defaults = mqtt::ConfigStore::Instance().Defaults();
  std::string expected_host = motor::command::QuoteString(defaults.host);
  std::string expected_user = motor::command::QuoteString(defaults.user);
  std::string expected_pass = motor::command::QuoteString(defaults.pass);

  TEST_ASSERT_EQUAL_STRING(expected_host.c_str(), FieldValue(lines[0], "host").c_str());
  TEST_ASSERT_EQUAL_STRING(std::to_string(defaults.port).c_str(), FieldValue(lines[0], "port").c_str());
  TEST_ASSERT_EQUAL_STRING(expected_user.c_str(), FieldValue(lines[0], "user").c_str());
  TEST_ASSERT_EQUAL_STRING(expected_pass.c_str(), FieldValue(lines[0], "pass").c_str());
}

void test_mqtt_set_config_persist() {
  mqtt::ConfigStore::Instance().ResetForTests();
  transport::message_id::ResetGenerator();
  transport::message_id::ClearActive();
  MotorCommandProcessor proc;
  auto apply = proc.execute("MQTT:SET_CONFIG host=test.local port=1885 user=tech pass=secret", 0);
  TEST_ASSERT_FALSE(apply.is_error);

  auto cfg = mqtt::ConfigStore::Instance().Current();
  TEST_ASSERT_EQUAL_STRING("test.local", cfg.host.c_str());
  TEST_ASSERT_TRUE(cfg.host_overridden);
  TEST_ASSERT_EQUAL_UINT16(1885, cfg.port);
  TEST_ASSERT_TRUE(cfg.port_overridden);
  TEST_ASSERT_EQUAL_STRING("tech", cfg.user.c_str());
  TEST_ASSERT_TRUE(cfg.user_overridden);
  TEST_ASSERT_EQUAL_STRING("secret", cfg.pass.c_str());
  TEST_ASSERT_TRUE(cfg.pass_overridden);

  // Simulate reboot by forcing reload
  mqtt::ConfigStore::Instance().Reload();

  MotorCommandProcessor proc2;
  auto verify = proc2.execute("MQTT:GET_CONFIG", 0);
  TEST_ASSERT_FALSE(verify.is_error);
  const auto &lines = verify.structuredResponse().lines;
  TEST_ASSERT_EQUAL_UINT32(1, lines.size());

  TEST_ASSERT_EQUAL_STRING(motor::command::QuoteString("test.local").c_str(),
                           FieldValue(lines[0], "host").c_str());
  TEST_ASSERT_EQUAL_STRING("1885", FieldValue(lines[0], "port").c_str());
  TEST_ASSERT_EQUAL_STRING(motor::command::QuoteString("tech").c_str(),
                           FieldValue(lines[0], "user").c_str());
  TEST_ASSERT_EQUAL_STRING(motor::command::QuoteString("secret").c_str(),
                           FieldValue(lines[0], "pass").c_str());
}

void test_mqtt_reset_to_defaults() {
  mqtt::ConfigStore::Instance().ResetForTests();
  transport::message_id::ResetGenerator();
  transport::message_id::ClearActive();
  MotorCommandProcessor proc;
  auto apply = proc.execute("MQTT:SET_CONFIG host=custom.local port=2010 user=admin pass=token", 0);
  TEST_ASSERT_FALSE(apply.is_error);

  auto reset = proc.execute("MQTT:SET_CONFIG RESET", 0);
  TEST_ASSERT_FALSE(reset.is_error);
  auto cfg = mqtt::ConfigStore::Instance().Current();
  auto defaults = mqtt::ConfigStore::Instance().Defaults();
  TEST_ASSERT_EQUAL_STRING(defaults.host.c_str(), cfg.host.c_str());
  TEST_ASSERT_EQUAL_UINT16(defaults.port, cfg.port);
  TEST_ASSERT_EQUAL_STRING(defaults.user.c_str(), cfg.user.c_str());
  TEST_ASSERT_EQUAL_STRING(defaults.pass.c_str(), cfg.pass.c_str());
}
