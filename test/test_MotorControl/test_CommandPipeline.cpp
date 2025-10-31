#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <unity.h>

#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "MotorControl/MotorCommandProcessor.h"
#include "MotorControl/command/CommandParser.h"
#include "MotorControl/command/CommandUtils.h"
#include "MotorControl/command/ResponseFormatter.h"
#include "transport/CommandSchema.h"
#include "transport/MessageId.h"

using motor::command::CommandParser;
using motor::command::ParsedCommand;
using motor::command::ParseCsvQuoted;
using motor::command::Trim;

void test_parser_alias_to_upper() {
  CommandParser parser;
  auto commands = parser.parse("st");
  TEST_ASSERT_EQUAL_UINT32(1, commands.size());
  TEST_ASSERT_EQUAL_STRING("ST", commands[0].action.c_str());
  TEST_ASSERT_TRUE(commands[0].args.empty());
}

void test_parser_handles_multicommands() {
  CommandParser parser;
  auto commands = parser.parse("move:0,120 ; sleep:0");
  TEST_ASSERT_EQUAL_UINT32(2, commands.size());
  TEST_ASSERT_EQUAL_STRING("MOVE", commands[0].action.c_str());
  TEST_ASSERT_EQUAL_STRING("0,120", Trim(commands[0].args).c_str());
  TEST_ASSERT_EQUAL_STRING("SLEEP", commands[1].action.c_str());
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
  std::string serial_output = proc.processLine("HELP", 0);
  TEST_ASSERT_FALSE(structured.is_error);
  TEST_ASSERT_EQUAL_STRING(serial_output.c_str(), formatted.c_str());
  TEST_ASSERT_TRUE(structured.structuredResponse().lines.size() > 1);
}

void test_execute_reports_errors_structurally() {
  MotorCommandProcessor proc;
  auto structured = proc.execute("FOO", 0);
  TEST_ASSERT_TRUE(structured.is_error);
  const auto &lines = structured.structuredResponse().lines;
  TEST_ASSERT_EQUAL_UINT32(1, lines.size());
  std::string line_text = transport::command::SerializeLine(lines[0]);
  TEST_ASSERT_TRUE(line_text.find("E01 BAD_CMD") != std::string::npos);
  std::string formatted = motor::command::FormatForSerial(structured);
  TEST_ASSERT_EQUAL_STRING(formatted.c_str(), line_text.c_str());
}

static std::string msg_id_from(const std::string &line) {
  auto pos = line.find("msg_id=");
  TEST_ASSERT_TRUE(pos != std::string::npos);
  pos += 6;  // skip past 'msg_id='
  size_t end = pos;
  while (end < line.size() && !isspace(static_cast<unsigned char>(line[end]))) {
    ++end;
  }
  auto id = line.substr(pos, end - pos);
  TEST_ASSERT_TRUE(id.size() == 36);
  TEST_ASSERT_EQUAL_CHAR('-', id[8]);
  TEST_ASSERT_EQUAL_CHAR('-', id[13]);
  TEST_ASSERT_EQUAL_CHAR('-', id[18]);
  TEST_ASSERT_EQUAL_CHAR('-', id[23]);
  return id;
}

void test_batch_aggregates_estimate() {
  MotorCommandProcessor proc;
  auto structured = proc.execute("MOVE:0,100;MOVE:1,200", 0);
  TEST_ASSERT_FALSE(structured.is_error);
  const auto &lines = structured.structuredResponse().lines;
  TEST_ASSERT_EQUAL_UINT32(1, lines.size());
  std::string line_text = transport::command::SerializeLine(lines[0]);
  TEST_ASSERT_TRUE(line_text.find("CTRL:ACK") == 0);
  TEST_ASSERT_TRUE(line_text.find("est_ms=") != std::string::npos);
}

void test_execute_cid_increments() {
  transport::message_id::SetGenerator([]() mutable {
    static uint64_t counter = 0;
    char buf[37];
    std::snprintf(buf, sizeof(buf),
                  "00000000-0000-4000-8000-%012llx",
                  static_cast<unsigned long long>(counter++));
    return std::string(buf);
  });
  MotorCommandProcessor proc;
  auto first = proc.execute("STATUS", 0);
  TEST_ASSERT_FALSE(first.is_error);
  const auto &first_lines = first.structuredResponse().lines;
  TEST_ASSERT_TRUE(first_lines.size() >= 1);
  auto second = proc.execute("STATUS", 0);
  TEST_ASSERT_FALSE(second.is_error);
  const auto &second_lines = second.structuredResponse().lines;
  TEST_ASSERT_TRUE(second_lines.size() >= 1);
  std::string first_text = transport::command::SerializeLine(first_lines[0]);
  std::string second_text = transport::command::SerializeLine(second_lines[0]);
  TEST_ASSERT_TRUE_MESSAGE(std::strcmp(first_text.c_str(), second_text.c_str()) != 0,
                          ("duplicate msg_id first=" + first_text + " second=" + second_text).c_str());
  transport::message_id::ResetGenerator();
}
