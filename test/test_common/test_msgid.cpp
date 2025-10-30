#include <cstring>
#include <unity.h>

#include "MotorControl/MotorCommandProcessor.h"
#include "net_onboarding/MessageId.h"

void test_status_msg_ids_unique() {
  net_onboarding::SetMsgIdGenerator([]() mutable {
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
  TEST_ASSERT_TRUE(first.lines.size() >= 1);
  TEST_ASSERT_TRUE(second.lines.size() >= 1);
  TEST_ASSERT_TRUE_MESSAGE(std::strcmp(first.lines[0].c_str(),
                                       second.lines[0].c_str()) != 0,
                          "STATUS msg_id repeated");

  net_onboarding::ResetMsgIdGenerator();
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_status_msg_ids_unique);
  return UNITY_END();
}
