#pragma once

#include <string>
#include <vector>

namespace test_helpers {

// Split string by newlines into vector of lines
inline std::vector<std::string> SplitLines(const std::string& s) {
  std::vector<std::string> out;
  size_t start = 0;
  while (start <= s.size()) {
    size_t pos = s.find('\n', start);
    if (pos == std::string::npos) {
      out.push_back(s.substr(start));
      break;
    }
    out.push_back(s.substr(start, pos - start));
    start = pos + 1;
  }
  return out;
}

// Find STATUS line for a specific motor ID (e.g., "id=0 pos=123 ...")
// Returns empty string if not found
inline std::string FindStatusLineForId(const std::vector<std::string>& lines, int id) {
  std::string prefix = std::string("id=") + std::to_string(id);
  for (const auto& line : lines) {
    if (line.rfind(prefix, 0) == 0) {
      return line;
    }
  }
  return "";
}

// Check if a line contains a specific key (substring match)
inline bool LineHasKey(const std::string& line, const std::string& key) {
  return line.find(key) != std::string::npos;
}

// Parse est_ms=NNN from an ACK response, returns 0 if not found
inline uint32_t ParseEstMs(const std::string& s) {
  auto pos = s.find("est_ms=");
  if (pos == std::string::npos) {
    return 0;
  }
  pos += 7;
  size_t end = pos;
  while (end < s.size() && isdigit(static_cast<unsigned char>(s[end]))) {
    ++end;
  }
  return static_cast<uint32_t>(strtoul(s.substr(pos, end - pos).c_str(), nullptr, 10));
}

// Parse msg_id=XXX from a response line, returns empty string if not found
inline std::string ParseMsgId(const std::string& line) {
  auto pos = line.find("msg_id=");
  if (pos == std::string::npos) {
    return "";
  }
  pos += 7;
  size_t end = pos;
  while (end < line.size() && !isspace(static_cast<unsigned char>(line[end]))) {
    ++end;
  }
  return line.substr(pos, end - pos);
}

// Skip CTRL: prefix lines and return index of first data line
inline size_t SkipCtrlLines(const std::vector<std::string>& lines) {
  size_t idx = 0;
  while (idx < lines.size() && lines[idx].rfind("CTRL:", 0) == 0) {
    ++idx;
  }
  return idx;
}

}  // namespace test_helpers
