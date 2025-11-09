#include "MotorControl/MotorCommandProcessor.h"
#include "transport/CommandSchema.h"
#include "transport/ResponseDispatcher.h"
#include "transport/ResponseModel.h"

#include <algorithm>
#include <string>
#include <unity.h>
#include <vector>

using transport::response::ResponseDispatcher;

static bool starts_with(const std::string& s, const char* prefix) {
  return s.rfind(prefix, 0) == 0;
}

void test_dispatcher_round_trip_help_has_payload() {
  // Simulate SerialConsole sink: Event -> Line, prefer raw if set.
  std::vector<std::string> printed;
  auto token = ResponseDispatcher::Instance().RegisterSink(
      [&printed](const transport::response::Event& evt) {
        auto line = transport::response::EventToLine(evt);
        std::string text = line.raw.empty() ? transport::command::SerializeLine(line) : line.raw;
        if (!text.empty()) {
          printed.push_back(text);
        }
      });

  MotorCommandProcessor proc;
  (void)proc.execute("HELP", 0);

  ResponseDispatcher::Instance().UnregisterSink(token);

  TEST_ASSERT_TRUE_MESSAGE(!printed.empty(), "No events printed for HELP");
  bool any_non_ctrl = false;
  for (const auto& ln : printed) {
    if (!starts_with(ln, "CTRL:")) {
      any_non_ctrl = true;
      break;
    }
  }
  TEST_ASSERT_TRUE_MESSAGE(any_non_ctrl, "Expected at least one non-CTRL line in HELP output");
}

void test_event_raw_preserved() {
  // When a ResponseLine carries raw text, it should survive Event->Line.
  transport::command::ResponseLine in;
  in.type = transport::command::ResponseLineType::kInfo;
  in.raw = "HELPTEXT";
  auto evt = transport::response::BuildEvent(in, "HELP");
  auto out = transport::response::EventToLine(evt);
  TEST_ASSERT_EQUAL_STRING("HELPTEXT", out.raw.c_str());
}
