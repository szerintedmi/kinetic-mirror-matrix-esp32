#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace motor {
namespace command {

// Trim leading/trailing whitespace.
std::string Trim(const std::string &s);

// Upper-case copy helper (ASCII only, mirroring legacy behavior).
std::string ToUpperCopy(const std::string &s);

// Split by delimiter (no trimming of segments).
std::vector<std::string> Split(const std::string &s, char delim);

// Parse CSV with support for quoted fields (\"" and "\\\\" escapes).
std::vector<std::string> ParseCsvQuoted(const std::string &s);

// Helper mirroring legacy quoting for key=value outputs.
std::string QuoteString(const std::string &s);

// String-to-int helpers that mirror the legacy boolean-returning API.
bool ParseInt(const std::string &s, long &out);
bool ParseInt(const std::string &s, int &out);

// Parse ID mask tokens such as "ALL" or "0,1,2".
// maxMotors defaults to 8 to preserve existing semantics.
bool ParseIdMask(const std::string &token, uint32_t &mask, uint8_t maxMotors = 8);

} // namespace command
} // namespace motor

