#include "transport/CommandSchema.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>
#include <initializer_list>

namespace transport {
namespace command {

namespace {

std::string Trim(const std::string &value) {
  size_t start = 0;
  while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }
  size_t end = value.size();
  while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }
  return value.substr(start, end - start);
}

std::vector<std::string> SplitTokens(const std::string &text) {
  std::vector<std::string> tokens;
  std::string current;
  bool in_quotes = false;
  bool escape_next = false;
  for (char ch : text) {
    if (escape_next) {
      current.push_back(ch);
      escape_next = false;
      continue;
    }
    if (ch == '\\') {
      escape_next = true;
      current.push_back(ch);
      continue;
    }
    if (ch == '"') {
      in_quotes = !in_quotes;
      current.push_back(ch);
      continue;
    }
    if (std::isspace(static_cast<unsigned char>(ch)) && !in_quotes) {
      if (!current.empty()) {
        tokens.push_back(current);
        current.clear();
      }
      continue;
    }
    current.push_back(ch);
  }
  if (!current.empty()) {
    tokens.push_back(current);
  }
  return tokens;
}

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
  };
  return kCatalog;
}

LineType DetermineType(const std::string &line) {
  if (line.rfind("CTRL:ACK", 0) == 0) {
    return LineType::kAck;
  }
  if (line.rfind("CTRL:WARN", 0) == 0) {
    return LineType::kWarn;
  }
  if (line.rfind("CTRL:ERR", 0) == 0) {
    return LineType::kError;
  }
  if (line.rfind("CTRL:INFO", 0) == 0) {
    return LineType::kInfo;
  }
  if (line.rfind("CTRL:", 0) == 0) {
    return LineType::kUnknown;
  }
  return LineType::kData;
}

void PushDetail(std::vector<Field> &fields, const std::string &value) {
  if (value.empty()) {
    return;
  }
  fields.push_back({"detail", value});
}

void ParseControlTokens(const std::vector<std::string> &tokens,
                        Line &out,
                        LineType type) {
  out.tokens = tokens;
  size_t idx = 0;
  if (idx < tokens.size() && tokens[idx].rfind("msg_id=", 0) == 0) {
    out.msg_id = tokens[idx].substr(7);
    ++idx;
  }

  if (type == LineType::kError) {
    if (idx < tokens.size()) {
      out.code = tokens[idx++];
    }
    if (idx < tokens.size() && tokens[idx].find('=') == std::string::npos) {
      out.reason = tokens[idx++];
    }
    for (; idx < tokens.size(); ++idx) {
      const std::string &tok = tokens[idx];
      auto pos = tok.find('=');
      if (pos != std::string::npos) {
        Field f{tok.substr(0, pos), tok.substr(pos + 1)};
        out.fields.push_back(std::move(f));
      } else {
        PushDetail(out.fields, tok);
      }
    }
    if (out.code.empty()) {
      out.code = "UNKNOWN";
    }
  } else if (type == LineType::kWarn || type == LineType::kInfo) {
    if (idx < tokens.size()) {
      out.code = tokens[idx++];
    }
    if (idx < tokens.size() && tokens[idx].find('=') == std::string::npos) {
      out.reason = tokens[idx++];
    }
    for (; idx < tokens.size(); ++idx) {
      const std::string &tok = tokens[idx];
      auto pos = tok.find('=');
      if (pos != std::string::npos) {
        Field f{tok.substr(0, pos), tok.substr(pos + 1)};
        out.fields.push_back(std::move(f));
      } else {
        PushDetail(out.fields, tok);
      }
    }
    if (out.code.empty()) {
      out.code = type == LineType::kWarn ? "WARN" : "INFO";
    }
  } else if (type == LineType::kAck) {
    for (; idx < tokens.size(); ++idx) {
      const std::string &tok = tokens[idx];
      auto pos = tok.find('=');
      if (pos != std::string::npos) {
        Field f{tok.substr(0, pos), tok.substr(pos + 1)};
        out.fields.push_back(std::move(f));
      } else {
        PushDetail(out.fields, tok);
      }
    }
  }
}

void ParseDataLine(const std::string &line, Line &out) {
  auto tokens = SplitTokens(line);
  out.tokens = tokens;
  for (const auto &tok : tokens) {
    auto pos = tok.find('=');
    if (pos != std::string::npos) {
      Field f{tok.substr(0, pos), tok.substr(pos + 1)};
      out.fields.push_back(std::move(f));
    }
  }
}

Line MakeControlLine(LineType type,
                     const std::string &msg_id,
                     const std::string &code,
                     const std::string &reason,
                     std::initializer_list<Field> fields) {
  Line line;
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

Line MakeAckLine(const std::string &msg_id,
                 std::initializer_list<Field> fields) {
  Line line;
  line.type = LineType::kAck;
  line.msg_id = msg_id;
  line.fields.assign(fields.begin(), fields.end());
  return line;
}

Line MakeWarnLine(const std::string &msg_id,
                  const std::string &code,
                  const std::string &reason,
                  std::initializer_list<Field> fields) {
  return MakeControlLine(LineType::kWarn, msg_id, code, reason, fields);
}

Line MakeInfoLine(const std::string &msg_id,
                  const std::string &code,
                  const std::string &reason,
                  std::initializer_list<Field> fields) {
  return MakeControlLine(LineType::kInfo, msg_id, code, reason, fields);
}

Line MakeErrorLine(const std::string &msg_id,
                   const std::string &code,
                   const std::string &reason,
                   std::initializer_list<Field> fields) {
  Line line = MakeControlLine(LineType::kError, msg_id, code, reason, fields);
  return line;
}

Line MakeDataLine(std::initializer_list<Field> fields) {
  Line line;
  line.type = LineType::kData;
  line.fields.assign(fields.begin(), fields.end());
  return line;
}

bool ParseCommandResponse(const std::vector<std::string> &lines,
                          Response &out,
                          std::string &error) {
  out.lines.clear();
  error.clear();
  for (const auto &raw_line : lines) {
    if (raw_line.empty()) {
      continue;
    }
    LineType type = DetermineType(raw_line);
    Line parsed;
    parsed.type = type;
    parsed.raw = Trim(raw_line);

    if (type == LineType::kData) {
      ParseDataLine(parsed.raw, parsed);
    } else {
      size_t space = parsed.raw.find(' ');
      std::string payload;
      if (space != std::string::npos) {
        payload = parsed.raw.substr(space + 1);
      }
      auto tokens = SplitTokens(payload);
      ParseControlTokens(tokens, parsed, type);
      if (parsed.msg_id.empty() &&
          (type == LineType::kAck || type == LineType::kError || type == LineType::kWarn)) {
        // Missing msg_id is a soft error; record the issue but continue.
        if (error.empty()) {
          error = "Missing msg_id in control line: " + raw_line;
        }
      }
    }
    out.lines.push_back(std::move(parsed));
  }
  return true;
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

std::string SerializeLine(const Line &line) {
  if (!line.raw.empty()) {
    return line.raw;
  }
  std::ostringstream oss;
  switch (line.type) {
  case LineType::kAck:
    oss << "CTRL:ACK";
    if (!line.msg_id.empty()) {
      oss << " msg_id=" << line.msg_id;
    }
    for (const auto &field : line.fields) {
      oss << ' ' << field.key << '=' << field.value;
    }
    break;
  case LineType::kWarn:
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
  case LineType::kError:
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
  case LineType::kInfo:
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
  case LineType::kData:
  case LineType::kUnknown:
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

const Line *FindAckLine(const Response &response) {
  for (const auto &line : response.lines) {
    if (line.type == LineType::kAck) {
      return &line;
    }
  }
  return nullptr;
}

const Line *FindPrimaryError(const Response &response) {
  for (const auto &line : response.lines) {
    if (line.type == LineType::kError) {
      return &line;
    }
  }
  return nullptr;
}

std::vector<Line> CollectWarnings(const Response &response) {
  std::vector<Line> warnings;
  for (const auto &line : response.lines) {
    if (line.type == LineType::kWarn) {
      warnings.push_back(line);
    }
  }
  return warnings;
}

CompletionStatus DeriveCompletionStatus(const Response &response) {
  if (const Line *err = FindPrimaryError(response)) {
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
