#pragma once

#include "MotorControl/MotorController.h"
#include "net_onboarding/NetOnboarding.h"

#include <string>

namespace motor {
namespace command {

class CommandExecutionContext {
public:
  CommandExecutionContext(MotorController& controller,
                          bool& thermal_limits_enabled,
                          int& default_speed_sps,
                          int& default_accel_sps2,
                          int& default_decel_sps2,
                          uint8_t& microstep_multiplier,
                          bool& in_batch,
                          bool& batch_initially_idle,
                          int& default_move_overshoot,
                          int& default_dither_amplitude,
                          int& default_dither_cycles,
                          int& default_dither_min_amplitude);

  MotorController& controller();
  const MotorController& controller() const;

  bool thermalLimitsEnabled() const;
  void setThermalLimitsEnabled(bool enabled);

  int& defaultSpeed();
  int& defaultAccel();
  int& defaultDecel();
  int& defaultMoveOvershoot();
  int& defaultDitherAmplitude();
  int& defaultDitherCycles();
  int& defaultDitherMinAmplitude();
  uint8_t microstepMultiplier() const;
  uint8_t& microstepMultiplierRef();

  std::string nextMsgId() const;
  void setActiveMsgId(const std::string& msg_id) const;
  void clearActiveMsgId() const;
  bool printCtrlLineImmediate(const std::string& line) const;

  net_onboarding::NetOnboarding& net() const;

  bool inBatch() const;
  bool batchInitiallyIdle() const;
  void setBatchState(bool in_batch, bool initially_idle);

private:
  MotorController& controller_;
  bool& thermal_limits_enabled_;
  int& default_speed_sps_;
  int& default_accel_sps2_;
  int& default_decel_sps2_;
  uint8_t& microstep_multiplier_;
  bool& in_batch_;
  bool& batch_initially_idle_;
  int& default_move_overshoot_;
  int& default_dither_amplitude_;
  int& default_dither_cycles_;
  int& default_dither_min_amplitude_;
};

}  // namespace command
}  // namespace motor
