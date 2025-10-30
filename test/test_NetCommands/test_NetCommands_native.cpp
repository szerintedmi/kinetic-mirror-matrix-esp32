#include <unity.h>
#include <string>
#include <thread>
#include <chrono>
#include "MotorControl/MotorCommandProcessor.h"
#include "net_onboarding/NetOnboarding.h"
#include "net_onboarding/NetSingleton.h"

using namespace net_onboarding;

static MotorCommandProcessor proto;

void setUp() {
  proto = MotorCommandProcessor();
  // Ensure a clean singleton state for each test
  // Start in AP mode with no creds
  Net().clearCredentials();
  Net().setConnectTimeoutMs(200);
  Net().begin(200);
}

void tearDown() {}

static void pump_for(uint32_t ms) {
  auto t0 = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - t0 < std::chrono::milliseconds(ms)) {
    Net().loop();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}

void test_help_includes_net_verbs() {
  std::string help = proto.processLine("HELP", 0);
  TEST_ASSERT_TRUE(help.find("NET:RESET") != std::string::npos);
  TEST_ASSERT_TRUE(help.find("NET:STATUS") != std::string::npos);
  TEST_ASSERT_TRUE(help.find("NET:SET,\"<ssid>\",\"<pass>\"") != std::string::npos);
  TEST_ASSERT_TRUE(help.find("NET:LIST") != std::string::npos);
}

void test_net_status_in_ap_mode() {
  std::string r = proto.processLine("NET:STATUS", 0);
  TEST_ASSERT_TRUE(r.rfind("CTRL:ACK msg_id=", 0) == 0);
  TEST_ASSERT_TRUE(r.find("state=AP_ACTIVE") != std::string::npos);
  TEST_ASSERT_TRUE(r.find(" rssi=NA") != std::string::npos);
  TEST_ASSERT_TRUE(r.find(" ip=") != std::string::npos);
  TEST_ASSERT_TRUE(r.find(" ssid=") != std::string::npos);
}

void test_net_set_returns_status_after_wait() {
  std::string resp = proto.processLine("NET:SET,\"ssid,with,commas\",\"pa ssword\"", 0);
  TEST_ASSERT_TRUE(resp.rfind("CTRL:ACK msg_id=", 0) == 0);
}

void test_net_set_validation_bad_param() {
  // Empty SSID
  std::string r1 = proto.processLine("NET:SET,\"\",\"password\"", 0);
  TEST_ASSERT_TRUE(r1.rfind("CTRL:ERR ", 0) == 0);
  TEST_ASSERT_TRUE(r1.find(" NET_BAD_PARAM") != std::string::npos);
  // Short password (<8)
  std::string r2 = proto.processLine("NET:SET,myssid,short", 0);
  TEST_ASSERT_TRUE(r2.rfind("CTRL:ERR ", 0) == 0);
  TEST_ASSERT_TRUE(r2.find(" NET_BAD_PARAM") != std::string::npos);
}

void test_net_reset_from_connected_back_to_ap() {
#if defined(USE_STUB_BACKEND)
  Net().setTestSimulation(true, 0);
#endif
  (void)proto.processLine("NET:SET,ssid,password", 0);
  pump_for(5);
  // Connected now; reset should return OK (status line emitted asynchronously)
  std::string rr = proto.processLine("NET:RESET", 0);
  TEST_ASSERT_TRUE(rr.rfind("CTRL:ACK", 0) == 0);
  TEST_ASSERT_TRUE(rr.find(" msg_id=") != std::string::npos);
}

void test_net_list_returns_networks() {
  // In native stub, scan returns canned results
  std::string r = proto.processLine("NET:LIST", 0);
  TEST_ASSERT_TRUE(r.rfind("CTRL:ACK msg_id=", 0) == 0);
  TEST_ASSERT_TRUE(r.find(" scanning=1") != std::string::npos);
  // ACK first; find the NET:LIST header line with msg_id
  auto pos = r.find("\nNET:LIST");
  TEST_ASSERT_TRUE(pos != std::string::npos);
  TEST_ASSERT_TRUE(r.find("NET:LIST msg_id=") != std::string::npos);
  std::string items = r.substr(pos);
  TEST_ASSERT_TRUE(items.find("SSID=") != std::string::npos);
  TEST_ASSERT_TRUE(items.find(" rssi=") != std::string::npos);
  // Result lines should not repeat msg_id tokens
  TEST_ASSERT_TRUE(items.find("\nmsg_id=") == std::string::npos);
}

void test_net_list_requires_ap_mode() {
#if defined(USE_STUB_BACKEND)
  Net().setTestSimulation(true, 0);
#endif
  (void)proto.processLine("NET:SET,ssid,password", 0);
  pump_for(5);
  std::string r = proto.processLine("NET:LIST", 0);
  TEST_ASSERT_TRUE(r.rfind("CTRL:ERR msg_id=", 0) == 0);
  TEST_ASSERT_TRUE(r.find("NET_SCAN_AP_ONLY") != std::string::npos);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_help_includes_net_verbs);
  RUN_TEST(test_net_status_in_ap_mode);
  RUN_TEST(test_net_set_returns_status_after_wait);
  RUN_TEST(test_net_set_validation_bad_param);
  RUN_TEST(test_net_reset_from_connected_back_to_ap);
  RUN_TEST(test_net_list_returns_networks);
  RUN_TEST(test_net_list_requires_ap_mode);
  return UNITY_END();
}
