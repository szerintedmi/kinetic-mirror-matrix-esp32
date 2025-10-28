#include <unity.h>
#include <string>
#include <thread>
#include <chrono>
#include "net_onboarding/NetOnboarding.h"

using namespace net_onboarding;

void setUp() {}
void tearDown() {}

void test_begin_no_creds_enters_ap()
{
  NetOnboarding n;
  // Short timeout to keep tests snappy, though we expect AP immediately with no creds
  n.begin(200);
  auto s = n.status();
  TEST_ASSERT_EQUAL((int)State::AP_ACTIVE, (int)s.state);
}

void test_set_creds_connect_timeout_falls_back_to_ap()
{
  NetOnboarding n;
  n.setConnectTimeoutMs(150);
  n.begin(150);
  // Simulate a connection attempt that never succeeds in native
  TEST_ASSERT_TRUE(n.setCredentials("ssid", "pass"));
  // Should immediately enter CONNECTING
  TEST_ASSERT_EQUAL((int)State::CONNECTING, (int)n.status().state);

  // Pump loop for > timeout with real sleeps to advance steady_clock
  auto t_start = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - t_start < std::chrono::milliseconds(220)) {
    n.loop();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  TEST_ASSERT_EQUAL((int)State::AP_ACTIVE, (int)n.status().state);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_begin_no_creds_enters_ap);
  RUN_TEST(test_set_creds_connect_timeout_falls_back_to_ap);
  return UNITY_END();
}
