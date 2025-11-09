#include "transport/ResponseModel.h"

#include <algorithm>
#include <sstream>

namespace transport {
namespace response {

namespace {

EventType MapLineType(command::ResponseLineType type) {
  switch (type) {
  case command::ResponseLineType::kAck:
    return EventType::kAck;
  case command::ResponseLineType::kWarn:
    return EventType::kWarn;
  case command::ResponseLineType::kError:
    return EventType::kError;
  case command::ResponseLineType::kInfo:
    return EventType::kInfo;
  case command::ResponseLineType::kData:
    return EventType::kData;
  case command::ResponseLineType::kUnknown:
  default:
    return EventType::kInfo;
  }
}

std::map<std::string, std::string> FieldsToMap(const std::vector<command::Field>& fields) {
  std::map<std::string, std::string> out;
  for (const auto& field : fields) {
    out[field.key] = field.value;
  }
  return out;
}

bool ExtractInt(const std::map<std::string, std::string>& fields,
                const char* key,
                int32_t& out_value) {
  auto it = fields.find(key);
  if (it == fields.end()) {
    return false;
  }
  try {
    out_value = static_cast<int32_t>(std::stol(it->second));
    return true;
  } catch (...) {
    return false;
  }
}

command::ResponseLineType MapEventType(EventType type) {
  switch (type) {
  case EventType::kAck:
    return command::ResponseLineType::kAck;
  case EventType::kWarn:
    return command::ResponseLineType::kWarn;
  case EventType::kError:
    return command::ResponseLineType::kError;
  case EventType::kInfo:
    return command::ResponseLineType::kInfo;
  case EventType::kData:
    return command::ResponseLineType::kData;
  case EventType::kDone:
    return command::ResponseLineType::kInfo;
  }
  return command::ResponseLineType::kInfo;
}

}  // namespace

CommandResponse BuildCommandResponse(const command::Response& response_lines,
                                     const std::string& action) {
  CommandResponse out;
  out.action = action;

  for (const auto& line : response_lines.lines) {
    Event evt = BuildEvent(line, action);
    if (evt.type == EventType::kAck) {
      if (!out.cmd_id.empty() && out.cmd_id != evt.cmd_id && !evt.cmd_id.empty()) {
        out.cmd_id = evt.cmd_id;
      } else if (!evt.cmd_id.empty()) {
        out.cmd_id = evt.cmd_id;
      }
      int32_t value = 0;
      if (ExtractInt(evt.attributes, "est_ms", value)) {
        out.est_ms = value;
        out.has_est_ms = true;
      }
      if (ExtractInt(evt.attributes, "actual_ms", value)) {
        out.actual_ms = value;
        out.has_actual_ms = true;
      }
    }
    if (evt.type == EventType::kError) {
      int32_t value = 0;
      if (ExtractInt(evt.attributes, "actual_ms", value)) {
        out.actual_ms = value;
        out.has_actual_ms = true;
      }
    }
    out.events.push_back(std::move(evt));
  }

  if (out.cmd_id.empty()) {
    // Best-effort: derive cmd_id from first event.
    for (const auto& evt : out.events) {
      if (!evt.cmd_id.empty()) {
        out.cmd_id = evt.cmd_id;
        break;
      }
    }
  }

  return out;
}

Event BuildEvent(const command::ResponseLine& line, const std::string& action) {
  Event evt;
  evt.type = MapLineType(line.type);
  evt.cmd_id = line.msg_id;
  evt.action = action;
  evt.code = line.code;
  evt.reason = line.reason;
  // Preserve any preformatted raw text for transports that want to print it.
  evt.raw = line.raw;
  evt.attributes = FieldsToMap(line.fields);
  auto status_it = evt.attributes.find("status");
  if (status_it != evt.attributes.end()) {
    const std::string& status = status_it->second;
    if (status == "done" || status == "DONE") {
      evt.type = EventType::kDone;
    }
  }
  if (evt.type == EventType::kInfo && !line.raw.empty() && line.raw.rfind("CTRL:DONE", 0) == 0) {
    evt.type = EventType::kDone;
  }
  return evt;
}

command::ResponseLine EventToLine(const Event& event) {
  command::ResponseLine line;
  line.type = MapEventType(event.type);
  line.msg_id = event.cmd_id;
  line.code = event.code;
  line.reason = event.reason;
  for (const auto& kv : event.attributes) {
    line.fields.push_back({kv.first, kv.second});
  }

  // If the event carried a raw representation, preserve it.
  if (!event.raw.empty()) {
    line.raw = event.raw;
  } else if (event.type == EventType::kDone) {
    std::ostringstream oss;
    oss << "CTRL:DONE";
    if (!event.cmd_id.empty()) {
      oss << " cmd_id=" << event.cmd_id;
    }
    if (!event.action.empty()) {
      oss << " action=" << event.action;
    }
    for (const auto& kv : event.attributes) {
      oss << ' ' << kv.first << '=' << kv.second;
    }
    line.raw = oss.str();
  }
  return line;
}

}  // namespace response
}  // namespace transport
