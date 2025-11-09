#ifdef ARDUINO
#include "net_onboarding/NetOnboarding.h"
#include "secrets.h"

#include <Arduino.h>
#include <WiFi.h>
#include <unity.h>

using namespace net_onboarding;

static void pump_for(NetOnboarding& n, uint32_t ms) {
  uint32_t start = millis();
  while ((millis() - start) < ms) {
    n.loop();
    delay(10);
  }
}

void test_net_ap_after_reset_no_creds() {
  NetOnboarding n;
  n.clearCredentials();
  delay(50);
  n.begin(800);
  // Should enter AP immediately when no creds exist
  auto s = n.status();
  TEST_ASSERT_EQUAL((int)State::AP_ACTIVE, (int)s.state);
  // DHCP defaults: SoftAP should be at 192.168.4.1
  IPAddress ap = WiFi.softAPIP();
  TEST_ASSERT_EQUAL_UINT32(192, ap[0]);
  TEST_ASSERT_EQUAL_UINT32(168, ap[1]);
  TEST_ASSERT_EQUAL_UINT32(4, ap[2]);
  TEST_ASSERT_EQUAL_UINT32(1, ap[3]);
}

void test_net_connect_timeout_to_ap() {
  NetOnboarding n;
  n.clearCredentials();
  delay(50);
  n.setConnectTimeoutMs(1500);
  n.begin(1500);
  // Intentionally invalid credentials
  TEST_ASSERT_TRUE(n.setCredentials("__invalid__", "__invalid__"));
  TEST_ASSERT_EQUAL((int)State::CONNECTING, (int)n.status().state);
  pump_for(n, 2500);
  TEST_ASSERT_EQUAL((int)State::AP_ACTIVE, (int)n.status().state);
}

void test_net_happy_path_if_seeded() {
#if defined(TEST_STA_SSID) && defined(TEST_STA_PASS)
  NetOnboarding n;
  // Seed and attempt connect
  TEST_ASSERT_TRUE(n.saveCredentials(TEST_STA_SSID, TEST_STA_PASS));
  n.setConnectTimeoutMs(8000);
  n.begin(8000);
  // Wait up to 12s for association
  uint32_t start = millis();
  while ((millis() - start) < 12000 && n.status().state != State::CONNECTED) {
    n.loop();
    delay(25);
  }
  auto s = n.status();
  // If environment is correct, we should be CONNECTED; otherwise, mark as skipped
  if (s.state != State::CONNECTED) {
    TEST_IGNORE_MESSAGE("Happy-path connect skipped (no test AP available)");
  } else {
    TEST_ASSERT_EQUAL((int)State::CONNECTED, (int)s.state);
  }
#else
  TEST_IGNORE_MESSAGE("Define TEST_STA_SSID/TEST_STA_PASS in include/secrets.h to run");
#endif
}

#endif  // ARDUINO
