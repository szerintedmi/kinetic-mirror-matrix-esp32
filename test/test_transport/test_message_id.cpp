#include <unity.h>

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>

#include "transport/MessageId.h"

namespace {

std::string makeId(uint64_t value) {
  char buf[37];
  std::snprintf(buf, sizeof(buf),
                "00000000-0000-4000-8000-%012llx",
                static_cast<unsigned long long>(value));
  return std::string(buf);
}

void resetGenerator() {
  transport::message_id::ResetGenerator();
  transport::message_id::ClearActive();
}

} // namespace

void setUp() {
  // no-op
}

void tearDown() {
  resetGenerator();
}

void test_next_skips_active_id() {
  transport::message_id::SetGenerator([]() mutable {
    static uint64_t counter = 0;
    return makeId(counter++);
  });

  transport::message_id::SetActive(makeId(1));
  auto first = transport::message_id::Next();
  TEST_ASSERT_TRUE_MESSAGE(std::strcmp(makeId(1).c_str(), first.c_str()) != 0,
                           "Next id should differ from active id");
  TEST_ASSERT_EQUAL_UINT32(36, static_cast<uint32_t>(first.size()));
}

void test_next_avoids_consecutive_duplicates() {
  transport::message_id::SetGenerator([]() mutable {
    static int call = 0;
    if (call == 0) {
      ++call;
      return makeId(10);
    }
    if (call == 1) {
      ++call;
      return makeId(10);
    }
    return makeId(10 + call++);
  });

  auto id1 = transport::message_id::Next();
  auto id2 = transport::message_id::Next();
  TEST_ASSERT_TRUE_MESSAGE(std::strcmp(id1.c_str(), id2.c_str()) != 0,
                           "Consecutive ids must differ");
}

void test_active_roundtrip() {
  const auto expected = makeId(99);
  transport::message_id::SetActive(expected);
  TEST_ASSERT_TRUE(transport::message_id::HasActive());
  TEST_ASSERT_EQUAL_STRING(expected.c_str(), transport::message_id::Active().c_str());
  transport::message_id::ClearActive();
  TEST_ASSERT_FALSE(transport::message_id::HasActive());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_next_skips_active_id);
  RUN_TEST(test_next_avoids_consecutive_duplicates);
  RUN_TEST(test_active_roundtrip);
  // Dispatcher/HELP round-trip regressions
  void test_dispatcher_round_trip_help_has_payload();
  void test_event_raw_preserved();
  RUN_TEST(test_dispatcher_round_trip_help_has_payload);
  RUN_TEST(test_event_raw_preserved);
  return UNITY_END();
}
