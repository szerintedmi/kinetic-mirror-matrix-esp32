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
  n.configureStatusLed(13, false);
  // Short timeout to keep tests snappy, though we expect AP immediately with no creds
  n.begin(200);
  auto s = n.status();
  TEST_ASSERT_EQUAL((int)State::AP_ACTIVE, (int)s.state);
  TEST_ASSERT_EQUAL_UINT8_MESSAGE(3, n.debugLedPattern(), "AP state should use fast blink pattern");
}

void test_set_creds_connect_timeout_falls_back_to_ap()
{
  NetOnboarding n;
  n.configureStatusLed(14, false);
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
  TEST_ASSERT_EQUAL_UINT8_MESSAGE(3, n.debugLedPattern(), "Fallback AP should restore fast blink");
}

void test_led_fast_blink_toggles_in_ap()
{
  NetOnboarding n;
  n.configureStatusLed(12, false);
  n.begin(150);
  TEST_ASSERT_EQUAL_UINT8(3, n.debugLedPattern());
  bool first = n.debugLedOn();
  bool toggled = false;
  auto start = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(350)) {
    n.loop();
    if (n.debugLedOn() != first) { toggled = true; break; }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  TEST_ASSERT_TRUE_MESSAGE(toggled, "Fast blink should toggle within 350ms");
}

void test_led_slow_blink_when_connecting()
{
  NetOnboarding n;
  n.configureStatusLed(11, false);
  n.setConnectTimeoutMs(600);
  n.begin(600);
  TEST_ASSERT_TRUE(n.setCredentials("ssid", "pass"));
  TEST_ASSERT_EQUAL_UINT8(2, n.debugLedPattern());
  bool first = n.debugLedOn();
  bool toggled = false;
  auto start = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(900)) {
    n.loop();
    if (n.debugLedOn() != first) { toggled = true; break; }
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
  }
  TEST_ASSERT_TRUE_MESSAGE(toggled, "Slow blink should toggle within 900ms");
}

void test_led_solid_when_connected()
{
  NetOnboarding n;
  n.configureStatusLed(10, false);
  n.setConnectTimeoutMs(200);
  n.begin(200);
  n.setTestSimulation(true, 20);
  TEST_ASSERT_TRUE(n.setCredentials("ssid", "pass"));
  auto start = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(400)) {
    n.loop();
    if (n.status().state == State::CONNECTED) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  TEST_ASSERT_EQUAL((int)State::CONNECTED, (int)n.status().state);
  TEST_ASSERT_EQUAL_UINT8(1, n.debugLedPattern());
  bool on = n.debugLedOn();
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  n.loop();
  TEST_ASSERT_EQUAL_MESSAGE(on, n.debugLedOn(), "Solid LED should not toggle");
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_begin_no_creds_enters_ap);
  RUN_TEST(test_set_creds_connect_timeout_falls_back_to_ap);
  RUN_TEST(test_led_fast_blink_toggles_in_ap);
  RUN_TEST(test_led_slow_blink_when_connecting);
  RUN_TEST(test_led_solid_when_connected);
  return UNITY_END();
}
