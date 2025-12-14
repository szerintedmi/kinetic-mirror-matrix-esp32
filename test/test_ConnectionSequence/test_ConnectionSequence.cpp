#include "net_onboarding/ConnectionSequence.h"

#include <unity.h>

using namespace net_onboarding;

void setUp() {}
void tearDown() {}

// Helper to create valid credentials
static WifiCredentials makeCreds(const char* ssid, const char* pass = "password") {
  WifiCredentials c;
  c.ssid = ssid;
  c.password = pass;
  return c;
}

// --- begin() tests ---

void test_begin_with_primary_only_starts_trying_primary() {
  ConnectionSequence seq(5000);
  seq.begin(makeCreds("primary"), WifiCredentials{}, 0);

  TEST_ASSERT_EQUAL((int)ConnectionSequence::State::TRYING_PRIMARY, (int)seq.state());
  TEST_ASSERT_EQUAL((int)CredentialSlot::PRIMARY, (int)seq.currentSlot());
  TEST_ASSERT_NOT_NULL(seq.currentCredentials());
  TEST_ASSERT_EQUAL_STRING("primary", seq.currentCredentials()->ssid.c_str());
}

void test_begin_with_secondary_only_starts_trying_secondary() {
  ConnectionSequence seq(5000);
  seq.begin(WifiCredentials{}, makeCreds("secondary"), 0);

  TEST_ASSERT_EQUAL((int)ConnectionSequence::State::TRYING_SECONDARY, (int)seq.state());
  TEST_ASSERT_EQUAL((int)CredentialSlot::SECONDARY, (int)seq.currentSlot());
  TEST_ASSERT_NOT_NULL(seq.currentCredentials());
  TEST_ASSERT_EQUAL_STRING("secondary", seq.currentCredentials()->ssid.c_str());
}

void test_begin_with_both_starts_with_primary() {
  ConnectionSequence seq(5000);
  seq.begin(makeCreds("primary"), makeCreds("secondary"), 0);

  TEST_ASSERT_EQUAL((int)ConnectionSequence::State::TRYING_PRIMARY, (int)seq.state());
  TEST_ASSERT_EQUAL_STRING("primary", seq.currentCredentials()->ssid.c_str());
}

void test_begin_with_neither_goes_exhausted() {
  ConnectionSequence seq(5000);
  seq.begin(WifiCredentials{}, WifiCredentials{}, 0);

  TEST_ASSERT_EQUAL((int)ConnectionSequence::State::EXHAUSTED, (int)seq.state());
  TEST_ASSERT_NULL(seq.currentCredentials());
}

void test_begin_with_last_slot_secondary_starts_with_secondary() {
  ConnectionSequence seq(5000);
  // Set last connected slot to SECONDARY before begin
  seq.setLastConnectedSlot(CredentialSlot::SECONDARY);

  // Now begin - should start with SECONDARY since it's the last connected
  seq.begin(makeCreds("primary"), makeCreds("secondary"), 0);
  TEST_ASSERT_EQUAL((int)ConnectionSequence::State::TRYING_SECONDARY, (int)seq.state());
  TEST_ASSERT_EQUAL_STRING("secondary", seq.currentCredentials()->ssid.c_str());
}

void test_begin_with_last_slot_secondary_but_invalid_falls_back_to_primary() {
  ConnectionSequence seq(5000);
  // Set last connected slot to SECONDARY but don't provide valid secondary creds
  seq.setLastConnectedSlot(CredentialSlot::SECONDARY);

  seq.begin(makeCreds("primary"), WifiCredentials{}, 0);
  // Should fall back to PRIMARY since SECONDARY has no valid credentials
  TEST_ASSERT_EQUAL((int)ConnectionSequence::State::TRYING_PRIMARY, (int)seq.state());
  TEST_ASSERT_EQUAL_STRING("primary", seq.currentCredentials()->ssid.c_str());
}

// --- checkTimeout() tests ---

void test_checkTimeout_no_change_before_timeout() {
  ConnectionSequence seq(5000);
  seq.begin(makeCreds("primary"), makeCreds("secondary"), 0);

  TEST_ASSERT_FALSE(seq.checkTimeout(4999));
  TEST_ASSERT_EQUAL((int)ConnectionSequence::State::TRYING_PRIMARY, (int)seq.state());
}

void test_checkTimeout_advances_to_secondary_after_timeout() {
  ConnectionSequence seq(5000);
  seq.begin(makeCreds("primary"), makeCreds("secondary"), 0);

  TEST_ASSERT_TRUE(seq.checkTimeout(5000));
  TEST_ASSERT_EQUAL((int)ConnectionSequence::State::TRYING_SECONDARY, (int)seq.state());
  TEST_ASSERT_EQUAL_STRING("secondary", seq.currentCredentials()->ssid.c_str());
}

void test_checkTimeout_exhausted_after_both_timeout() {
  ConnectionSequence seq(5000);
  seq.begin(makeCreds("primary"), makeCreds("secondary"), 0);

  seq.checkTimeout(5000);  // -> TRYING_SECONDARY
  TEST_ASSERT_TRUE(seq.checkTimeout(10000));  // -> EXHAUSTED
  TEST_ASSERT_EQUAL((int)ConnectionSequence::State::EXHAUSTED, (int)seq.state());
  TEST_ASSERT_NULL(seq.currentCredentials());
}

void test_checkTimeout_primary_only_goes_exhausted() {
  ConnectionSequence seq(5000);
  seq.begin(makeCreds("primary"), WifiCredentials{}, 0);

  TEST_ASSERT_TRUE(seq.checkTimeout(5000));
  TEST_ASSERT_EQUAL((int)ConnectionSequence::State::EXHAUSTED, (int)seq.state());
}

void test_checkTimeout_handles_time_wraparound() {
  ConnectionSequence seq(5000);
  // Start near max uint32_t
  uint32_t start = 0xFFFFFFF0;
  seq.begin(makeCreds("primary"), makeCreds("secondary"), start);

  // Time wraps around - need 5000ms elapsed
  // 0xFFFFFFF0 + 5000 = 0x00001378 (after wraparound)
  uint32_t after_wrap = 0x00001380;  // Just past 5000ms elapsed
  TEST_ASSERT_TRUE(seq.checkTimeout(after_wrap));
  TEST_ASSERT_EQUAL((int)ConnectionSequence::State::TRYING_SECONDARY, (int)seq.state());
}

// --- markConnected() tests ---

void test_markConnected_sets_idle_and_last_slot() {
  ConnectionSequence seq(5000);
  seq.begin(makeCreds("primary"), makeCreds("secondary"), 0);

  seq.markConnected(CredentialSlot::PRIMARY);

  TEST_ASSERT_EQUAL((int)ConnectionSequence::State::IDLE, (int)seq.state());
  TEST_ASSERT_EQUAL((int)CredentialSlot::PRIMARY, (int)seq.lastConnectedSlot());
}

void test_markConnected_secondary_remembers_slot() {
  ConnectionSequence seq(5000);
  seq.begin(makeCreds("primary"), makeCreds("secondary"), 0);
  seq.checkTimeout(5000);  // -> TRYING_SECONDARY

  seq.markConnected(CredentialSlot::SECONDARY);

  TEST_ASSERT_EQUAL((int)CredentialSlot::SECONDARY, (int)seq.lastConnectedSlot());
}

// --- onDisconnect() tests ---

void test_onDisconnect_retries_last_connected_first() {
  ConnectionSequence seq(5000);
  seq.begin(makeCreds("primary"), makeCreds("secondary"), 0);
  seq.markConnected(CredentialSlot::PRIMARY);

  seq.onDisconnect(1000);

  TEST_ASSERT_EQUAL((int)ConnectionSequence::State::TRYING_PRIMARY, (int)seq.state());
  TEST_ASSERT_EQUAL_STRING("primary", seq.currentCredentials()->ssid.c_str());
}

void test_onDisconnect_retries_secondary_first_if_last_connected() {
  ConnectionSequence seq(5000);
  seq.begin(makeCreds("primary"), makeCreds("secondary"), 0);
  seq.markConnected(CredentialSlot::SECONDARY);

  seq.onDisconnect(1000);

  TEST_ASSERT_EQUAL((int)ConnectionSequence::State::TRYING_SECONDARY, (int)seq.state());
  TEST_ASSERT_EQUAL_STRING("secondary", seq.currentCredentials()->ssid.c_str());
}

void test_onDisconnect_tries_other_after_timeout() {
  ConnectionSequence seq(5000);
  seq.begin(makeCreds("primary"), makeCreds("secondary"), 0);
  seq.markConnected(CredentialSlot::PRIMARY);

  seq.onDisconnect(1000);
  seq.checkTimeout(6000);  // primary times out

  TEST_ASSERT_EQUAL((int)ConnectionSequence::State::TRYING_SECONDARY, (int)seq.state());
}

void test_onDisconnect_exhausted_after_both_fail() {
  ConnectionSequence seq(5000);
  seq.begin(makeCreds("primary"), makeCreds("secondary"), 0);
  seq.markConnected(CredentialSlot::PRIMARY);

  seq.onDisconnect(1000);
  seq.checkTimeout(6000);   // primary times out -> secondary
  seq.checkTimeout(11000);  // secondary times out -> exhausted

  TEST_ASSERT_EQUAL((int)ConnectionSequence::State::EXHAUSTED, (int)seq.state());
}

// --- updateSecondary() tests ---

void test_updateSecondary_updates_credentials() {
  ConnectionSequence seq(5000);
  seq.begin(makeCreds("primary"), WifiCredentials{}, 0);
  seq.markConnected(CredentialSlot::PRIMARY);

  // Add secondary while connected
  seq.updateSecondary(makeCreds("new_secondary"));

  // On disconnect, secondary should now be available
  seq.onDisconnect(1000);
  seq.checkTimeout(6000);  // primary times out

  TEST_ASSERT_EQUAL((int)ConnectionSequence::State::TRYING_SECONDARY, (int)seq.state());
  TEST_ASSERT_EQUAL_STRING("new_secondary", seq.currentCredentials()->ssid.c_str());
}

// --- reset() tests ---

void test_reset_clears_state() {
  ConnectionSequence seq(5000);
  seq.begin(makeCreds("primary"), makeCreds("secondary"), 0);

  seq.reset();

  TEST_ASSERT_EQUAL((int)ConnectionSequence::State::IDLE, (int)seq.state());
  TEST_ASSERT_NULL(seq.currentCredentials());
}

int main(int, char**) {
  UNITY_BEGIN();

  // begin() tests
  RUN_TEST(test_begin_with_primary_only_starts_trying_primary);
  RUN_TEST(test_begin_with_secondary_only_starts_trying_secondary);
  RUN_TEST(test_begin_with_both_starts_with_primary);
  RUN_TEST(test_begin_with_neither_goes_exhausted);
  RUN_TEST(test_begin_with_last_slot_secondary_starts_with_secondary);
  RUN_TEST(test_begin_with_last_slot_secondary_but_invalid_falls_back_to_primary);

  // checkTimeout() tests
  RUN_TEST(test_checkTimeout_no_change_before_timeout);
  RUN_TEST(test_checkTimeout_advances_to_secondary_after_timeout);
  RUN_TEST(test_checkTimeout_exhausted_after_both_timeout);
  RUN_TEST(test_checkTimeout_primary_only_goes_exhausted);
  RUN_TEST(test_checkTimeout_handles_time_wraparound);

  // markConnected() tests
  RUN_TEST(test_markConnected_sets_idle_and_last_slot);
  RUN_TEST(test_markConnected_secondary_remembers_slot);

  // onDisconnect() tests
  RUN_TEST(test_onDisconnect_retries_last_connected_first);
  RUN_TEST(test_onDisconnect_retries_secondary_first_if_last_connected);
  RUN_TEST(test_onDisconnect_tries_other_after_timeout);
  RUN_TEST(test_onDisconnect_exhausted_after_both_fail);

  // updateSecondary() tests
  RUN_TEST(test_updateSecondary_updates_credentials);

  // reset() tests
  RUN_TEST(test_reset_clears_state);

  return UNITY_END();
}
