#pragma once

#include <string>
#include <vector>
#include <initializer_list>

namespace transport {
namespace command {

enum class LineType {
  kAck,
  kWarn,
  kError,
  kInfo,
  kData,
  kUnknown
};

struct Field {
  std::string key;
  std::string value;
};

struct Line {
  LineType type = LineType::kUnknown;
  std::string msg_id;
  std::string code;
  std::string reason;
  std::vector<Field> fields;
  std::vector<std::string> tokens;
  std::string raw;
};

struct Response {
  std::vector<Line> lines;
};

enum class CompletionStatus {
  kOk,
  kError,
  kUnknown
};

struct ErrorDescriptor {
  const char *code;
  const char *reason;
  CompletionStatus status;
  const char *description;
};

const std::vector<ErrorDescriptor> &ErrorCatalog();
const ErrorDescriptor *LookupError(const std::string &code);

Line MakeAckLine(const std::string &msg_id,
                 std::initializer_list<Field> fields = {});
Line MakeWarnLine(const std::string &msg_id,
                  const std::string &code,
                  const std::string &reason,
                  std::initializer_list<Field> fields = {});
Line MakeInfoLine(const std::string &msg_id,
                  const std::string &code,
                  const std::string &reason,
                  std::initializer_list<Field> fields = {});
Line MakeErrorLine(const std::string &msg_id,
                   const std::string &code,
                   const std::string &reason,
                   std::initializer_list<Field> fields = {});
Line MakeDataLine(std::initializer_list<Field> fields = {});

bool ParseCommandResponse(const std::vector<std::string> &lines,
                          Response &out,
                          std::string &error);

std::string FormatSerialResponse(const Response &response);
std::string SerializeLine(const Line &line);
CompletionStatus DeriveCompletionStatus(const Response &response);

const Line *FindAckLine(const Response &response);
std::vector<Line> CollectWarnings(const Response &response);
const Line *FindPrimaryError(const Response &response);

} // namespace command
} // namespace transport
