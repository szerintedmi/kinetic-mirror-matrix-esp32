#include "MotorControl/command/CommandExecutionContext.h"

#include "transport/MessageId.h"
#include "net_onboarding/NetSingleton.h"
#include "net_onboarding/SerialImmediate.h"

namespace motor {
namespace command {

CommandExecutionContext::CommandExecutionContext(MotorController &controller,
                                                 bool &thermal_limits_enabled,
                                                 int &default_speed_sps,
                                                 int &default_accel_sps2,
                                                 int &default_decel_sps2,
                                                 bool &in_batch,
                                                 bool &batch_initially_idle)
    : controller_(controller),
      thermal_limits_enabled_(thermal_limits_enabled),
      default_speed_sps_(default_speed_sps),
      default_accel_sps2_(default_accel_sps2),
      default_decel_sps2_(default_decel_sps2),
      in_batch_(in_batch),
      batch_initially_idle_(batch_initially_idle) {}

MotorController &CommandExecutionContext::controller() { return controller_; }
const MotorController &CommandExecutionContext::controller() const { return controller_; }

bool CommandExecutionContext::thermalLimitsEnabled() const { return thermal_limits_enabled_; }

void CommandExecutionContext::setThermalLimitsEnabled(bool enabled) {
  thermal_limits_enabled_ = enabled;
  controller_.setThermalLimitsEnabled(enabled);
}

int &CommandExecutionContext::defaultSpeed() { return default_speed_sps_; }
int &CommandExecutionContext::defaultAccel() { return default_accel_sps2_; }
int &CommandExecutionContext::defaultDecel() { return default_decel_sps2_; }

std::string CommandExecutionContext::nextMsgId() const {
  return transport::message_id::Next();
}

void CommandExecutionContext::setActiveMsgId(const std::string &msg_id) const {
  transport::message_id::SetActive(msg_id);
}

void CommandExecutionContext::clearActiveMsgId() const {
  transport::message_id::ClearActive();
}

bool CommandExecutionContext::printCtrlLineImmediate(const std::string &line) const {
  return net_onboarding::PrintCtrlLineImmediate(line);
}

net_onboarding::NetOnboarding &CommandExecutionContext::net() const {
  return net_onboarding::Net();
}

bool CommandExecutionContext::inBatch() const { return in_batch_; }
bool CommandExecutionContext::batchInitiallyIdle() const { return batch_initially_idle_; }

void CommandExecutionContext::setBatchState(bool in_batch, bool initially_idle) {
  in_batch_ = in_batch;
  batch_initially_idle_ = initially_idle;
}

} // namespace command
} // namespace motor
