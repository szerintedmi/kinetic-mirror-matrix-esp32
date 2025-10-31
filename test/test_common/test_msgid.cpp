#include <cstring>
#include <unity.h>

#include "MotorControl/MotorCommandProcessor.h"
#include "transport/CommandSchema.h"
#include "transport/MessageId.h"

void test_status_msg_ids_unique() {
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
  auto second = proc.execute("STATUS", 0);
  TEST_ASSERT_FALSE(first.is_error);
  TEST_ASSERT_FALSE(second.is_error);
  const auto &first_lines = first.structuredResponse().lines;
  const auto &second_lines = second.structuredResponse().lines;
  TEST_ASSERT_TRUE(first_lines.size() >= 1);
  TEST_ASSERT_TRUE(second_lines.size() >= 1);
  std::string first_text = transport::command::SerializeLine(first_lines[0]);
  std::string second_text = transport::command::SerializeLine(second_lines[0]);
  TEST_ASSERT_TRUE_MESSAGE(std::strcmp(first_text.c_str(),
                                       second_text.c_str()) != 0,
                          "STATUS msg_id repeated");

  transport::message_id::ResetGenerator();
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_status_msg_ids_unique);
  return UNITY_END();
}
