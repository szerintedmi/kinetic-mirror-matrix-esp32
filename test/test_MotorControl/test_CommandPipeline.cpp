#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <unity.h>

#include <cctype>
#include <cstdlib>

#include "MotorControl/MotorCommandProcessor.h"
#include "MotorControl/command/CommandParser.h"
#include "MotorControl/command/CommandUtils.h"
#include "MotorControl/command/ResponseFormatter.h"

using motor::command::CommandParser;
using motor::command::ParsedCommand;
using motor::command::ParseCsvQuoted;
using motor::command::Trim;

void test_parser_alias_to_upper() {
  CommandParser parser;
  auto commands = parser.parse("st");
  TEST_ASSERT_EQUAL_UINT32(1, commands.size());
  TEST_ASSERT_EQUAL_STRING("ST", commands[0].verb.c_str());
  TEST_ASSERT_TRUE(commands[0].args.empty());
}

void test_parser_handles_multicommands() {
  CommandParser parser;
  auto commands = parser.parse("move:0,120 ; sleep:0");
  TEST_ASSERT_EQUAL_UINT32(2, commands.size());
  TEST_ASSERT_EQUAL_STRING("MOVE", commands[0].verb.c_str());
  TEST_ASSERT_EQUAL_STRING("0,120", Trim(commands[0].args).c_str());
  TEST_ASSERT_EQUAL_STRING("SLEEP", commands[1].verb.c_str());
}

void test_parse_csv_with_quotes() {
  auto tokens = ParseCsvQuoted("SET,\"ssid,with,comma\",\"p\\\"ass\"");
  TEST_ASSERT_EQUAL_UINT32(3, tokens.size());
  TEST_ASSERT_EQUAL_STRING("SET", tokens[0].c_str());
  TEST_ASSERT_EQUAL_STRING("ssid,with,comma", tokens[1].c_str());
  TEST_ASSERT_EQUAL_STRING("p\"ass", tokens[2].c_str());
}

void test_batch_conflict_detection() {
  MotorCommandProcessor proc;
  std::string result = proc.processLine("MOVE:0,10;MOVE:0,50", 0);
  TEST_ASSERT_TRUE(result.rfind("CTRL:ERR", 0) == 0);
  TEST_ASSERT_NOT_EQUAL(std::string::npos, result.find("E03 BAD_PARAM MULTI_CMD_CONFLICT"));
}

void test_execute_matches_serial_output() {
  MotorCommandProcessor proc;
  auto structured = proc.execute("HELP", 0);
  std::string formatted = motor::command::FormatForSerial(structured);
  std::string legacy = proc.processLine("HELP", 0);
  TEST_ASSERT_FALSE(structured.is_error);
  TEST_ASSERT_EQUAL_STRING(legacy.c_str(), formatted.c_str());
  TEST_ASSERT_TRUE(structured.lines.size() > 1);
}

void test_execute_reports_errors_structurally() {
  MotorCommandProcessor proc;
  auto structured = proc.execute("FOO", 0);
  TEST_ASSERT_TRUE(structured.is_error);
  TEST_ASSERT_EQUAL_UINT32(1, structured.lines.size());
  TEST_ASSERT_TRUE(structured.lines[0].find("E01 BAD_CMD") != std::string::npos);
  std::string formatted = motor::command::FormatForSerial(structured);
  TEST_ASSERT_EQUAL_STRING(formatted.c_str(), structured.lines[0].c_str());
}

static uint32_t cid_from(const std::string &line) {
  auto pos = line.find("CID=");
  TEST_ASSERT_TRUE(pos != std::string::npos);
  pos += 4;
  size_t end = pos;
  while (end < line.size() && isdigit(static_cast<unsigned char>(line[end]))) ++end;
  return static_cast<uint32_t>(strtoul(line.substr(pos, end - pos).c_str(), nullptr, 10));
}

void test_batch_aggregates_estimate() {
  MotorCommandProcessor proc;
  auto structured = proc.execute("MOVE:0,100;MOVE:1,200", 0);
  TEST_ASSERT_FALSE(structured.is_error);
  TEST_ASSERT_EQUAL_UINT32(1, structured.lines.size());
  TEST_ASSERT_TRUE(structured.lines[0].find("CTRL:ACK") == 0);
  TEST_ASSERT_TRUE(structured.lines[0].find("est_ms=") != std::string::npos);
}

void test_execute_cid_increments() {
  MotorCommandProcessor proc;
  auto first = proc.execute("STATUS", 0);
  TEST_ASSERT_FALSE(first.is_error);
  TEST_ASSERT_TRUE(first.lines.size() >= 1);
  uint32_t cid1 = cid_from(first.lines[0]);
  auto second = proc.execute("STATUS", 0);
  TEST_ASSERT_FALSE(second.is_error);
  TEST_ASSERT_TRUE(second.lines.size() >= 1);
  uint32_t cid2 = cid_from(second.lines[0]);
  TEST_ASSERT_TRUE(cid2 != cid1);
}
