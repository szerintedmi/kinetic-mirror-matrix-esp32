#include "transport/CommandSchema.h"

#include <algorithm>
#include <initializer_list>
#include <sstream>

namespace transport {
namespace command {

namespace {

const std::vector<ErrorDescriptor> &BuildCatalog() {
  static const std::vector<ErrorDescriptor> kCatalog = {
      {"E01", "BAD_CMD", CompletionStatus::kError,
       "Unknown or unsupported command action."},
      {"E02", "BAD_ID", CompletionStatus::kError,
       "Motor identifier or target mask is invalid."},
      {"E03", "BAD_PARAM", CompletionStatus::kError,
       "Command parameter failed validation."},
      {"E04", "BUSY", CompletionStatus::kError,
       "Controller is busy executing another command."},
      {"E07", "POS_OUT_OF_RANGE", CompletionStatus::kError,
       "Requested position is outside the allowed travel range."},
      {"E10", "THERMAL_REQ_GT_MAX", CompletionStatus::kError,
       "Requested move exceeds the maximum thermal budget."},
      {"E11", "THERMAL_NO_BUDGET", CompletionStatus::kError,
       "Insufficient thermal budget to run the command."},
      {"E12", "THERMAL_NO_BUDGET_WAKE", CompletionStatus::kError,
       "Wake rejected because the motor lacks thermal budget."},
      {"NET_BAD_PARAM", nullptr, CompletionStatus::kError,
       "Network credentials payload is invalid."},
      {"NET_SAVE_FAILED", nullptr, CompletionStatus::kError,
       "Failed to persist Wi-Fi credentials."},
      {"NET_SCAN_AP_ONLY", nullptr, CompletionStatus::kError,
       "Wi-Fi scan allowed only when device is in AP mode."},
      {"NET_BUSY_CONNECTING", nullptr, CompletionStatus::kError,
       "Wi-Fi subsystem is busy connecting; request deferred."},
      {"NET_CONNECT_FAILED", nullptr, CompletionStatus::kError,
      "Wi-Fi connection attempt failed."},
      {"MQTT_BAD_PAYLOAD", nullptr, CompletionStatus::kError,
       "MQTT command payload failed validation."},
      {"MQTT_UNSUPPORTED_ACTION", nullptr, CompletionStatus::kError,
       "Requested command action is not supported over MQTT."},
      {"MQTT_BAD_PARAM", nullptr, CompletionStatus::kError,
       "MQTT command parameters failed validation."},
      {"MQTT_CONFIG_SAVE_FAILED", nullptr, CompletionStatus::kError,
       "Failed to persist MQTT configuration changes."},
  };
  return kCatalog;
}

ResponseLine MakeControlLine(ResponseLineType type,
                             const std::string &msg_id,
                             const std::string &code,
                             const std::string &reason,
                             std::initializer_list<Field> fields) {
  ResponseLine line;
  line.type = type;
  line.msg_id = msg_id;
  line.code = code;
  line.reason = reason;
  line.fields.assign(fields.begin(), fields.end());
  return line;
}

} // namespace

const std::vector<ErrorDescriptor> &ErrorCatalog() {
  return BuildCatalog();
}

const ErrorDescriptor *LookupError(const std::string &code) {
  const auto &catalog = BuildCatalog();
  auto it = std::find_if(catalog.begin(), catalog.end(),
                         [&](const ErrorDescriptor &entry) {
                           return code == entry.code;
                         });
  if (it != catalog.end()) {
    return &*it;
  }
  return nullptr;
}

ResponseLine MakeAckLine(const std::string &msg_id,
                         std::initializer_list<Field> fields) {
  ResponseLine line;
  line.type = ResponseLineType::kAck;
  line.msg_id = msg_id;
  line.fields.assign(fields.begin(), fields.end());
  return line;
}

ResponseLine MakeWarnLine(const std::string &msg_id,
                          const std::string &code,
                          const std::string &reason,
                          std::initializer_list<Field> fields) {
  return MakeControlLine(ResponseLineType::kWarn, msg_id, code, reason, fields);
}

ResponseLine MakeInfoLine(const std::string &msg_id,
                          const std::string &code,
                          const std::string &reason,
                          std::initializer_list<Field> fields) {
  return MakeControlLine(ResponseLineType::kInfo, msg_id, code, reason, fields);
}

ResponseLine MakeErrorLine(const std::string &msg_id,
                           const std::string &code,
                           const std::string &reason,
                           std::initializer_list<Field> fields) {
  ResponseLine line = MakeControlLine(ResponseLineType::kError, msg_id, code, reason, fields);
  return line;
}

ResponseLine MakeDataLine(std::initializer_list<Field> fields) {
  ResponseLine line;
  line.type = ResponseLineType::kData;
  line.fields.assign(fields.begin(), fields.end());
  return line;
}

std::string FormatSerialResponse(const Response &response) {
  if (response.lines.empty()) {
    return {};
  }
  std::ostringstream oss;
  for (size_t i = 0; i < response.lines.size(); ++i) {
    if (i > 0) {
      oss << '\n';
    }
    oss << SerializeLine(response.lines[i]);
  }
  return oss.str();
}

std::string SerializeLine(const ResponseLine &line) {
  if (!line.raw.empty()) {
    return line.raw;
  }
  std::ostringstream oss;
  switch (line.type) {
  case ResponseLineType::kAck:
    oss << "CTRL:ACK";
    if (!line.msg_id.empty()) {
      oss << " msg_id=" << line.msg_id;
    }
    for (const auto &field : line.fields) {
      oss << ' ' << field.key << '=' << field.value;
    }
    break;
  case ResponseLineType::kWarn:
    oss << "CTRL:WARN";
    if (!line.msg_id.empty()) {
      oss << " msg_id=" << line.msg_id;
    }
    if (!line.code.empty()) {
      oss << ' ' << line.code;
    }
    if (!line.reason.empty()) {
      oss << ' ' << line.reason;
    }
    for (const auto &field : line.fields) {
      oss << ' ' << field.key << '=' << field.value;
    }
    break;
  case ResponseLineType::kError:
    oss << "CTRL:ERR";
    if (!line.msg_id.empty()) {
      oss << " msg_id=" << line.msg_id;
    }
    if (!line.code.empty()) {
      oss << ' ' << line.code;
    }
    if (!line.reason.empty()) {
      oss << ' ' << line.reason;
    }
    for (const auto &field : line.fields) {
      oss << ' ' << field.key << '=' << field.value;
    }
    break;
  case ResponseLineType::kInfo:
    oss << "CTRL:INFO";
    if (!line.msg_id.empty()) {
      oss << " msg_id=" << line.msg_id;
    }
    if (!line.code.empty()) {
      oss << ' ' << line.code;
    }
    if (!line.reason.empty()) {
      oss << ' ' << line.reason;
    }
    for (const auto &field : line.fields) {
      oss << ' ' << field.key << '=' << field.value;
    }
    break;
  case ResponseLineType::kData:
  case ResponseLineType::kUnknown:
  default:
    for (size_t i = 0; i < line.fields.size(); ++i) {
      if (i > 0) {
        oss << ' ';
      }
      oss << line.fields[i].key << '=' << line.fields[i].value;
    }
    break;
  }
  return oss.str();
}

const ResponseLine *FindAckLine(const Response &response) {
  for (const auto &line : response.lines) {
    if (line.type == ResponseLineType::kAck) {
      return &line;
    }
  }
  return nullptr;
}

const ResponseLine *FindPrimaryError(const Response &response) {
  for (const auto &line : response.lines) {
    if (line.type == ResponseLineType::kError) {
      return &line;
    }
  }
  return nullptr;
}

std::vector<ResponseLine> CollectWarnings(const Response &response) {
  std::vector<ResponseLine> warnings;
  for (const auto &line : response.lines) {
    if (line.type == ResponseLineType::kWarn) {
      warnings.push_back(line);
    }
  }
  return warnings;
}

CompletionStatus DeriveCompletionStatus(const Response &response) {
  if (const ResponseLine *err = FindPrimaryError(response)) {
    if (!err->code.empty()) {
      if (const ErrorDescriptor *desc = LookupError(err->code)) {
        return desc->status;
      }
    }
    return CompletionStatus::kError;
  }
  if (FindAckLine(response)) {
    return CompletionStatus::kOk;
  }
  return CompletionStatus::kUnknown;
}

} // namespace command
} // namespace transport
