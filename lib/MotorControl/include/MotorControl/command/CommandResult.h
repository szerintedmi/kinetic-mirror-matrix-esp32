#pragma once

#include <string>
#include <vector>

#include "transport/CommandSchema.h"

namespace motor {
namespace command {

struct CommandResult {
  bool is_error = false;
  std::vector<std::string> lines;
  bool has_structured = false;
  transport::command::Response structured;

  void append(const transport::command::Line &line) {
    lines.push_back(transport::command::SerializeLine(line));
    structured.lines.push_back(line);
    has_structured = true;
  }

  bool hasStructuredResponse() const {
    return has_structured;
  }

  const transport::command::Response &structuredResponse() const {
    return structured;
  }

  void clearStructured() {
    has_structured = false;
    structured.lines.clear();
  }

  // Convenience helpers
  static CommandResult Error(const std::string &line) {
    CommandResult r;
    r.is_error = true;
    r.lines.push_back(line);
    return r;
  }

  static CommandResult Error(const transport::command::Line &line) {
    CommandResult r;
    r.is_error = true;
    r.append(line);
    return r;
  }

  static CommandResult SingleLine(const std::string &line) {
    CommandResult r;
    r.lines.push_back(line);
    return r;
  }

  static CommandResult SingleLine(const transport::command::Line &line) {
    CommandResult r;
    r.append(line);
    return r;
  }
};

std::string JoinLines(const std::vector<std::string> &lines);

} // namespace command
} // namespace motor
