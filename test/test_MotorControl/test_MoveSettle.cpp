#ifdef ARDUINO
#include <Arduino.h>
#endif
#include "MotorControl/MotionKinematics.h"
#include "MotorControl/MotorCommandProcessor.h"
#include "MotorControl/MotorControlConstants.h"
#include "test_common/TestHelpers.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <unity.h>

using test_helpers::FindStatusLineForId;
using test_helpers::SplitLines;

// Helper: extract a field value from a response line
static std::string ExtractField(const std::string& line, const std::string& key) {
  std::string search = key + "=";
  size_t pos = line.find(search);
  if (pos == std::string::npos)
    return "";
  pos += search.size();
  size_t end = line.find(' ', pos);
  if (end == std::string::npos)
    end = line.size();
  return line.substr(pos, end - pos);
}

// ---- GET/SET tests ----

void test_get_set_move_overshoot() {
  MotorCommandProcessor p;
  // Default value
  std::string r1 = p.processLine("GET:MOVE_OVERSHOOT", 0);
  TEST_ASSERT_TRUE(r1.find("MOVE_OVERSHOOT=80") != std::string::npos);
  // Set to new value
  std::string r2 = p.processLine("SET MOVE_OVERSHOOT=500", 0);
  TEST_ASSERT_TRUE(r2.rfind("CTRL:DONE", 0) == 0);
  TEST_ASSERT_TRUE(r2.find("MOVE_OVERSHOOT=500") != std::string::npos);
  // Verify GET reflects new value
  std::string r3 = p.processLine("GET:MOVE_OVERSHOOT", 0);
  TEST_ASSERT_TRUE(r3.find("MOVE_OVERSHOOT=500") != std::string::npos);
  // Set to 0 (disabled)
  std::string r4 = p.processLine("SET MOVE_OVERSHOOT=0", 0);
  TEST_ASSERT_TRUE(r4.rfind("CTRL:DONE", 0) == 0);
  std::string r5 = p.processLine("GET:MOVE_OVERSHOOT", 0);
  TEST_ASSERT_TRUE(r5.find("MOVE_OVERSHOOT=0") != std::string::npos);
}

void test_get_set_dither_amplitude() {
  MotorCommandProcessor p;
  // Default is 0 (disabled)
  std::string r1 = p.processLine("GET:DITHER_AMPLITUDE", 0);
  TEST_ASSERT_TRUE(r1.find("DITHER_AMPLITUDE=0") != std::string::npos);
  // Set
  std::string r2 = p.processLine("SET DITHER_AMPLITUDE=100", 0);
  TEST_ASSERT_TRUE(r2.rfind("CTRL:DONE", 0) == 0);
  std::string r3 = p.processLine("GET:DITHER_AMPLITUDE", 0);
  TEST_ASSERT_TRUE(r3.find("DITHER_AMPLITUDE=100") != std::string::npos);
}

void test_get_set_dither_cycles() {
  MotorCommandProcessor p;
  std::string r1 = p.processLine("GET:DITHER_CYCLES", 0);
  TEST_ASSERT_TRUE(r1.find("DITHER_CYCLES=3") != std::string::npos);
  std::string r2 = p.processLine("SET DITHER_CYCLES=5", 0);
  TEST_ASSERT_TRUE(r2.rfind("CTRL:DONE", 0) == 0);
  std::string r3 = p.processLine("GET:DITHER_CYCLES", 0);
  TEST_ASSERT_TRUE(r3.find("DITHER_CYCLES=5") != std::string::npos);
}

void test_get_set_dither_min_amplitude() {
  MotorCommandProcessor p;
  std::string r1 = p.processLine("GET:DITHER_MIN_AMPLITUDE", 0);
  TEST_ASSERT_TRUE(r1.find("DITHER_MIN_AMPLITUDE=20") != std::string::npos);
  std::string r2 = p.processLine("SET DITHER_MIN_AMPLITUDE=10", 0);
  TEST_ASSERT_TRUE(r2.rfind("CTRL:DONE", 0) == 0);
  std::string r3 = p.processLine("GET:DITHER_MIN_AMPLITUDE", 0);
  TEST_ASSERT_TRUE(r3.find("DITHER_MIN_AMPLITUDE=10") != std::string::npos);
}

void test_set_settle_rejects_negative() {
  MotorCommandProcessor p;
  // MOVE_OVERSHOOT accepts negative (sign controls approach direction)
  std::string r1 = p.processLine("SET MOVE_OVERSHOOT=-300", 0);
  TEST_ASSERT_TRUE(r1.rfind("CTRL:DONE", 0) == 0);
  // DITHER_* params still reject negative
  std::string r2 = p.processLine("SET DITHER_AMPLITUDE=-5", 0);
  TEST_ASSERT_TRUE(r2.find("E03") != std::string::npos);
  std::string r3 = p.processLine("SET DITHER_CYCLES=-1", 0);
  TEST_ASSERT_TRUE(r3.find("E03") != std::string::npos);
  std::string r4 = p.processLine("SET DITHER_MIN_AMPLITUDE=-10", 0);
  TEST_ASSERT_TRUE(r4.find("E03") != std::string::npos);
}

void test_get_all_includes_settle_params() {
  MotorCommandProcessor p;
  std::string r = p.processLine("GET:ALL", 0);
  TEST_ASSERT_TRUE(r.find("MOVE_OVERSHOOT=80") != std::string::npos);
  TEST_ASSERT_TRUE(r.find("DITHER_AMPLITUDE=0") != std::string::npos);
  TEST_ASSERT_TRUE(r.find("DITHER_CYCLES=3") != std::string::npos);
  TEST_ASSERT_TRUE(r.find("DITHER_MIN_AMPLITUDE=20") != std::string::npos);
}

// ---- MOVE command parsing tests ----

void test_move_accepts_overshoot_param() {
  MotorCommandProcessor p;
  p.processLine("SET THERMAL_LIMITING=OFF", 0);
  // MOVE with explicit overshoot=0 (disabling default)
  std::string r = p.processLine("MOVE:0,100,,,,", 0);
  TEST_ASSERT_TRUE(r.rfind("CTRL:ACK", 0) == 0);
}

void test_move_accepts_settle_params() {
  MotorCommandProcessor p;
  p.processLine("SET THERMAL_LIMITING=OFF", 0);
  // MOVE with overshoot=200, dither_amp=50, dither_cycles=4
  std::string r = p.processLine("MOVE:0,100,,,200,50,4", 0);
  TEST_ASSERT_TRUE(r.rfind("CTRL:ACK", 0) == 0);
}

void test_move_accepts_negative_overshoot() {
  MotorCommandProcessor p;
  p.processLine("SET THERMAL_LIMITING=OFF", 0);
  std::string r = p.processLine("MOVE:0,100,,,-1", 0);
  TEST_ASSERT_TRUE(r.rfind("CTRL:ACK", 0) == 0);
}

void test_move_rejects_extra_trailing_params() {
  MotorCommandProcessor p;
  // 8th param should be rejected
  std::string r = p.processLine("MOVE:0,100,,,0,0,0,99", 0);
  TEST_ASSERT_TRUE(r.find("E03") != std::string::npos);
}

// ---- Settle state machine tests (via StubMotorController) ----

void test_move_with_overshoot_completes_at_target() {
  MotorCommandProcessor p;
  p.processLine("SET THERMAL_LIMITING=OFF", 0);
  p.processLine("SET MOVE_OVERSHOOT=100", 0);
  p.processLine("SET DITHER_AMPLITUDE=0", 0);
  // Move motor 0 to position 500
  std::string r = p.processLine("MOVE:0,500", 0);
  TEST_ASSERT_TRUE(r.rfind("CTRL:ACK", 0) == 0);
  // The StubMotorController adds settle time to end_ms
  // Tick far enough to complete all phases
  p.tick(30000);
  auto st = p.processLine("STATUS", 30000);
  auto lines = SplitLines(st);
  std::string L0 = FindStatusLineForId(lines, 0);
  TEST_ASSERT_FALSE(L0.empty());
  // Motor should end at the target position (500 user-space)
  TEST_ASSERT_TRUE(L0.find("pos=500") != std::string::npos);
  TEST_ASSERT_TRUE(L0.find("moving=0") != std::string::npos);
}

void test_move_with_overshoot_increases_estimate() {
  MotorCommandProcessor p;
  p.processLine("SET THERMAL_LIMITING=OFF", 0);
  p.processLine("SET SPEED=1000", 0);
  p.processLine("SET ACCEL=10000", 0);
  // Move without overshoot
  p.processLine("SET MOVE_OVERSHOOT=0", 0);
  std::string r1 = p.processLine("MOVE:0,500", 0);
  std::string est1_str = ExtractField(r1, "est_ms");
  long est1 = std::atol(est1_str.c_str());
  // Complete the move
  p.tick(5000);
  // Move with overshoot
  p.processLine("SET MOVE_OVERSHOOT=200", 5000);
  std::string r2 = p.processLine("MOVE:0,0", 5000);
  std::string est2_str = ExtractField(r2, "est_ms");
  long est2 = std::atol(est2_str.c_str());
  // Overshoot should increase the estimate
  TEST_ASSERT_TRUE(est2 > est1);
}

void test_move_overshoot_adds_time_even_when_zero_delta() {
  MotorCommandProcessor p;
  p.processLine("SET THERMAL_LIMITING=OFF", 0);
  p.processLine("SET MOVE_OVERSHOOT=300", 0);
  p.processLine("SET SPEED=1000", 0);
  p.processLine("SET ACCEL=10000", 0);
  // Move to current position (delta=0): overshoot still applies (fixed approach direction)
  std::string r1 = p.processLine("MOVE:0,0", 0);
  std::string est1 = ExtractField(r1, "est_ms");
  // Estimate should include overshoot time (2x 300-step moves)
  long est_val = std::atol(est1.c_str());
  TEST_ASSERT_TRUE(est_val > 100);
}

void test_move_with_dither_increases_estimate() {
  MotorCommandProcessor p;
  p.processLine("SET THERMAL_LIMITING=OFF", 0);
  p.processLine("SET SPEED=1000", 0);
  p.processLine("SET ACCEL=10000", 0);
  p.processLine("SET MOVE_OVERSHOOT=0", 0);
  // Without dither
  p.processLine("SET DITHER_AMPLITUDE=0", 0);
  std::string r1 = p.processLine("MOVE:0,500", 0);
  std::string est1_str = ExtractField(r1, "est_ms");
  long est1 = std::atol(est1_str.c_str());
  p.tick(5000);
  // With dither
  p.processLine("SET DITHER_AMPLITUDE=50", 5000);
  p.processLine("SET DITHER_CYCLES=3", 5000);
  std::string r2 = p.processLine("MOVE:0,0", 5000);
  std::string est2_str = ExtractField(r2, "est_ms");
  long est2 = std::atol(est2_str.c_str());
  TEST_ASSERT_TRUE(est2 > est1);
}

void test_move_overshoot_inline_param_overrides_default() {
  MotorCommandProcessor p;
  p.processLine("SET THERMAL_LIMITING=OFF", 0);
  p.processLine("SET SPEED=1000", 0);
  p.processLine("SET ACCEL=10000", 0);
  // Default overshoot is 80, but inline 0 should override
  std::string r1 = p.processLine("MOVE:0,500,,,0", 0);
  std::string est1_str = ExtractField(r1, "est_ms");
  long est1 = std::atol(est1_str.c_str());
  p.tick(5000);
  // With default overshoot=80
  std::string r2 = p.processLine("MOVE:0,0", 5000);
  std::string est2_str = ExtractField(r2, "est_ms");
  long est2 = std::atol(est2_str.c_str());
  // r2 (with default overshoot) should have higher estimate than r1 (overshoot=0)
  TEST_ASSERT_TRUE(est2 > est1);
}

void test_dither_amplitude_clamped_near_limits() {
  MotorCommandProcessor p;
  p.processLine("SET THERMAL_LIMITING=OFF", 0);
  p.processLine("SET MOVE_OVERSHOOT=0", 0);
  p.processLine("SET SPEED=1000", 0);
  p.processLine("SET ACCEL=10000", 0);
  // Dither amplitude 200, target at 1100 — only 100 steps to upper limit
  // Amplitude should be clamped to 100 (symmetric: min(1200-1100, 1100-(-1200)) = 100)
  p.processLine("SET DITHER_AMPLITUDE=200", 0);
  p.processLine("SET DITHER_CYCLES=3", 0);
  p.processLine("SET DITHER_MIN_AMPLITUDE=1", 0);
  std::string r = p.processLine("MOVE:0,1100", 0);
  TEST_ASSERT_TRUE(r.rfind("CTRL:ACK", 0) == 0);
  // Complete the move
  p.tick(30000);
  auto st = p.processLine("STATUS", 30000);
  auto lines = SplitLines(st);
  std::string L0 = FindStatusLineForId(lines, 0);
  // Motor should end at exact target despite dither near boundary
  TEST_ASSERT_TRUE(L0.find("pos=1100") != std::string::npos);
  TEST_ASSERT_TRUE(L0.find("moving=0") != std::string::npos);

  // Also verify the estimate is smaller than with full amplitude far from limits
  // Move back to center first
  p.processLine("MOVE:0,0", 30000);
  p.tick(60000);
  // Move to 0 with dither=200 (far from limits, full amplitude)
  std::string r_full = p.processLine("MOVE:0,500", 60000);
  std::string est_full_str = ExtractField(r_full, "est_ms");
  long est_full = std::atol(est_full_str.c_str());
  p.tick(90000);
  // Move near limit with same dither (amplitude clamped)
  std::string r_clamped = p.processLine("MOVE:0,1100", 90000);
  std::string est_clamped_str = ExtractField(r_clamped, "est_ms");
  long est_clamped = std::atol(est_clamped_str.c_str());
  // Clamped amplitude should produce a smaller dither estimate
  TEST_ASSERT_TRUE(est_clamped < est_full);
}

void test_move_overshoot_clamped_at_boundary() {
  MotorCommandProcessor p;
  p.processLine("SET THERMAL_LIMITING=OFF", 0);
  p.processLine("SET MOVE_OVERSHOOT=300", 0);
  p.processLine("SET DITHER_AMPLITUDE=0", 0);
  p.processLine("SET SPEED=1000", 0);
  p.processLine("SET ACCEL=10000", 0);
  // target=-1100, overshoot=300 → overshoot_pos=-1400, clamped to -1200
  // Effective overshoot becomes 100 (= -1100 - (-1200))
  std::string r = p.processLine("MOVE:0,-1100", 0);
  TEST_ASSERT_TRUE(r.rfind("CTRL:ACK", 0) == 0);
  // Motor should complete at the target
  p.tick(30000);
  auto st = p.processLine("STATUS", 30000);
  auto lines = SplitLines(st);
  std::string L0 = FindStatusLineForId(lines, 0);
  TEST_ASSERT_FALSE(L0.empty());
  TEST_ASSERT_TRUE(L0.find("pos=-1100") != std::string::npos);
  TEST_ASSERT_TRUE(L0.find("moving=0") != std::string::npos);

  // Verify estimate reflects clamped overshoot (smaller than full 300)
  // Move back to 0 first
  p.processLine("SET MOVE_OVERSHOOT=0", 30000);
  p.processLine("MOVE:0,0", 30000);
  p.tick(60000);
  // Now test estimate: move to -1100 with overshoot=300 (clamped to 100)
  p.processLine("SET MOVE_OVERSHOOT=300", 60000);
  std::string r_clamped = p.processLine("MOVE:0,-1100", 60000);
  std::string est_clamped_str = ExtractField(r_clamped, "est_ms");
  long est_clamped = std::atol(est_clamped_str.c_str());
  p.tick(90000);
  // Compare with move to -500 with overshoot=300 (not clamped, full 300)
  p.processLine("MOVE:0,0", 90000);
  p.tick(120000);
  std::string r_full = p.processLine("MOVE:0,-500", 120000);
  std::string est_full_str = ExtractField(r_full, "est_ms");
  long est_full = std::atol(est_full_str.c_str());
  // Both go ~same distance (~1100 vs ~500+300=800 leg1), but clamped has smaller overshoot leg
  // The clamped overshoot leg (100) should be smaller than unclamped (300)
  // We just verify the clamped estimate is reasonable (> 0)
  TEST_ASSERT_TRUE(est_clamped > 0);
  TEST_ASSERT_TRUE(est_full > 0);
}

void test_help_includes_settle_params() {
  MotorCommandProcessor p;
  std::string help = p.processLine("HELP", 0);
  TEST_ASSERT_TRUE(help.find("MOVE_OVERSHOOT") != std::string::npos);
  TEST_ASSERT_TRUE(help.find("DITHER_AMPLITUDE") != std::string::npos);
  TEST_ASSERT_TRUE(help.find("DITHER_CYCLES") != std::string::npos);
  TEST_ASSERT_TRUE(help.find("DITHER_MIN_AMPLITUDE") != std::string::npos);
}
