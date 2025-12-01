#include "MotorControl/Bitpack.h"
#include "MotorControl/HardwareMotorController.h"
#include "MotorControl/MotorControlConstants.h"

#include <string>
#include <unity.h>
#include <vector>

// Bitpack tests
void test_bitpack_dir_sleep_basic() {
  TEST_ASSERT_EQUAL_HEX8(0xA5, compute_dir_bits(0xA5));
  TEST_ASSERT_EQUAL_HEX8(0x0F, compute_sleep_bits(0x00, 0x0F, true));
  TEST_ASSERT_EQUAL_HEX8(0xF3, compute_sleep_bits(0xFF, 0x0C, false));
}

// Hardware backend tests
struct Event {
  std::string type;
  int id;
};
static std::vector<Event> g_events;

class LoggingShift595 : public IShift595 {
public:
  void begin() override {
    last_dir_ = 0;
    last_sleep_ = 0;
    latch_count_ = 0;
  }
  void setDirSleep(uint8_t dir_bits, uint8_t sleep_bits) override {
    last_dir_ = dir_bits;
    last_sleep_ = sleep_bits;
    latch_count_++;
    g_events.push_back({"LATCH", -1});
  }
  void resetCounters() {
    latch_count_ = 0;
  }
  uint8_t last_dir() const {
    return last_dir_;
  }
  uint8_t last_sleep() const {
    return last_sleep_;
  }
  unsigned latch_count() const {
    return latch_count_;
  }

private:
  uint8_t last_dir_ = 0;
  uint8_t last_sleep_ = 0;
  unsigned latch_count_ = 0;
};

class FasAdapterStub : public IFasAdapter {
public:
  struct StartCall {
    uint8_t id;
    long target;
    int speed;
    int accel;
  };
  void begin() override {}
  void configureStepPin(uint8_t, int) override {}
  bool startMoveAbs(uint8_t id, long target, int speed, int accel) override {
    if (id >= 8)
      return false;
    moving_[id] = true;
    starts_.push_back({id, target, speed, accel});
    g_events.push_back({"START", (int)id});
    targets_[id] = target;
    return true;
  }
  bool isMoving(uint8_t id) const override {
    return (id < 8) ? moving_[id] : false;
  }
  long currentPosition(uint8_t id) const override {
    return (id < 8) ? position_[id] : 0;
  }
  void setCurrentPosition(uint8_t id, long pos) override {
    if (id < 8) {
      position_[id] = pos;
      if (position_[id] == targets_[id])
        moving_[id] = false;
    }
  }
  void setMoving(uint8_t id, bool m) {
    if (id < 8)
      moving_[id] = m;
  }
  const std::vector<StartCall>& starts() const {
    return starts_;
  }
  void forceStop(uint8_t id) override {
    if (id < 8) {
      moving_[id] = false;
      force_stop_calls_.push_back(id);
      g_events.push_back({"FORCE_STOP", (int)id});
    }
  }
  void disableOutputs(uint8_t id) override {
    if (id < 8) {
      disable_outputs_calls_.push_back(id);
      g_events.push_back({"DISABLE_OUTPUTS", (int)id});
    }
  }
  const std::vector<uint8_t>& forceStopCalls() const {
    return force_stop_calls_;
  }
  const std::vector<uint8_t>& disableOutputsCalls() const {
    return disable_outputs_calls_;
  }
  void clearCalls() {
    force_stop_calls_.clear();
    disable_outputs_calls_.clear();
  }

private:
  bool moving_[8] = {false, false, false, false, false, false, false, false};
  long position_[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  long targets_[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  std::vector<StartCall> starts_;
  std::vector<uint8_t> force_stop_calls_;
  std::vector<uint8_t> disable_outputs_calls_;
};

static void clear_events() {
  g_events.clear();
}

void test_backend_latch_before_start() {
  LoggingShift595 shift;
  FasAdapterStub fas;
  HardwareMotorController ctrl(shift, fas, 8);
  shift.resetCounters();
  clear_events();
  uint32_t mask = (1u << 0) | (1u << 1);
  bool ok = ctrl.moveAbsMask(mask,
                             100,
                             MotorControlConstants::DEFAULT_SPEED_SPS,
                             MotorControlConstants::DEFAULT_ACCEL_SPS2,
                             0);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_GREATER_OR_EQUAL(1u, (unsigned)shift.latch_count());
  TEST_ASSERT_TRUE(g_events.size() >= 3);
  TEST_ASSERT_EQUAL_STRING("LATCH", g_events[0].type.c_str());
  bool found_start_after = (g_events[1].type == "START" || g_events[2].type == "START");
  TEST_ASSERT_TRUE(found_start_after);
}

void test_backend_dir_bits_per_target() {
  LoggingShift595 shift;
  FasAdapterStub fas;
  HardwareMotorController ctrl(shift, fas, 8);
  shift.resetCounters();
  clear_events();
  ctrl.moveAbsMask((1u << 0) | (1u << 1),
                   100,
                   MotorControlConstants::DEFAULT_SPEED_SPS,
                   MotorControlConstants::DEFAULT_ACCEL_SPS2,
                   0);
  fas.setCurrentPosition(1, 100);
  ctrl.tick(0);
  fas.setCurrentPosition(1, 200);
  ctrl.tick(0);
  ctrl.moveAbsMask((1u << 1),
                   -100,
                   MotorControlConstants::DEFAULT_SPEED_SPS,
                   MotorControlConstants::DEFAULT_ACCEL_SPS2,
                   0);
  uint8_t dir = shift.last_dir();
  TEST_ASSERT_TRUE((dir & (1u << 0)) != 0);
  TEST_ASSERT_TRUE((dir & (1u << 1)) == 0);
}

void test_backend_wake_sleep_overrides() {
  LoggingShift595 shift;
  FasAdapterStub fas;
  HardwareMotorController ctrl(shift, fas, 8);
  shift.resetCounters();
  clear_events();
  ctrl.sleepMask(0xFF);
  unsigned latch_before = shift.latch_count();
  ctrl.wakeMask((1u << 2) | (1u << 3));
  TEST_ASSERT_EQUAL_UINT(latch_before + 1, shift.latch_count());
  uint8_t sleep = shift.last_sleep();
  TEST_ASSERT_TRUE((sleep & (1u << 2)) != 0);
  TEST_ASSERT_TRUE((sleep & (1u << 3)) != 0);
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
  bool ok = ctrl.moveAbsMask(1u << 0,
                             500,
                             MotorControlConstants::DEFAULT_SPEED_SPS,
                             MotorControlConstants::DEFAULT_ACCEL_SPS2,
                             0);
  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_EQUAL_UINT(0, shift.latch_count());
}

void test_backend_dir_latched_once_per_move() {
  LoggingShift595 shift;
  FasAdapterStub fas;
  HardwareMotorController ctrl(shift, fas, 8);
  shift.resetCounters();
  clear_events();
  bool ok = ctrl.moveAbsMask(1u << 4,
                             250,
                             MotorControlConstants::DEFAULT_SPEED_SPS,
                             MotorControlConstants::DEFAULT_ACCEL_SPS2,
                             0);
  TEST_ASSERT_TRUE(ok);
  unsigned lc = shift.latch_count();
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

// Thermal safety: forceStop() should be called before disableOutputs() during thermal shutdown
// This prevents auto-enable from fighting with the disable when motor is actively moving
void test_backend_thermal_overrun_calls_forceStop_before_disable() {
  LoggingShift595 shift;
  FasAdapterStub fas;
  HardwareMotorController ctrl(shift, fas, 8);
  ctrl.setThermalLimitsEnabled(true);
  shift.resetCounters();
  clear_events();
  fas.clearCalls();

  // Start a move - motor will be "moving" and "awake"
  bool ok = ctrl.moveAbsMask(1u << 0,
                             1000,
                             MotorControlConstants::DEFAULT_SPEED_SPS,
                             MotorControlConstants::DEFAULT_ACCEL_SPS2,
                             0);
  TEST_ASSERT_TRUE(ok);

  // Keep motor moving (simulate long move by not completing it)
  fas.setMoving(0, true);

  // First tick to establish awake state (budget drains based on awake flag from previous tick)
  ctrl.tick(0);

  // Advance time past budget + grace period to trigger thermal shutdown
  // Budget is MAX_RUNNING_TIME_S (60s), grace is AUTO_SLEEP_IF_OVER_BUDGET_S (5s)
  const uint32_t t_ms = (MotorControlConstants::MAX_RUNNING_TIME_S +
                         MotorControlConstants::AUTO_SLEEP_IF_OVER_BUDGET_S + 1) *
                        1000;
  ctrl.tick(t_ms);

  // forceStop should have been called for motor 0
  TEST_ASSERT_EQUAL_UINT(1, fas.forceStopCalls().size());
  TEST_ASSERT_EQUAL_UINT8(0, fas.forceStopCalls()[0]);

  // disableOutputs should also have been called (at least once from thermal overrun)
  TEST_ASSERT_TRUE(fas.disableOutputsCalls().size() >= 1);
  TEST_ASSERT_EQUAL_UINT8(0, fas.disableOutputsCalls()[0]);

  // Verify order: forceStop should come BEFORE disableOutputs in thermal handling
  // Find positions in event log
  int force_stop_pos = -1;
  int disable_outputs_pos = -1;
  for (size_t i = 0; i < g_events.size(); ++i) {
    if (g_events[i].type == "FORCE_STOP" && g_events[i].id == 0) {
      force_stop_pos = static_cast<int>(i);
    }
    if (g_events[i].type == "DISABLE_OUTPUTS" && g_events[i].id == 0 && force_stop_pos >= 0) {
      // Only capture disableOutputs AFTER we've seen forceStop (the thermal one)
      disable_outputs_pos = static_cast<int>(i);
      break;  // Found the pair we care about
    }
  }
  TEST_ASSERT_TRUE_MESSAGE(force_stop_pos >= 0, "forceStop should be called");
  TEST_ASSERT_TRUE_MESSAGE(disable_outputs_pos >= 0, "disableOutputs should be called after forceStop");
  TEST_ASSERT_TRUE_MESSAGE(force_stop_pos < disable_outputs_pos,
                           "forceStop should be called BEFORE disableOutputs");
}
