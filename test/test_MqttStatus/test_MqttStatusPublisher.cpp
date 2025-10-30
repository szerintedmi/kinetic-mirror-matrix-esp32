#include <unity.h>

#include "MotorControl/MotorControlConstants.h"
#include "mqtt/MqttStatusPublisher.h"
#include "net_onboarding/NetOnboarding.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

using mqtt::PublishMessage;

namespace {

void connectNet(net_onboarding::NetOnboarding &net) {
  net.begin(10);
  net.setTestSimulation(true, 0);
  net.setCredentials("demo-ssid", "demo-pass");
  for (int i = 0; i < 60; ++i) {
    net.loop();
  }
}

class StubController : public MotorController {
public:
  explicit StubController(std::vector<MotorState> motors)
      : motors_(std::move(motors)) {}

  size_t motorCount() const override { return motors_.size(); }

  const MotorState &state(size_t idx) const override { return motors_.at(idx); }

  bool isAnyMovingForMask(uint32_t mask) const override {
    for (const auto &m : motors_) {
      if (((mask >> m.id) & 0x1u) && m.moving) {
        return true;
      }
    }
    return false;
  }

  void wakeMask(uint32_t) override {}
  bool sleepMask(uint32_t) override { return true; }
  bool moveAbsMask(uint32_t, long, int, int, uint32_t) override { return true; }
  bool homeMask(uint32_t, long, long, int, int, long, uint32_t) override {
    return true;
  }
  void tick(uint32_t) override {}
  void setThermalLimitsEnabled(bool) override {}
  void setDeceleration(int) override {}

  std::vector<MotorState> &data() { return motors_; }

private:
  std::vector<MotorState> motors_;
};

MotorState makeMotor(uint8_t id) {
  MotorState s{};
  s.id = id;
  s.position = 120 * id;
  s.speed = 4000;
  s.accel = 16000;
  s.moving = (id % 2) != 0;
  s.awake = true;
  s.homed = true;
  s.steps_since_home = 300 + (50 * id);
  s.budget_tenths = MotorControlConstants::BUDGET_TENTHS_MAX;
  s.last_update_ms = 0;
  s.last_op_started_ms = 800000 + id * 123;
  s.last_op_last_ms = 250 + id * 10;
  s.last_op_est_ms = 240 + id * 5;
  s.last_op_type = 1;
  s.last_op_ongoing = (id % 2) != 0;
  return s;
}

int countOccurrences(const std::string &text, const std::string &needle) {
  int count = 0;
  std::string::size_type pos = text.find(needle);
  while (pos != std::string::npos) {
    ++count;
    pos = text.find(needle, pos + needle.size());
  }
  return count;
}

} // namespace

void test_status_publisher_serializes_snapshot() {
  net_onboarding::NetOnboarding net;
  connectNet(net);

  StubController controller({makeMotor(0), makeMotor(1)});

  std::vector<PublishMessage> published;
  auto publish_fn = [&](const PublishMessage &msg) {
    published.push_back(msg);
    return true;
  };

  mqtt::MqttStatusPublisher publisher(publish_fn, net);
  publisher.setTopic("devices/02123456789a/status");
  publisher.forceImmediate();
  publisher.loop(controller, 0);

  TEST_ASSERT_EQUAL_INT(1, static_cast<int>(published.size()));
  TEST_ASSERT_EQUAL_STRING("devices/02123456789a/status",
                           published[0].topic.c_str());

  const std::string &payload = published[0].payload;
  TEST_ASSERT_NOT_EQUAL(-1,
                        static_cast<int>(payload.find("\"node_state\":\"ready\"")));
  TEST_ASSERT_NOT_EQUAL(-1,
                        static_cast<int>(payload.find("\"ip\":\"10.0.0.2\"")));
  TEST_ASSERT_NOT_EQUAL(-1,
                        static_cast<int>(payload.find("\"0\":{\"id\":0,\"position\":0")));
  TEST_ASSERT_NOT_EQUAL(-1,
                        static_cast<int>(payload.find("\"moving\":false")));
  TEST_ASSERT_NOT_EQUAL(-1,
                        static_cast<int>(payload.find("\"awake\":true")));
  TEST_ASSERT_NOT_EQUAL(-1,
                        static_cast<int>(payload.find("\"speed\":4000")));
  TEST_ASSERT_NOT_EQUAL(-1,
                        static_cast<int>(payload.find("\"accel\":16000")));
  TEST_ASSERT_NOT_EQUAL(-1,
                        static_cast<int>(payload.find("\"started_ms\":800000")));
  TEST_ASSERT_EQUAL_INT(1,
                        countOccurrences(payload, "\"actual_ms\":"));
  TEST_ASSERT_NOT_EQUAL(-1,
                        static_cast<int>(payload.find("\"1\":{\"id\":1,\"position\":120")));
  TEST_ASSERT_NOT_EQUAL(-1,
                        static_cast<int>(payload.find("\"moving\":true")));
  TEST_ASSERT_NOT_EQUAL(-1,
                        static_cast<int>(payload.find("\"motors\":{")));
}

void test_status_publisher_cadence_and_changes() {
  net_onboarding::NetOnboarding net;
  connectNet(net);

  StubController controller({makeMotor(0), makeMotor(1)});

  std::vector<PublishMessage> published;
  auto publish_fn = [&](const PublishMessage &msg) {
    published.push_back(msg);
    return true;
  };

  mqtt::MqttStatusPublisher publisher(publish_fn, net);
  publisher.setTopic("devices/02123456789a/status");
  publisher.forceImmediate();

  publisher.loop(controller, 0);
  TEST_ASSERT_EQUAL_INT(1, static_cast<int>(published.size()));

  publisher.loop(controller, 50);
  TEST_ASSERT_EQUAL_INT(1, static_cast<int>(published.size()));

  publisher.loop(controller, 200);
  TEST_ASSERT_EQUAL_INT(2, static_cast<int>(published.size()));

  controller.data()[0].position += 10;
  publisher.loop(controller, 250);
  TEST_ASSERT_EQUAL_INT(3, static_cast<int>(published.size()));

  controller.data()[1].moving = false;
  controller.data()[1].last_op_ongoing = false;
  publisher.loop(controller, 300);
  TEST_ASSERT_EQUAL_INT(4, static_cast<int>(published.size()));

  publisher.loop(controller, 900);
  TEST_ASSERT_EQUAL_INT(4, static_cast<int>(published.size()));

  publisher.loop(controller, 1300);
  TEST_ASSERT_EQUAL_INT(5, static_cast<int>(published.size()));

  const std::string &latest = published.back().payload;
  TEST_ASSERT_NOT_EQUAL(-1,
                        static_cast<int>(latest.find("\"moving\":false")));
  TEST_ASSERT_EQUAL_INT(2,
                        countOccurrences(latest, "\"actual_ms\":"));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_status_publisher_serializes_snapshot);
  RUN_TEST(test_status_publisher_cadence_and_changes);
  return UNITY_END();
}

