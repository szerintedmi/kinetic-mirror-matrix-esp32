#pragma once

#include <cstdint>
#include <ArduinoJson.h>
#include <deque>
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>

#include "MotorControl/MotorCommandProcessor.h"
#include "mqtt/MqttPresenceClient.h"
#include "transport/CommandSchema.h"
#include "transport/ResponseDispatcher.h"

namespace mqtt {

class MqttCommandServer {
public:
  using PublishFn = std::function<bool(const PublishMessage &)>;
  using SubscribeCallback = std::function<void(const std::string &, const std::string &)>;
  using SubscribeFn = std::function<bool(const std::string &, uint8_t, SubscribeCallback)>;
  using LogFn = std::function<void(const std::string &)>;
  using ClockFn = std::function<uint32_t()>;

  struct Config {
    Config(size_t cache = 12, uint32_t interval = 1000)
        : duplicate_cache(cache), duplicate_log_interval_ms(interval) {}
    size_t duplicate_cache;
    uint32_t duplicate_log_interval_ms;
  };

  MqttCommandServer(MotorCommandProcessor &processor,
                    PublishFn publish,
                    SubscribeFn subscribe,
                    LogFn log,
                    ClockFn clock,
                    Config cfg = Config());
  ~MqttCommandServer();

  bool begin(const std::string &device_topic);
  void loop(uint32_t now_ms);

private:
  void handleIncoming(const std::string &topic, const std::string &payload);
  bool isDuplicate(const std::string &cmd_id) const;
  void recordCompleted(const std::string &cmd_id,
                       const std::string &ack_payload,
                       const std::string &completion_payload);
  bool publishAck(const std::string &payload);
  bool publishCompletion(const std::string &payload);
  bool buildCommandLine(const std::string &action,
                        ArduinoJson::JsonVariantConst params,
                        std::string &out,
                        std::vector<uint8_t> &targets,
                        std::string &error,
                        bool &unsupported) const;
  bool parsePayload(const std::string &payload,
                    ArduinoJson::JsonDocument &doc,
                    std::string &error) const;
  std::string buildAckPayload(const std::string &cmd_id,
                              const std::string &action,
                              const transport::command::Response &response,
                              std::vector<transport::command::Line> warnings) const;
  std::string buildCompletionPayload(const std::string &cmd_id,
                                     const std::string &action,
                                     const transport::command::Response &response,
                                     transport::command::CompletionStatus status,
                                     const std::vector<transport::command::Line> &warnings,
                                     const std::vector<transport::command::Line> &errors,
                                     const std::vector<transport::command::Line> &data_lines,
                                     uint32_t mask,
                                     uint32_t started_ms,
                                     bool include_motor_snapshot,
                                     int32_t actual_ms = -1);
  void finalizeCompleted(uint32_t now_ms);
  uint32_t maskForTargets(const std::vector<uint8_t> &targets) const;
  std::string statusToString(transport::command::CompletionStatus status) const;
  void logDuplicate(const std::string &cmd_id, uint32_t now_ms);
  std::vector<transport::command::Line> collectErrors(const transport::command::Response &response) const;
  std::vector<transport::command::Line> collectDataLines(const transport::command::Response &response) const;
  void handleDispatcherEvent(const transport::response::Event &event);
  void ensureStream(const std::string &cmd_id,
                    const std::string &action,
                    uint32_t mask,
                    uint32_t started_ms);
  struct DispatchStream;
  void publishAckFromStream(struct DispatchStream &stream);
  void publishCompletionFromStream(struct DispatchStream &stream,
                                   const transport::response::Event *done_event = nullptr);
  int32_t extractActualMs(const transport::response::Event *done_event) const;

  struct CachedResponse {
    std::string cmd_id;
    std::string ack_payload;
    std::string completion_payload;
  };

  struct PendingCompletion {
    std::string cmd_id;
    std::string action;
    std::string command_line;
    transport::command::Response response;
    std::string ack_payload;
    uint32_t mask = 0;
    uint32_t started_ms = 0;
    bool awaiting_motor_finish = false;
    std::vector<uint8_t> targets;
  };

  struct DispatchStream {
    std::string cmd_id;
    std::string msg_id;
    std::string action;
    transport::command::Response response;
    uint32_t mask = 0;
    uint32_t started_ms = 0;
    bool saw_event = false;
    bool ack_published = false;
    bool done_published = false;
    std::string ack_payload;
  };

  MotorCommandProcessor &processor_;
  PublishFn publish_;
  SubscribeFn subscribe_;
  LogFn log_;
  ClockFn clock_;
  Config config_;

  std::string command_topic_;
  std::string response_topic_;
  bool subscribed_ = false;
  uint32_t last_duplicate_log_ms_ = 0;

  std::deque<CachedResponse> recent_;
  std::vector<PendingCompletion> pending_;
  transport::response::ResponseDispatcher::SinkToken dispatcher_token_ = 0;
  std::unordered_map<std::string, DispatchStream> streams_;
  std::unordered_map<std::string, std::vector<transport::response::Event>> orphan_events_;

  void bindStreamToMessageId(DispatchStream &stream, const std::string &msg_id);
  void processStreamEvent(DispatchStream &stream, const transport::response::Event &event);
};

} // namespace mqtt
