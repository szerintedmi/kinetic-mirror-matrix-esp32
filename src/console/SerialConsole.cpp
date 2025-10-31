// Arduino serial console only
#if defined(ARDUINO)
#include <Arduino.h>
#include "MotorControl/MotorCommandProcessor.h"
#include "MotorControl/command/ResponseFormatter.h"
#include "mqtt/MqttPresenceClient.h"
#include "mqtt/MqttStatusPublisher.h"
#include "mqtt/MqttCommandServer.h"

#include <cstring>
#include <string>
#include <utility>
#include "net_onboarding/NetSingleton.h"
#include "net_onboarding/SerialImmediate.h"
#include "transport/CommandSchema.h"
#include "transport/ResponseDispatcher.h"
#include "transport/ResponseModel.h"
#include "transport/CompletionTracker.h"
#include "MotorControl/command/CommandResult.h"

// Defer heavy backend initialization until setup() runs, to avoid
// hardware init during static construction before Arduino core is ready.
static MotorCommandProcessor *commandProcessor = nullptr;
static char inputBuf[256];
static size_t inputLen = 0;
static uint32_t ignore_until_ms = 0; // grace period to ignore deploy-time noise
static mqtt::AsyncMqttPresenceClient *presenceClient = nullptr;
static mqtt::MqttStatusPublisher *statusPublisher = nullptr;
static mqtt::MqttCommandServer *commandServer = nullptr;
static bool commandServerBound = false;
static transport::response::ResponseDispatcher::SinkToken serialSinkToken = 0;

namespace {

bool StatusTopicHasDeviceId(const std::string &topic)
{
  constexpr const char *kPrefix = "devices/";
  constexpr const char *kSuffix = "/status";
  const size_t prefix_len = std::strlen(kPrefix);
  const size_t suffix_len = std::strlen(kSuffix);
  if (topic.size() <= prefix_len + suffix_len)
  {
    return false;
  }
  if (topic.rfind(kSuffix) != topic.size() - suffix_len)
  {
    return false;
  }
  if (topic.rfind(kPrefix, 0) != 0)
  {
    return false;
  }
  std::string node_segment = topic.substr(prefix_len, topic.size() - prefix_len - suffix_len);
  if (node_segment.empty() || node_segment.find('/') != std::string::npos)
  {
    return false;
  }
  return true;
}

} // namespace

void serial_console_setup()
{
  Serial.begin(115200);
  while (!Serial)
  {
    ;
  }

  if (!commandProcessor)
  {
    commandProcessor = new MotorCommandProcessor();
  }

  if (!presenceClient)
  {
    presenceClient = new mqtt::AsyncMqttPresenceClient(net_onboarding::Net());
    presenceClient->begin();
  }

  if (!statusPublisher)
  {
    auto publish_fn = [](const mqtt::PublishMessage &msg) {
      if (!presenceClient)
      {
        return false;
      }
      return presenceClient->enqueuePublish(msg);
    };
    statusPublisher = new mqtt::MqttStatusPublisher(publish_fn, net_onboarding::Net());
    statusPublisher->setTopic(presenceClient ? presenceClient->statusTopic() : std::string());
    statusPublisher->forceImmediate();
  }

  if (!serialSinkToken)
  {
    serialSinkToken = transport::response::ResponseDispatcher::Instance().RegisterSink(
        [](const transport::response::Event &evt) {
          if (evt.type == transport::response::EventType::kData)
          {
            return;
          }
          auto line = transport::response::EventToLine(evt);
          std::string text = line.raw.empty() ? transport::command::SerializeLine(line) : line.raw;
          if (!text.empty())
          {
            Serial.println(text.c_str());
          }
        });
  }

  if (!commandServer && commandProcessor && presenceClient)
  {
    auto publish_fn = [](const mqtt::PublishMessage &msg) {
      return presenceClient && presenceClient->enqueuePublish(msg);
    };
    auto subscribe_fn = [](const std::string &topic, uint8_t qos, mqtt::MqttCommandServer::SubscribeCallback cb) {
      if (!presenceClient)
      {
        return false;
      }
      return presenceClient->subscribe(topic, qos, std::move(cb));
    };
    auto log_fn = [](const std::string &line) {
      net_onboarding::PrintCtrlLineImmediate(line);
    };
    auto clock_fn = []() -> uint32_t {
      return millis();
    };
    commandServer = new mqtt::MqttCommandServer(*commandProcessor,
                                                publish_fn,
                                                subscribe_fn,
                                                log_fn,
                                                clock_fn);
    auto status_topic = presenceClient->statusTopic();
    if (StatusTopicHasDeviceId(status_topic))
    {
      commandServerBound = commandServer->begin(status_topic);
    }
  }

  // After flashing/deploy, host tools may send text into the port.
  // Ignore all serial input for a short window to avoid parsing it.
  ignore_until_ms = millis() + 500;
}

void serial_console_tick()
{
  // During initial grace window, drain and ignore any incoming bytes
  if (ignore_until_ms && millis() < ignore_until_ms)
  {
    while (Serial.available() > 0)
    {
      (void)Serial.read();
    }
    return;
  }
  if (ignore_until_ms && millis() >= ignore_until_ms)
  {
    ignore_until_ms = 0; // end grace period
    Serial.println("CTRL:READY Serial v1 — send HELP");
  }

  // Read incoming bytes until newline, then process
  while (Serial.available() > 0)
  {
    char c = (char)Serial.read();
    if (c == '\r')
    {
      // Ignore carriage return (common in CRLF); don't echo
      continue;
    }
    if (c == '\n')
    {
      // Echo newline for user feedback
      Serial.println();
      inputBuf[inputLen] = '\0';
      if (!commandProcessor)
        return; // not ready
      motor::command::CommandResult result = commandProcessor->execute(std::string(inputBuf), millis());
      std::string resp = motor::command::FormatForSerial(result);
      if (!result.hasStructuredResponse() && !resp.empty())
      {
        Serial.println(resp.c_str());
      }
      inputLen = 0;
    }
    else
    {
      // Handle backspace/delete
      if (c == 0x08 || c == 0x7F)
      { // BS or DEL
        if (inputLen > 0)
        {
          inputLen--;
          // Visual erase: move back, write space, move back
          Serial.write('\b');
          Serial.write(' ');
          Serial.write('\b');
        }
        continue;
      }
      if (inputLen + 1 < sizeof(inputBuf))
      {
        inputBuf[inputLen++] = c;
        // Echo typed character
        Serial.write(c);
      }
      else
      {
        // Overflow guard: reset buffer
        inputLen = 0;
        Serial.println("CTRL:ERR E03 BAD_PARAM buffer_overflow");
      }
    }
  }

  // Allow controller to progress time-based completions
  if (commandProcessor)
  {
    commandProcessor->tick(millis());
  }
  transport::response::CompletionTracker::Instance().Tick(millis());

  if (presenceClient && commandProcessor)
  {
    uint32_t now_ms = millis();
    if (commandServer && !commandServerBound)
    {
      auto status_topic = presenceClient->statusTopic();
      if (StatusTopicHasDeviceId(status_topic))
      {
        commandServerBound = commandServer->begin(status_topic);
      }
    }
    if (commandServer)
    {
      commandServer->loop(now_ms);
    }
    bool any_moving = false;
    bool any_awake = false;
    MotorController &mc = commandProcessor->controller();
    size_t motor_count = mc.motorCount();
    for (size_t i = 0; i < motor_count; ++i)
    {
      const MotorState &s = mc.state(i);
      if (s.moving)
        any_moving = true;
      if (s.awake)
        any_awake = true;
      if (any_moving && any_awake)
        break;
    }
    presenceClient->updateMotionState(any_moving);
    presenceClient->updatePowerState(any_awake);
    presenceClient->loop(now_ms);

    if (statusPublisher)
    {
      statusPublisher->setTopic(presenceClient->statusTopic());
      statusPublisher->loop(mc, now_ms);
    }
  }

  // Debug heartbeat removed after stabilization
}

#endif // ARDUINO
