#include "StepperMotor.hpp"

#include <Arduino.h>

namespace StepperControl
{

  StepperMotor::StepperMotor(uint16_t id, const MotorPins &pins)
      : id_(id), pins_(pins)
  {
  }

  bool StepperMotor::attach(FastAccelStepperEngine &engine)
  {
    detach();

    stepper_ = engine.stepperConnectToPin(static_cast<uint8_t>(pins_.step));
    if (!stepper_)
    {
      Serial.printf("StepperControl: Failed to attach motor %u to STEP pin %d\n", id_, pins_.step);
      return false;
    }

    stepper_->setDirectionPin(static_cast<uint8_t>(pins_.dir));
    stepper_->setEnablePin(static_cast<uint8_t>(pins_.sleep), false);
    stepper_->setAutoEnable(false);
    stepper_->disableOutputs();

    driverAwake_ = false;
    autoSleepRequestMs_ = 0;
    lastKnownPosition_ = 0;
    resetRuntimeState();
    return true;
  }

  void StepperMotor::detach()
  {
    if (stepper_)
    {
      stepper_->disableOutputs();
      stepper_->detachFromPin();
      stepper_ = nullptr;
    }
    driverAwake_ = false;
    autoSleepRequestMs_ = 0;
    resetRuntimeState();
  }

  void StepperMotor::setDriverAwake(bool awake, unsigned long wakeDelayMs)
  {
    if (!stepper_)
    {
      driverAwake_ = false;
      autoSleepRequestMs_ = 0;
      return;
    }

    if (awake)
    {
      if (!driverAwake_)
      {
        stepper_->enableOutputs();
        if (wakeDelayMs > 0)
        {
          delay(wakeDelayMs);
        }
        driverAwake_ = true;
      }
      autoSleepRequestMs_ = 0;
      return;
    }

    if (!driverAwake_)
    {
      return;
    }

    stepper_->disableOutputs();
    driverAwake_ = false;
    autoSleepRequestMs_ = 0;
  }

  bool StepperMotor::ensureDriverAwake(unsigned long wakeDelayMs)
  {
    if (!driverAwake_)
    {
      setDriverAwake(true, wakeDelayMs);
    }
    return driverAwake_;
  }

  MoveResultCode StepperMotor::moveTo(long targetPosition)
  {
    if (!stepper_)
    {
      return MOVE_ERR_ACCELERATION_IS_UNDEFINED;
    }
    return stepper_->moveTo(targetPosition);
  }

  MoveResultCode StepperMotor::moveRelative(long deltaSteps)
  {
    if (!stepper_)
    {
      return MOVE_ERR_ACCELERATION_IS_UNDEFINED;
    }
    return stepper_->move(deltaSteps);
  }

  bool StepperMotor::isRunning() const
  {
    return stepper_ && stepper_->isRunning();
  }

  long StepperMotor::getCurrentPosition() const
  {
    return stepper_ ? stepper_->getCurrentPosition() : lastKnownPosition_;
  }

  long StepperMotor::getTargetPosition() const
  {
    return stepper_ ? stepper_->targetPos() : lastKnownPosition_;
  }

  float StepperMotor::getCurrentSpeedHz() const
  {
    if (!stepper_)
    {
      return 0.0f;
    }
    return static_cast<float>(stepper_->getCurrentSpeedInMilliHz(true)) / 1000.0f;
  }

  void StepperMotor::setCurrentPosition(long position)
  {
    if (stepper_)
    {
      stepper_->setCurrentPosition(position);
    }
    lastKnownPosition_ = position;
    activeMove_.startPosition = position;
    activeMove_.startMillis = millis();
  }

  void StepperMotor::forceStopAtCurrentPosition()
  {
    if (!stepper_)
    {
      return;
    }
    long currentPos = stepper_->getCurrentPosition();
    stepper_->forceStopAndNewPosition(currentPos);
    lastKnownPosition_ = currentPos;
  }

  bool StepperMotor::configureMotion(float maxSpeedHz, float acceleration)
  {
    if (!stepper_)
    {
      return false;
    }

    bool ok = true;
    if (stepper_->setSpeedInHz(static_cast<uint32_t>(maxSpeedHz)) != 0)
    {
      ok = false;
    }
    if (stepper_->setAcceleration(static_cast<int32_t>(acceleration)) != 0)
    {
      ok = false;
    }
    return ok;
  }

  void StepperMotor::resetRuntimeState()
  {
    mode = RunMode::Idle;
    activeMove_.direction = 1;
    activeMove_.plannedSteps = 0;
    activeMove_.startPosition = lastKnownPosition_;
    activeMove_.startMillis = millis();

    lastRun.valid = false;
    lastRun.aborted = false;
    lastRun.startMs = 0;
    lastRun.durationMs = 0;
    lastRun.steps = 0;
    lastRun.loopMaxGapUs = 0;
  }

  void StepperMotor::startMove(int direction, long startPosition, unsigned long startMillis)
  {
    activeMove_.direction = (direction >= 0) ? 1 : -1;
    activeMove_.startPosition = startPosition;
    activeMove_.startMillis = startMillis;
    activeMove_.plannedSteps = 0;
    lastKnownPosition_ = startPosition;

    lastRun.valid = false;
    lastRun.aborted = false;
    lastRun.startMs = startMillis;
    lastRun.durationMs = 0;
    lastRun.steps = 0;
    lastRun.loopMaxGapUs = 0;
  }

  void StepperMotor::updatePlannedSteps(long steps)
  {
    activeMove_.plannedSteps = steps < 0 ? -steps : steps;
  }

  void StepperMotor::beginHoming(unsigned long now, long forwardSteps, long backoffSteps, long limitMin, long limitMax)
  {
    homing_.stage = HomingState::Stage::Forward;
    homing_.commandQueued = false;
    homing_.forwardSteps = forwardSteps < 0 ? 0 : forwardSteps;
    homing_.backoffSteps = backoffSteps < 0 ? 0 : backoffSteps;
    homing_.limitMin = limitMin;
    homing_.limitMax = limitMax;
    homing_.startMs = now;
    homing_.totalSteps = 0;

    if (homing_.forwardSteps == 0)
    {
      homing_.stage = homing_.backoffSteps > 0 ? HomingState::Stage::Backoff : HomingState::Stage::Center;
    }
  }

  void StepperMotor::accumulateHomingSteps(long steps)
  {
    if (steps < 0)
    {
      steps = -steps;
    }
    homing_.totalSteps += steps;
  }

  void StepperMotor::finishHoming(bool aborted, unsigned long now)
  {
    lastRun.valid = true;
    lastRun.aborted = aborted;
    lastRun.startMs = homing_.startMs;
    lastRun.durationMs = (homing_.startMs != 0) ? (now - homing_.startMs) : 0;
    lastRun.steps = homing_.totalSteps;
    lastRun.loopMaxGapUs = 0;

    clearHomingState();
  }

  void StepperMotor::clearHomingState()
  {
    homing_.stage = HomingState::Stage::Inactive;
    homing_.commandQueued = false;
    homing_.forwardSteps = 0;
    homing_.backoffSteps = 0;
    homing_.limitMin = 0;
    homing_.limitMax = 0;
    homing_.startMs = 0;
    homing_.totalSteps = 0;
  }

  void StepperMotor::recordLastRun(bool aborted, unsigned long now, long currentPos)
  {
    lastRun.valid = true;
    lastRun.aborted = aborted;
    lastRun.startMs = activeMove_.startMillis;
    lastRun.durationMs = (activeMove_.startMillis != 0) ? (now - activeMove_.startMillis) : 0;
    lastRun.steps = currentPos - activeMove_.startPosition;
    lastRun.loopMaxGapUs = 0;

    lastKnownPosition_ = currentPos;
    activeMove_.startPosition = currentPos;
    activeMove_.startMillis = now;
  }

} // namespace StepperControl
