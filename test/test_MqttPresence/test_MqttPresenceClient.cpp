#include "mqtt/MqttPresenceClient.h"
#include "net_onboarding/NetOnboarding.h"

#include <chrono>
#include <string>
#include <thread>
#include <unity.h>
#include <vector>

using mqtt::PublishMessage;

namespace {

void connectNet(net_onboarding::NetOnboarding& net) {
  net.begin(10);
  net.setTestSimulation(true, 0);
  net.setCredentials("demo-ssid", "demo-pass");
  for (int i = 0; i < 60; ++i) {
    net.loop();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
}

}  // namespace

void test_presence_payload_formatting() {
  net_onboarding::NetOnboarding net;
  connectNet(net);

  PublishMessage captured;
  bool published = false;
  auto publish_fn = [&](const PublishMessage& pub) {
    captured = pub;
    published = true;
    return true;
  };
  std::vector<std::string> logs;
  auto log_fn = [&](const std::string& line) { logs.push_back(line); };

  mqtt::MqttPresenceClient client(net, publish_fn, log_fn);
  client.handleConnected(0);
  client.loop(0);

  TEST_ASSERT_TRUE(published);
  TEST_ASSERT_EQUAL_STRING("devices/02123456789a/status", captured.topic.c_str());
  TEST_ASSERT_EQUAL_STRING("{\"node_state\":\"ready\",\"ip\":\"10.0.0.2\",\"motors\":{}}",
                           captured.payload.c_str());
}

void test_presence_logs_connect_success() {
  net_onboarding::NetOnboarding net;
  connectNet(net);

  auto publish_fn = [](const PublishMessage&) { return true; };
  std::vector<std::string> logs;
  auto log_fn = [&](const std::string& line) { logs.push_back(line); };

  mqtt::MqttPresenceClient client(net, publish_fn, log_fn);
  client.handleConnected(0, "mqtt.local:1883");

  TEST_ASSERT_FALSE(logs.empty());
  TEST_ASSERT_EQUAL_STRING("CTRL: MQTT_CONNECTED broker=mqtt.local:1883", logs.front().c_str());
}

void test_presence_republish_on_force() {
  net_onboarding::NetOnboarding net;
  connectNet(net);

  std::vector<std::string> payloads;
  auto publish_fn = [&](const PublishMessage& pub) {
    payloads.push_back(pub.payload);
    return true;
  };

  mqtt::MqttPresenceClient client(net, publish_fn);
  client.handleConnected(100);
  client.loop(100);
  client.forceImmediate();
  client.loop(150);

  TEST_ASSERT_EQUAL_INT(2, static_cast<int>(payloads.size()));
  TEST_ASSERT_NOT_EQUAL(-1, static_cast<int>(payloads[0].find("\"node_state\":\"ready\"")));
  TEST_ASSERT_NOT_EQUAL(-1, static_cast<int>(payloads[1].find("\"node_state\":\"ready\"")));
}

void test_presence_logs_failure_once() {
  net_onboarding::NetOnboarding net;
  connectNet(net);

  std::vector<std::string> logs;
  auto publish_fn = [](const PublishMessage&) { return true; };
  auto log_fn = [&](const std::string& line) { logs.push_back(line); };

  mqtt::MqttPresenceClient client(net, publish_fn, log_fn);
  client.handleConnectFailure();
  client.handleConnectFailure();

  TEST_ASSERT_EQUAL_INT(1, static_cast<int>(logs.size()));
  TEST_ASSERT_NOT_EQUAL(-1, static_cast<int>(logs[0].find("MQTT_CONNECT_FAILED")));
}

void test_presence_failure_log_resets_after_connect() {
  net_onboarding::NetOnboarding net;
  connectNet(net);

  std::vector<std::string> logs;
  auto publish_fn = [](const PublishMessage&) { return true; };
  auto log_fn = [&](const std::string& line) { logs.push_back(line); };

  mqtt::MqttPresenceClient client(net, publish_fn, log_fn);
  client.handleConnectFailure();
  TEST_ASSERT_EQUAL_INT(1, static_cast<int>(logs.size()));

  client.handleConnected(0, "mqtt.local:1883");
  TEST_ASSERT_EQUAL_INT(2, static_cast<int>(logs.size()));
  client.handleDisconnected();
  client.handleConnectFailure();

  TEST_ASSERT_EQUAL_INT(3, static_cast<int>(logs.size()));
  TEST_ASSERT_NOT_EQUAL(-1, static_cast<int>(logs.back().find("MQTT_CONNECT_FAILED")));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_presence_payload_formatting);
  RUN_TEST(test_presence_logs_connect_success);
  RUN_TEST(test_presence_republish_on_force);
  RUN_TEST(test_presence_logs_failure_once);
  RUN_TEST(test_presence_failure_log_resets_after_connect);
  return UNITY_END();
}
