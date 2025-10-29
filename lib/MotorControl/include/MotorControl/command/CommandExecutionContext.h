#pragma once

#include "MotorControl/MotorController.h"
#include "net_onboarding/NetOnboarding.h"

#include <cstdint>
#include <functional>

namespace motor {
namespace command {

class CommandExecutionContext {
public:
  CommandExecutionContext(MotorController &controller,
                          bool &thermal_limits_enabled,
                          int &default_speed_sps,
                          int &default_accel_sps2,
                          int &default_decel_sps2,
                          bool &in_batch,
                          bool &batch_initially_idle);

  MotorController &controller();
  const MotorController &controller() const;

  bool thermalLimitsEnabled() const;
  void setThermalLimitsEnabled(bool enabled);

  int &defaultSpeed();
  int &defaultAccel();
  int &defaultDecel();

  uint32_t nextCid() const;
  void setActiveCid(uint32_t cid) const;
  void clearActiveCid() const;
  bool printCtrlLineImmediate(const std::string &line) const;

  net_onboarding::NetOnboarding &net() const;

  bool inBatch() const;
  bool batchInitiallyIdle() const;
  void setBatchState(bool in_batch, bool initially_idle);

private:
  MotorController &controller_;
  bool &thermal_limits_enabled_;
  int &default_speed_sps_;
  int &default_accel_sps2_;
  int &default_decel_sps2_;
  bool &in_batch_;
  bool &batch_initially_idle_;
};

} // namespace command
} // namespace motor
