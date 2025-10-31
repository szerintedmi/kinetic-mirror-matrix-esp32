#include "transport/ResponseModel.h"

#include <algorithm>
#include <sstream>

namespace transport {
namespace response {

namespace {

EventType MapLineType(command::LineType type) {
  switch (type) {
  case command::LineType::kAck:
    return EventType::kAck;
  case command::LineType::kWarn:
    return EventType::kWarn;
  case command::LineType::kError:
    return EventType::kError;
  case command::LineType::kInfo:
    return EventType::kInfo;
  case command::LineType::kData:
    return EventType::kData;
  case command::LineType::kUnknown:
  default:
    return EventType::kInfo;
  }
}

std::map<std::string, std::string> FieldsToMap(const std::vector<command::Field> &fields) {
  std::map<std::string, std::string> out;
  for (const auto &field : fields) {
    out[field.key] = field.value;
  }
  return out;
}

bool ExtractInt(const std::map<std::string, std::string> &fields,
                const char *key,
                int32_t &out_value) {
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

command::LineType MapEventType(EventType type) {
  switch (type) {
  case EventType::kAck:
    return command::LineType::kAck;
  case EventType::kWarn:
    return command::LineType::kWarn;
  case EventType::kError:
    return command::LineType::kError;
  case EventType::kInfo:
    return command::LineType::kInfo;
  case EventType::kData:
    return command::LineType::kData;
  case EventType::kDone:
    return command::LineType::kInfo;
  }
  return command::LineType::kInfo;
}

} // namespace

CommandResponse BuildCommandResponse(const command::Response &legacy,
                                     const std::string &action) {
  CommandResponse out;
  out.action = action;

  for (const auto &line : legacy.lines) {
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
    for (const auto &evt : out.events) {
      if (!evt.cmd_id.empty()) {
        out.cmd_id = evt.cmd_id;
        break;
      }
    }
  }

  return out;
}

Event BuildEvent(const command::Line &line, const std::string &action) {
  Event evt;
  evt.type = MapLineType(line.type);
  evt.cmd_id = line.msg_id;
  evt.action = action;
  evt.code = line.code;
  evt.reason = line.reason;
  evt.attributes = FieldsToMap(line.fields);
  return evt;
}

command::Line EventToLine(const Event &event) {
  command::Line line;
  line.type = MapEventType(event.type);
  line.msg_id = event.cmd_id;
  line.code = event.code;
  line.reason = event.reason;
  for (const auto &kv : event.attributes) {
    line.fields.push_back({kv.first, kv.second});
  }

  if (event.type == EventType::kDone) {
    std::ostringstream oss;
    oss << "CTRL:DONE";
    if (!event.cmd_id.empty()) {
      oss << " cmd_id=" << event.cmd_id;
    }
    if (!event.action.empty()) {
      oss << " action=" << event.action;
    }
    for (const auto &kv : event.attributes) {
      oss << ' ' << kv.first << '=' << kv.second;
    }
    line.raw = oss.str();
  }
  return line;
}

} // namespace response
} // namespace transport
