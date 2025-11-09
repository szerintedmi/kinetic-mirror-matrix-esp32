#pragma once

#include "transport/CommandSchema.h"

#include <string>
#include <vector>

namespace motor {
namespace command {

struct CommandResult {
  bool is_error = false;
  transport::command::Response structured;

  void append(const transport::command::ResponseLine& line) {
    structured.lines.push_back(line);
  }

  void mergeFrom(const CommandResult& other) {
    if (&other == this) {
      return;
    }
    for (const auto& line : other.structured.lines) {
      append(line);
    }
    if (other.is_error) {
      is_error = true;
    }
  }

  bool hasStructuredResponse() const {
    return !structured.lines.empty();
  }

  const transport::command::Response& structuredResponse() const {
    return structured;
  }

  void clearStructured() {
    structured.lines.clear();
  }

  // Convenience helpers
  static CommandResult Error(const transport::command::ResponseLine& line) {
    CommandResult r;
    r.is_error = true;
    r.append(line);
    return r;
  }

  static CommandResult SingleLine(const transport::command::ResponseLine& line) {
    CommandResult r;
    r.append(line);
    return r;
  }
};

}  // namespace command
}  // namespace motor
