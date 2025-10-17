#include <unity.h>
#include <vector>
#include <string>

#include "MotorControl/HardwareMotorController.h"

// Test doubles and event log
struct Event {
  std::string type; // "LATCH" or "START"
  int id;           // motor id for START, -1 for LATCH
};

static std::vector<Event> g_events;

class LoggingShift595 : public IShift595 {
public:
  void begin() override { last_dir_ = 0; last_sleep_ = 0; latch_count_ = 0; }
  void setDirSleep(uint8_t dir_bits, uint8_t sleep_bits) override {
    last_dir_ = dir_bits;
    last_sleep_ = sleep_bits;
    latch_count_++;
    g_events.push_back({"LATCH", -1});
  }
  void resetCounters() { latch_count_ = 0; }
  uint8_t last_dir() const { return last_dir_; }
  uint8_t last_sleep() const { return last_sleep_; }
  unsigned latch_count() const { return latch_count_; }
private:
  uint8_t last_dir_ = 0;
  uint8_t last_sleep_ = 0;
  unsigned latch_count_ = 0;
};

class FasAdapterStub : public IFasAdapter {
public:
  struct StartCall { uint8_t id; long target; int speed; int accel; };
  void begin() override {}
  void configureStepPin(uint8_t /*id*/, int /*gpio*/) override {}
  bool startMoveAbs(uint8_t id, long target, int speed, int accel) override {
    if (id >= 8) return false;
    moving_[id] = true;
    starts_.push_back({id, target, speed, accel});
    g_events.push_back({"START", (int)id});
    // Do not update position automatically; tests control it via setCurrentPosition
    targets_[id] = target;
    return true;
  }
  bool isMoving(uint8_t id) const override { return (id < 8) ? moving_[id] : false; }
  long currentPosition(uint8_t id) const override { return (id < 8) ? position_[id] : 0; }
  void setCurrentPosition(uint8_t id, long pos) override {
    if (id < 8) { position_[id] = pos; if (position_[id] == targets_[id]) moving_[id] = false; }
  }
  void setMoving(uint8_t id, bool m) { if (id < 8) moving_[id] = m; }
  const std::vector<StartCall>& starts() const { return starts_; }
private:
  bool moving_[8] = {false,false,false,false,false,false,false,false};
  long position_[8] = {0,0,0,0,0,0,0,0};
  long targets_[8] = {0,0,0,0,0,0,0,0};
  std::vector<StartCall> starts_;
};

static void clear_events() { g_events.clear(); }

void test_backend_latch_before_start() {
  LoggingShift595 shift;
  FasAdapterStub fas;
  HardwareMotorController ctrl(shift, fas, 8);
  // Controller constructor latches once; ignore by clearing
  shift.resetCounters();
  clear_events();

  // Move motors 0 and 1 to +100
  uint32_t mask = (1u << 0) | (1u << 1);
  bool ok = ctrl.moveAbsMask(mask, 100, 4000, 16000, 0);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_GREATER_OR_EQUAL(1u, (unsigned)shift.latch_count());
  TEST_ASSERT_TRUE(g_events.size() >= 3);
  TEST_ASSERT_EQUAL_STRING("LATCH", g_events[0].type.c_str());
  // The two START events must come after the first LATCH
  bool found_start_after = (g_events[1].type == "START" || g_events[2].type == "START");
  TEST_ASSERT_TRUE(found_start_after);
}

void test_backend_dir_bits_per_target() {
  LoggingShift595 shift;
  FasAdapterStub fas;
  HardwareMotorController ctrl(shift, fas, 8);
  shift.resetCounters();
  clear_events();

  // Motor 0 -> +100 (DIR=1), Motor 1 -> -100 (DIR=0)
  ctrl.moveAbsMask((1u << 0) | (1u << 1), 100, 4000, 16000, 0);
  // Mark motor 1 move complete at target to clear BUSY, then advance position
  fas.setCurrentPosition(1, 100);
  ctrl.tick(0);
  // Update motor 1 position to +200 then command -100 absolute -> negative delta
  fas.setCurrentPosition(1, 200);
  ctrl.tick(0);
  ctrl.moveAbsMask((1u << 1), -100, 4000, 16000, 0);

  uint8_t dir = shift.last_dir();
  TEST_ASSERT_TRUE((dir & (1u << 0)) != 0); // motor 0 forward
  TEST_ASSERT_TRUE((dir & (1u << 1)) == 0); // motor 1 reverse
}

void test_backend_wake_sleep_overrides() {
  LoggingShift595 shift;
  FasAdapterStub fas;
  HardwareMotorController ctrl(shift, fas, 8);
  shift.resetCounters();
  clear_events();

  // Force all asleep then WAKE motors 2,3
  ctrl.sleepMask(0xFF);
  unsigned latch_before = shift.latch_count();
  ctrl.wakeMask((1u << 2) | (1u << 3));
  TEST_ASSERT_EQUAL_UINT(latch_before + 1, shift.latch_count());
  uint8_t sleep = shift.last_sleep();
  TEST_ASSERT_TRUE((sleep & (1u << 2)) != 0);
  TEST_ASSERT_TRUE((sleep & (1u << 3)) != 0);

  // SLEEP while moving should fail and not change latch count
  fas.setMoving(2, true);
  bool ok = ctrl.sleepMask(1u << 2);
  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_EQUAL_UINT(latch_before + 1, shift.latch_count());
}

void test_backend_busy_rule_overlapping_move() {
  LoggingShift595 shift;
  FasAdapterStub fas;
  HardwareMotorController ctrl(shift, fas, 8);
  shift.resetCounters();
  clear_events();

  fas.setMoving(0, true);
  bool ok = ctrl.moveAbsMask(1u << 0, 500, 4000, 16000, 0);
  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_EQUAL_UINT(0, shift.latch_count());
}

void test_backend_dir_latched_once_per_move() {
  LoggingShift595 shift;
  FasAdapterStub fas;
  HardwareMotorController ctrl(shift, fas, 8);
  shift.resetCounters();
  clear_events();

  bool ok = ctrl.moveAbsMask(1u << 4, 250, 4000, 16000, 0);
  TEST_ASSERT_TRUE(ok);
  unsigned lc = shift.latch_count();
  // Tick should not cause additional latch
  ctrl.tick(1);
  TEST_ASSERT_EQUAL_UINT(lc, shift.latch_count());
}

void test_backend_speed_accel_passed_to_adapter() {
  LoggingShift595 shift;
  FasAdapterStub fas;
  HardwareMotorController ctrl(shift, fas, 8);
  shift.resetCounters();
  clear_events();

  bool ok = ctrl.moveAbsMask(1u << 6, 900, 5000, 12000, 0);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_FALSE(fas.starts().empty());
  auto s = fas.starts().back();
  TEST_ASSERT_EQUAL_UINT8(6, s.id);
  TEST_ASSERT_EQUAL(5000, s.speed);
  TEST_ASSERT_EQUAL(12000, s.accel);
}

// Note: No test runner/main here. The suite-level runner in
// test_MotorControl/test_Core.cpp will declare and invoke these tests.
