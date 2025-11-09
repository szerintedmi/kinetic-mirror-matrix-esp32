// Arduino serial console only
#if defined(ARDUINO)
#include <Arduino.h>
#include "MotorControl/MotorCommandProcessor.h"
#include "mqtt/MqttPresenceClient.h"
#include "mqtt/MqttStatusPublisher.h"
#include "mqtt/MqttCommandServer.h"

#include <array>
#include <cstring>
#include <string>
#include <utility>
#include "net_onboarding/NetSingleton.h"
#include "net_onboarding/SerialImmediate.h"
#include "transport/CommandSchema.h"
#include "transport/ResponseDispatcher.h"
#include "transport/ResponseModel.h"
#include "transport/CompletionTracker.h"

namespace {

constexpr uint32_t kSerialGracePeriodMs = 500;
constexpr char kBackspaceChar = 0x08;
constexpr char kDeleteChar = 0x7F;

struct SerialConsoleState {
  MotorCommandProcessor *command_processor = nullptr;
  std::array<char, 256> input_buffer{};
  size_t input_length = 0;
  uint32_t ignore_until_ms = 0; // grace period to ignore deploy-time noise
  mqtt::AsyncMqttPresenceClient *presence_client = nullptr;
  mqtt::MqttStatusPublisher *status_publisher = nullptr;
  mqtt::MqttCommandServer *command_server = nullptr;
  bool command_server_bound = false;
  transport::response::ResponseDispatcher::SinkToken serial_sink_token = 0;
};

SerialConsoleState &ConsoleState()
{
  static SerialConsoleState state{};
  return state;
}

bool HandleGracePeriod(SerialConsoleState &state, uint32_t now_ms);
void ProcessSerialInput(SerialConsoleState &state);
void TickBackends(SerialConsoleState &state, uint32_t now_ms);

bool StatusTopicHasDeviceId(const std::string &topic)
{
  constexpr const char *kPrefix = "devices/";
  constexpr const char *kSuffix = "/status";
  const size_t prefix_length = std::strlen(kPrefix); // NOLINT(cppcoreguidelines-init-variables)
  const size_t suffix_length = std::strlen(kSuffix); // NOLINT(cppcoreguidelines-init-variables)
  if (topic.size() <= prefix_length + suffix_length)
  {
    return false;
  }
  if (topic.rfind(kSuffix) != topic.size() - suffix_length)
  {
    return false;
  }
  if (topic.rfind(kPrefix, 0) != 0)
  {
    return false;
  }
  std::string node_segment = topic.substr(prefix_length, topic.size() - prefix_length - suffix_length); // NOLINT(cppcoreguidelines-init-variables)
  return !node_segment.empty() && (node_segment.find('/') == std::string::npos);
}

bool HandleGracePeriod(SerialConsoleState &state, uint32_t now_ms)
{
  if ((state.ignore_until_ms != 0) && (now_ms < state.ignore_until_ms))
  {
    while (Serial.available() > 0)
    {
      (void)Serial.read();
    }
    return true;
  }
  if ((state.ignore_until_ms != 0) && (now_ms >= state.ignore_until_ms))
  {
    state.ignore_until_ms = 0;
    Serial.println("CTRL:READY Serial v1 — send HELP");
  }
  return false;
}

void ProcessSerialInput(SerialConsoleState &state)
{
  while (Serial.available() > 0)
  {
    const char input_char = static_cast<char>(Serial.read()); // NOLINT(cppcoreguidelines-init-variables)
    if (input_char == '\r')
    {
      continue;
    }
    if (input_char == '\n')
    {
      Serial.println();
      state.input_buffer[state.input_length] = '\0';
      if (state.command_processor == nullptr)
      {
        return;
      }
      (void)state.command_processor->execute(std::string(state.input_buffer.data()), millis());
      state.input_length = 0;
      continue;
    }

    if (input_char == kBackspaceChar || input_char == kDeleteChar)
    {
      if (state.input_length > 0)
      {
        state.input_length--;
        Serial.write('\b');
        Serial.write(' ');
        Serial.write('\b');
      }
      continue;
    }

    if (state.input_length + 1 < state.input_buffer.size())
    {
      state.input_buffer[state.input_length++] = input_char;
      Serial.write(input_char);
    }
    else
    {
      state.input_length = 0;
      Serial.println("CTRL:ERR E03 BAD_PARAM buffer_overflow");
    }
  }
}

void TickBackends(SerialConsoleState &state, uint32_t now_ms)
{
  if (state.command_processor != nullptr)
  {
    state.command_processor->tick(now_ms);
  }
  transport::response::CompletionTracker::Instance().Tick(now_ms);

  if (state.presence_client == nullptr || state.command_processor == nullptr)
  {
    return;
  }

  if (state.command_server != nullptr && !state.command_server_bound)
  {
    auto status_topic = state.presence_client->statusTopic();
    if (StatusTopicHasDeviceId(status_topic))
    {
      state.command_server_bound = state.command_server->begin(status_topic);
    }
  }
  if (state.command_server != nullptr)
  {
    state.command_server->loop(now_ms);
  }

  bool any_moving = false;
  bool any_awake = false;
  MotorController &controller = state.command_processor->controller();
  size_t motor_count = controller.motorCount(); // NOLINT(cppcoreguidelines-init-variables)
  for (size_t motor_index = 0; motor_index < motor_count; ++motor_index) // NOLINT(cppcoreguidelines-init-variables)
  {
    const MotorState &motor_state = controller.state(motor_index);
    if (motor_state.moving)
    {
      any_moving = true;
    }
    if (motor_state.awake)
    {
      any_awake = true;
    }
    if (any_moving && any_awake)
    {
      break;
    }
  }
  state.presence_client->updateMotionState(any_moving);
  state.presence_client->updatePowerState(any_awake);
  state.presence_client->loop(now_ms);

  if (state.status_publisher != nullptr)
  {
    state.status_publisher->setTopic(state.presence_client->statusTopic());
    state.status_publisher->loop(controller, now_ms);
  }
}

} // namespace

void serial_console_setup()
{
  auto &state = ConsoleState();
  Serial.begin(115200);
  while (!Serial)
  {
    ;
  }

  if (state.command_processor == nullptr)
  {
    state.command_processor = new MotorCommandProcessor();
  }

  if (state.presence_client == nullptr)
  {
    state.presence_client = new mqtt::AsyncMqttPresenceClient(net_onboarding::Net());
    state.presence_client->begin();
  }

  if (state.status_publisher == nullptr)
  {
    auto publish_fn = [&state](const mqtt::PublishMessage &msg) {
      if (state.presence_client == nullptr)
      {
        return false;
      }
      return state.presence_client->enqueuePublish(msg);
    };
    state.status_publisher = new mqtt::MqttStatusPublisher(publish_fn, net_onboarding::Net());
    state.status_publisher->setTopic(state.presence_client != nullptr ? state.presence_client->statusTopic() : std::string());
    state.status_publisher->forceImmediate();
  }

  if (state.serial_sink_token == 0)
  {
    state.serial_sink_token = transport::response::ResponseDispatcher::Instance().RegisterSink(
        [](const transport::response::Event &evt) {
          auto line = transport::response::EventToLine(evt);
          std::string text = line.raw.empty() ? transport::command::SerializeLine(line) : line.raw; // NOLINT(cppcoreguidelines-init-variables)
          if (!text.empty())
          {
            Serial.println(text.c_str());
          }
        });
  }

  if (state.command_server == nullptr && state.command_processor != nullptr && state.presence_client != nullptr)
  {
    auto publish_fn = [&state](const mqtt::PublishMessage &msg) {
      return state.presence_client != nullptr && state.presence_client->enqueuePublish(msg);
    };
    auto subscribe_fn = [&state](const std::string &topic, uint8_t qos, mqtt::MqttCommandServer::SubscribeCallback callback) {
      if (state.presence_client == nullptr)
      {
        return false;
      }
      return state.presence_client->subscribe(topic, qos, std::move(callback));
    };
    auto log_fn = [](const std::string &line) {
      net_onboarding::PrintCtrlLineImmediate(line);
    };
    auto clock_fn = []() -> uint32_t {
      return millis();
    };
    state.command_server = new mqtt::MqttCommandServer(*state.command_processor,
                                                      publish_fn,
                                                      subscribe_fn,
                                                      log_fn,
                                                      clock_fn);
    auto status_topic = state.presence_client->statusTopic();
    if (StatusTopicHasDeviceId(status_topic))
    {
      state.command_server_bound = state.command_server->begin(status_topic);
    }
  }

  // After flashing/deploy, host tools may send text into the port.
  // Ignore all serial input for a short window to avoid parsing it.
  state.ignore_until_ms = millis() + kSerialGracePeriodMs;
}

void serial_console_tick()
{
  auto &state = ConsoleState();
  const uint32_t now_ms = millis(); // NOLINT(cppcoreguidelines-init-variables)
  if (HandleGracePeriod(state, now_ms))
  {
    return;
  }
  ProcessSerialInput(state);
  TickBackends(state, now_ms);
}

#endif // ARDUINO
