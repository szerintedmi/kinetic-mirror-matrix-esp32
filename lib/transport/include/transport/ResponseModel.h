#pragma once

#include "transport/CommandSchema.h"

#include <map>
#include <string>
#include <vector>

namespace transport {
namespace response {

// Unified event types emitted by command handlers, regardless of transport.
enum class EventType {
  kAck,
  kDone,
  kWarn,
  kError,
  kInfo,
  kData
};

struct Event {
  EventType type = EventType::kInfo;
  std::string cmd_id;
  std::string action;
  std::string code;
  std::string reason;
  std::map<std::string, std::string> attributes;
};

struct CommandResponse {
  std::string cmd_id;
  std::string action;
  bool has_est_ms = false;
  int32_t est_ms = 0;
  bool has_actual_ms = false;
  int32_t actual_ms = 0;
  std::vector<Event> events;
};

// Build a structured CommandResponse from the response line collection emitted by handlers.
CommandResponse BuildCommandResponse(const command::Response &response_lines,
                                     const std::string &action = std::string());

// Build a single Event from a control line + action.
Event BuildEvent(const command::ResponseLine &line, const std::string &action = std::string());

// Convert an Event back into a control line for transport-specific rendering.
command::ResponseLine EventToLine(const Event &event);

} // namespace response
} // namespace transport
