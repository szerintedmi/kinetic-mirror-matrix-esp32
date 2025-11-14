#pragma once

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

#if defined(ARDUINO)

#include "hal/FasAdapter.h"
#include "MotorControl/SharedStepGuards.h"
#include "MotorControl/SharedStepTiming.h"
#include "drivers/Esp32/SharedStepRmt.h"

#include <Arduino.h>
#include <array>
#include <climits>

// Shared-STEP motion adapter implementing IFasAdapter without FastAccelStepper.
// Generates a single STEP pulse train and gates per-motor SLEEP via 74HC595.
class SharedStepAdapterEsp32 : public IFasAdapter {
public:
  SharedStepAdapterEsp32();
  void begin() override;
  void configureStepPin([[maybe_unused]] uint8_t motor_id, [[maybe_unused]] int gpio) override {}
  bool startMoveAbs(uint8_t motor_id, long target, int speed, int accel) override;
  bool isMoving(uint8_t motor_id) const override;
  long currentPosition(uint8_t motor_id) const override;
  void setCurrentPosition(uint8_t motor_id, long pos) override;

  void attachShiftRegister(IShift595* drv) override {
    shift_ = drv;
  }
  void setAutoEnable([[maybe_unused]] uint8_t motor_id,
                     [[maybe_unused]] bool auto_enable) override {}
  void enableOutputs(uint8_t motor_id) override;
  void disableOutputs(uint8_t motor_id) override;
  void setDeceleration(int decel_sps2) override {
    d_sps2_ = decel_sps2 > 0 ? decel_sps2 : 0;
  }

private:
  static constexpr uint8_t kMotorSlots = 8;
  struct Slot {
    bool moving = false;
    bool awake = false;
    bool forced_awake = false;  // set via enableOutputs(); prevents auto-sleep
    int dir_sign = 1;           // +1 fwd, -1 rev
    int speed_sps = 0;
    long target = 0;
    long pos = 0;
    uint32_t last_us = 0;
    uint32_t acc_us = 0;  // fractional accumulator
  };

  struct FlipSchedule {
    bool active = false;
    uint8_t phase = 0;  // 0: wait low, 1: wait flip, 2: wait high
    SharedStepGuards::DirFlipWindow w{};
    int new_dir_sign = +1;
  };

  void maybeStartGen_(int speed_sps) const;
  void maybeStopGen_() const;
  void flushLatch_() const;
  void updateProgress_(uint8_t motor_id) const;
  void runFlipScheduler_(uint8_t motor_id, uint32_t now_us) const;
  void updateRamp_(uint32_t now_us) const;
  void setDirectionBit_(uint8_t motor_id, int dir_sign) const;
  bool computeMotionWindow_(uint32_t* min_remaining) const;
  struct DivisionOperands {
    uint64_t numerator = 0;
    uint64_t denominator = 0;
    explicit constexpr DivisionOperands(uint64_t num = 0, uint64_t den = 0)
        : numerator(num), denominator(den) {}
  };
  static inline uint32_t ceil_div_u32(DivisionOperands operands) {
    if (operands.denominator == 0) {
      return 0;
    }
    return static_cast<uint32_t>((operands.numerator + operands.denominator - 1ULL) /
                                 operands.denominator);
  }

  // Edge hook plumbing (ISR context)
  static void onRisingEdgeHook_(void* context);

  mutable std::array<Slot, kMotorSlots> slots_{};
  mutable std::array<FlipSchedule, kMotorSlots> flips_{};
  IShift595* shift_ = nullptr;
  mutable SharedStepRmtGenerator gen_;
  mutable bool gen_running_ = false;
  mutable int current_speed_sps_ = 0;
  mutable uint32_t period_us_ = 0;
  mutable uint64_t phase_anchor_us_ = 0;  // approximate reference for edge alignment
  volatile mutable uint8_t dir_bits_ = 0;
  volatile mutable uint8_t sleep_bits_ = 0;

  // Global ramp state (shared STEP → single global speed trajectory)
  mutable uint32_t ramp_last_us_ = 0;
  mutable int v_cur_sps_ = 0;      // current generator speed
  mutable int v_max_sps_ = 0;      // target cruise speed
  mutable int a_sps2_ = 0;         // global acceleration
  mutable int d_sps2_ = 0;         // global deceleration (0 disables ramp-down)
  mutable uint32_t dv_accum_ = 0;  // fractional accumulator in (sps*us)

  static constexpr uint32_t kMinGenSps = 20;  // keep RMT durations in range
};

#endif  // ARDUINO
