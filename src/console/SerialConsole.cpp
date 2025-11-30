// Arduino serial console only
#if defined(ARDUINO)
#include "MotorControl/MotorCommandProcessor.h"
#include "mqtt/MqttCommandServer.h"
#include "mqtt/MqttConfigPublisher.h"
#include "mqtt/MqttPresenceClient.h"
#include "mqtt/MqttStatusPublisher.h"
#include "net_onboarding/NetSingleton.h"
#include "net_onboarding/SerialImmediate.h"
#include "serial_buffer/SerialInputBuffer.h"
#include "transport/CommandSchema.h"
#include "transport/CompletionTracker.h"
#include "transport/ResponseDispatcher.h"
#include "transport/ResponseModel.h"

#include <Arduino.h>
#include <cstring>
#include <string>
#include <utility>

namespace {

constexpr uint32_t kSerialGracePeriodMs = 500;

// Tiered loop timing: motor tick runs every iteration, network work is throttled
constexpr uint32_t kSlowTickIntervalMs = 20;           // Network/MQTT work interval (50Hz)
constexpr uint32_t kMotionStatusIntervalMs = 500;      // MQTT status publish interval during motion
constexpr size_t kMaxSerialBytesPerTick = 32;  // Limit serial processing per tick

struct SerialConsoleState {
  MotorCommandProcessor* command_processor = nullptr;
  SerialInputBuffer input_buffer{};
  uint32_t ignore_until_ms = 0;  // grace period to ignore deploy-time noise
  uint32_t last_slow_tick_ms = 0;  // track last network/MQTT tick for throttling
  mqtt::AsyncMqttPresenceClient* presence_client = nullptr;
  mqtt::MqttStatusPublisher* status_publisher = nullptr;
  mqtt::MqttConfigPublisher* config_publisher = nullptr;
  mqtt::MqttCommandServer* command_server = nullptr;
  bool command_server_bound = false;
  transport::response::ResponseDispatcher::SinkToken serial_sink_token = 0;
};

SerialConsoleState& ConsoleState() {
  static SerialConsoleState state{};
  return state;
}

bool HandleGracePeriod(SerialConsoleState& state, uint32_t now_ms);
void ProcessSerialInput(SerialConsoleState& state);
void ProcessSerialInputLimited(SerialConsoleState& state, size_t max_bytes);
void TickBackends(SerialConsoleState& state, uint32_t now_ms);
void TickBackendsNetwork(SerialConsoleState& state, uint32_t now_ms);

bool StatusTopicHasDeviceId(const std::string& topic) {
  constexpr const char* kPrefix = "devices/";
  constexpr const char* kSuffix = "/status";
  const size_t prefix_length = std::strlen(kPrefix);
  const size_t suffix_length = std::strlen(kSuffix);
  if (topic.size() <= prefix_length + suffix_length) {
    return false;
  }
  if (topic.rfind(kSuffix) != topic.size() - suffix_length) {
    return false;
  }
  if (topic.rfind(kPrefix, 0) != 0) {
    return false;
  }
  std::string node_segment = topic.substr(
      prefix_length,
      topic.size() - prefix_length - suffix_length);
  return !node_segment.empty() && (node_segment.find('/') == std::string::npos);
}

bool HandleGracePeriod(SerialConsoleState& state, uint32_t now_ms) {
  if ((state.ignore_until_ms != 0) && (now_ms < state.ignore_until_ms)) {
    while (Serial.available() > 0) {
      (void)Serial.read();
    }
    return true;
  }
  if ((state.ignore_until_ms != 0) && (now_ms >= state.ignore_until_ms)) {
    state.ignore_until_ms = 0;
    Serial.println("CTRL:READY Serial v1 — send HELP");
  }
  return false;
}

void ProcessSerialInput(SerialConsoleState& state) {
  while (Serial.available() > 0) {
    const char input_char = static_cast<char>(Serial.read());

    // Track previous length for echo handling
    const size_t prev_len = state.input_buffer.length();
    const auto result = state.input_buffer.addChar(input_char);

    switch (result) {
      case SerialInputBuffer::AddResult::kContinue:
        // Echo handling for normal characters
        if (input_char == SerialInputBuffer::kBackspaceChar ||
            input_char == SerialInputBuffer::kDeleteChar) {
          if (prev_len > state.input_buffer.length()) {
            Serial.write('\b');
            Serial.write(' ');
            Serial.write('\b');
          }
        } else if (input_char != '\r' && !state.input_buffer.isOverflowPending()) {
          Serial.write(input_char);
        }
        break;

      case SerialInputBuffer::AddResult::kLineReady:
        Serial.println();
        if (state.command_processor != nullptr) {
          (void)state.command_processor->execute(
              std::string(state.input_buffer.getLine()), millis());
        }
        state.input_buffer.reset();
        break;

      case SerialInputBuffer::AddResult::kOverflowError:
        Serial.println();
        Serial.println("CTRL:ERR E03 BAD_PARAM buffer_overflow");
        break;
    }
  }
}

// Limited version of ProcessSerialInput: process at most max_bytes per call
// to prevent blocking the motor tick loop when large serial data arrives
void ProcessSerialInputLimited(SerialConsoleState& state, size_t max_bytes) {
  size_t processed = 0;
  while (Serial.available() > 0 && processed < max_bytes) {
    const char input_char = static_cast<char>(Serial.read());
    ++processed;

    // Track previous length for echo handling
    const size_t prev_len = state.input_buffer.length();
    const auto result = state.input_buffer.addChar(input_char);

    switch (result) {
      case SerialInputBuffer::AddResult::kContinue:
        // Echo handling for normal characters
        if (input_char == SerialInputBuffer::kBackspaceChar ||
            input_char == SerialInputBuffer::kDeleteChar) {
          if (prev_len > state.input_buffer.length()) {
            Serial.write('\b');
            Serial.write(' ');
            Serial.write('\b');
          }
        } else if (input_char != '\r' && !state.input_buffer.isOverflowPending()) {
          Serial.write(input_char);
        }
        break;

      case SerialInputBuffer::AddResult::kLineReady:
        Serial.println();
        if (state.command_processor != nullptr) {
          (void)state.command_processor->execute(
              std::string(state.input_buffer.getLine()), millis());
        }
        state.input_buffer.reset();
        break;

      case SerialInputBuffer::AddResult::kOverflowError:
        Serial.println();
        Serial.println("CTRL:ERR E03 BAD_PARAM buffer_overflow");
        break;
    }
  }
}

// Full tick including motor controller (used by original API)
void TickBackends(SerialConsoleState& state, uint32_t now_ms) {
  if (state.command_processor != nullptr) {
    state.command_processor->tick(now_ms);
  }
  TickBackendsNetwork(state, now_ms);
}

// Network/MQTT work only - excludes motor tick for use in tiered loop
// Motor tick (including pollLatch) should be called separately at high frequency
void TickBackendsNetwork(SerialConsoleState& state, uint32_t now_ms) {
  transport::response::CompletionTracker::Instance().Tick(now_ms);

  if (state.presence_client == nullptr || state.command_processor == nullptr) {
    return;
  }

  if (state.command_server != nullptr && !state.command_server_bound) {
    auto status_topic = state.presence_client->statusTopic();
    if (StatusTopicHasDeviceId(status_topic)) {
      state.command_server_bound = state.command_server->begin(status_topic);
    }
  }
  if (state.command_server != nullptr) {
    state.command_server->loop(now_ms);
  }

  bool any_moving = false;
  bool any_awake = false;
  MotorController& controller = state.command_processor->controller();
  const size_t motor_count = controller.motorCount();
  for (size_t motor_index = 0; motor_index < motor_count; ++motor_index) {
    const MotorState& motor_state = controller.state(motor_index);
    if (motor_state.moving) {
      any_moving = true;
    }
    if (motor_state.awake) {
      any_awake = true;
    }
    if (any_moving && any_awake) {
      break;
    }
  }
  state.presence_client->updateMotionState(any_moving);
  state.presence_client->updatePowerState(any_awake);
  state.presence_client->loop(now_ms);

  if (state.status_publisher != nullptr) {
    state.status_publisher->setTopic(state.presence_client->statusTopic());
    state.status_publisher->loop(controller, now_ms, state.command_processor->microstepMultiplier());
  }

  if (state.config_publisher != nullptr) {
    state.config_publisher->setTopic(state.presence_client->configTopic());
    state.config_publisher->loop(now_ms);
  }
}

}  // namespace

void serial_console_setup() {
  auto& state = ConsoleState();
  Serial.begin(115200);
  while (!Serial) {
    ;
  }

  if (state.command_processor == nullptr) {
    state.command_processor = new MotorCommandProcessor();
  }

  if (state.presence_client == nullptr) {
    state.presence_client = new mqtt::AsyncMqttPresenceClient(net_onboarding::Net());
    state.presence_client->begin();
  }

  if (state.status_publisher == nullptr) {
    auto publish_fn = [&state](const mqtt::PublishMessage& msg) {
      if (state.presence_client == nullptr) {
        return false;
      }
      return state.presence_client->enqueuePublish(msg);
    };
    mqtt::MqttStatusPublisher::Config status_cfg;
    status_cfg.motion_interval_ms = kMotionStatusIntervalMs;
    state.status_publisher = new mqtt::MqttStatusPublisher(publish_fn, net_onboarding::Net(), status_cfg);
    state.status_publisher->setTopic(
        state.presence_client != nullptr ? state.presence_client->statusTopic() : std::string());
    state.status_publisher->forceImmediate();
  }

  if (state.config_publisher == nullptr && state.command_processor != nullptr) {
    auto publish_fn = [&state](const mqtt::PublishMessage& msg) {
      if (state.presence_client == nullptr) {
        return false;
      }
      return state.presence_client->enqueuePublish(msg);
    };
    state.config_publisher =
        new mqtt::MqttConfigPublisher(publish_fn, *state.command_processor);
    state.config_publisher->setTopic(
        state.presence_client != nullptr ? state.presence_client->configTopic() : std::string());
    state.config_publisher->forceImmediate();
  }

  if (state.serial_sink_token == 0) {
    state.serial_sink_token = transport::response::ResponseDispatcher::Instance().RegisterSink(
        [](const transport::response::Event& evt) {
          auto line = transport::response::EventToLine(evt);
          const std::string& text = line.raw.empty()
                                        ? transport::command::SerializeLine(line)
                                        : line.raw;
          if (!text.empty()) {
            Serial.println(text.c_str());
          }
        });
  }

  if (state.command_server == nullptr && state.command_processor != nullptr &&
      state.presence_client != nullptr) {
    auto publish_fn = [&state](const mqtt::PublishMessage& msg) {
      return state.presence_client != nullptr && state.presence_client->enqueuePublish(msg);
    };
    auto subscribe_fn = [&state](const std::string& topic,
                                 uint8_t qos,
                                 mqtt::MqttCommandServer::SubscribeCallback callback) {
      if (state.presence_client == nullptr) {
        return false;
      }
      return state.presence_client->subscribe(topic, qos, std::move(callback));
    };
    auto log_fn = [](const std::string& line) { net_onboarding::PrintCtrlLineImmediate(line); };
    auto clock_fn = []() -> uint32_t { return millis(); };
    state.command_server = new mqtt::MqttCommandServer(
        *state.command_processor, publish_fn, subscribe_fn, log_fn, clock_fn);
    auto status_topic = state.presence_client->statusTopic();
    if (StatusTopicHasDeviceId(status_topic)) {
      state.command_server_bound = state.command_server->begin(status_topic);
    }
  }

  // After flashing/deploy, host tools may send text into the port.
  // Ignore all serial input for a short window to avoid parsing it.
  state.ignore_until_ms = millis() + kSerialGracePeriodMs;
}

void serial_console_tick() {
  auto& state = ConsoleState();
  const uint32_t now_ms = millis();
  if (HandleGracePeriod(state, now_ms)) {
    return;
  }

  // HIGH PRIORITY: Motor tick runs every call to ensure pollLatch() is called
  // frequently enough (~100Hz minimum) to prevent step skipping from ISR-deferred
  // SPI latch delays
  if (state.command_processor != nullptr) {
    state.command_processor->tick(now_ms);
  }

  // MEDIUM PRIORITY: Serial input processing (limited bytes per call to prevent
  // blocking the motor tick loop when large data arrives)
  ProcessSerialInputLimited(state, kMaxSerialBytesPerTick);

  // LOW PRIORITY: Network/MQTT work (throttled to prevent main loop starvation
  // that causes MQTT keepalive timeouts)
  if (now_ms - state.last_slow_tick_ms >= kSlowTickIntervalMs) {
    TickBackendsNetwork(state, now_ms);
    state.last_slow_tick_ms = now_ms;
  }
}

#endif  // ARDUINO
