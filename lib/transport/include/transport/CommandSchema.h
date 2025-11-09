#pragma once

#include <initializer_list>
#include <string>
#include <vector>

namespace transport {
namespace command {

enum class ResponseLineType { kAck, kWarn, kError, kInfo, kData, kUnknown };

struct Field {
  std::string key;
  std::string value;
};

struct ResponseLine {
  ResponseLineType type = ResponseLineType::kUnknown;
  std::string msg_id;
  std::string code;
  std::string reason;
  std::vector<Field> fields;
  std::vector<std::string> tokens;
  std::string raw;
};

struct Response {
  std::vector<ResponseLine> lines;
};

enum class CompletionStatus { kOk, kError, kUnknown };

struct ErrorDescriptor {
  const char* code;
  const char* reason;
  CompletionStatus status;
  const char* description;
};

const std::vector<ErrorDescriptor>& ErrorCatalog();
const ErrorDescriptor* LookupError(const std::string& code);

ResponseLine MakeAckLine(const std::string& msg_id, std::initializer_list<Field> fields = {});
ResponseLine MakeWarnLine(const std::string& msg_id,
                          const std::string& code,
                          const std::string& reason,
                          std::initializer_list<Field> fields = {});
ResponseLine MakeInfoLine(const std::string& msg_id,
                          const std::string& code,
                          const std::string& reason,
                          std::initializer_list<Field> fields = {});
ResponseLine MakeErrorLine(const std::string& msg_id,
                           const std::string& code,
                           const std::string& reason,
                           std::initializer_list<Field> fields = {});
ResponseLine MakeDataLine(std::initializer_list<Field> fields = {});

std::string FormatSerialResponse(const Response& response);
std::string SerializeLine(const ResponseLine& line);
CompletionStatus DeriveCompletionStatus(const Response& response);

const ResponseLine* FindAckLine(const Response& response);
std::vector<ResponseLine> CollectWarnings(const Response& response);
const ResponseLine* FindPrimaryError(const Response& response);

}  // namespace command
}  // namespace transport
