#include "MotorControl/command/CommandUtils.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace motor {
namespace command {

std::string Trim(const std::string &s) {
  size_t a = 0;
  while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) {
    ++a;
  }
  size_t b = s.size();
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) {
    --b;
  }
  return s.substr(a, b - a);
}

std::string ToUpperCopy(const std::string &s) {
  std::string out = s;
  std::transform(out.begin(), out.end(), out.begin(), ::toupper);
  return out;
}

std::vector<std::string> Split(const std::string &s, char delim) {
  std::vector<std::string> out;
  std::string cur;
  std::stringstream ss(s);
  while (std::getline(ss, cur, delim)) {
    out.push_back(cur);
  }
  return out;
}

std::vector<std::string> ParseCsvQuoted(const std::string &s) {
  std::vector<std::string> out;
  std::string cur;
  bool in_quotes = false;
  bool escape = false;
  for (char c : s) {
    if (in_quotes) {
      if (escape) {
        if (c == '"' || c == '\\') {
          cur.push_back(c);
        } else {
          cur.push_back(c);
        }
        escape = false;
      } else if (c == '\\') {
        escape = true;
      } else if (c == '"') {
        in_quotes = false;
      } else {
        cur.push_back(c);
      }
    } else {
      if (c == ',') {
        out.push_back(Trim(cur));
        cur.clear();
      } else if (c == '"') {
        if (!Trim(cur).empty()) {
          cur.push_back(c);
        } else {
          cur.clear();
          in_quotes = true;
        }
      } else {
        cur.push_back(c);
      }
    }
  }
  if (in_quotes) {
    return {};
  }
  out.push_back(Trim(cur));
  return out;
}

std::string QuoteString(const std::string &s) {
  std::string out;
  out.reserve(s.size() + 2);
  out.push_back('"');
  for (char c : s) {
    if (c == '"' || c == '\\') {
      out.push_back('\\');
    }
    out.push_back(c);
  }
  out.push_back('"');
  return out;
}

bool ParseInt(const std::string &s, long &out) {
  if (s.empty()) {
    return false;
  }
  char *end = nullptr;
  long v = std::strtol(s.c_str(), &end, 10);
  if (*end != '\0') {
    return false;
  }
  out = v;
  return true;
}

bool ParseInt(const std::string &s, int &out) {
  long tmp;
  if (!ParseInt(s, tmp)) {
    return false;
  }
  out = static_cast<int>(tmp);
  return true;
}

bool ParseIdMask(const std::string &token, uint32_t &mask, uint8_t maxMotors) {
  std::string upper = ToUpperCopy(token);
  if (upper == "ALL") {
    if (maxMotors >= 32) {
      mask = 0xFFFFFFFFu;
    } else {
      mask = (maxMotors == 0) ? 0u : ((1u << maxMotors) - 1u);
    }
    return true;
  }
  long id;
  if (!ParseInt(token, id)) {
    return false;
  }
  if (id < 0 || id >= maxMotors) {
    return false;
  }
  mask = (1u << static_cast<uint8_t>(id));
  return true;
}

} // namespace command
} // namespace motor

