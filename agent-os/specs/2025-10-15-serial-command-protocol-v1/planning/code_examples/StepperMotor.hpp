#pragma once

#include "StepperTypes.h"

#include <Arduino.h>
#include <FastAccelStepper.h>

namespace StepperControl {

class StepperMotor {
public:
  StepperMotor(uint16_t id, const MotorPins& pins);

  uint16_t id() const {
    return id_;
  }
  const MotorPins& pins() const {
    return pins_;
  }
  void setPins(const MotorPins& pins) {
    pins_ = pins;
  }

  bool attach(FastAccelStepperEngine& engine);
  void detach();
  bool isAttached() const {
    return stepper_ != nullptr;
  }

  bool driverAwake() const {
    return driverAwake_;
  }
  void setDriverAwake(bool awake, unsigned long wakeDelayMs);
  bool ensureDriverAwake(unsigned long wakeDelayMs);

  unsigned long autoSleepRequestMs() const {
    return autoSleepRequestMs_;
  }
  void startAutoSleepTimer(unsigned long now) {
    autoSleepRequestMs_ = now;
  }
  void clearAutoSleepTimer() {
    autoSleepRequestMs_ = 0;
  }

  MoveResultCode moveTo(long targetPosition);
  MoveResultCode moveRelative(long deltaSteps);
  bool isRunning() const;
  long getCurrentPosition() const;
  long getTargetPosition() const;
  float getCurrentSpeedHz() const;
  void setCurrentPosition(long position);
  void forceStopAtCurrentPosition();

  bool configureMotion(float maxSpeedHz, float acceleration);

  void resetRuntimeState();
  void startMove(int direction, long startPosition, unsigned long startMillis);
  void recordLastRun(bool aborted, unsigned long now, long currentPos);
  void updatePlannedSteps(long steps);

  struct HomingState {
    enum class Stage : uint8_t { Inactive, Forward, Backoff, Center } stage = Stage::Inactive;

    bool commandQueued = false;
    long forwardSteps = 0;
    long backoffSteps = 0;
    long limitMin = 0;
    long limitMax = 0;
    unsigned long startMs = 0;
    long totalSteps = 0;
  };

  void beginHoming(
      unsigned long now, long forwardSteps, long backoffSteps, long limitMin, long limitMax);
  void accumulateHomingSteps(long steps);
  void finishHoming(bool aborted, unsigned long now);
  void clearHomingState();
  bool homingActive() const {
    return homing_.stage != HomingState::Stage::Inactive;
  }
  HomingState& homingState() {
    return homing_;
  }
  const HomingState& homingState() const {
    return homing_;
  }

  struct ActiveMove {
    int direction = 1;
    long startPosition = 0;
    unsigned long startMillis = 0;
    long plannedSteps = 0;
  };

  const ActiveMove& activeMove() const {
    return activeMove_;
  }
  int moveDirection() const {
    return activeMove_.direction;
  }

  RunMode mode = RunMode::Idle;

  struct LastRunInfo {
    bool valid = false;
    bool aborted = false;
    unsigned long startMs = 0;
    unsigned long durationMs = 0;
    long steps = 0;
    unsigned long loopMaxGapUs = 0;
  } lastRun;

private:
  uint16_t id_ = 0;
  MotorPins pins_;
  FastAccelStepper* stepper_ = nullptr;
  bool driverAwake_ = false;
  unsigned long autoSleepRequestMs_ = 0;
  ActiveMove activeMove_;
  long lastKnownPosition_ = 0;
  HomingState homing_;
};

}  // namespace StepperControl
