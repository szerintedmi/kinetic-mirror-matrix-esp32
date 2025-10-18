// Central parameters for motion defaults and the runtime (thermal) budget model.

#pragma once
#include <stdint.h>

namespace MotorControlConstants
{

    // Motion limits (absolute step range used across commands)
    constexpr long MIN_POS_STEPS = -1200;
    constexpr long MAX_POS_STEPS = 1200;

    // Default motion parameters (applied when MOVE/HOME omit speed/accel)
    constexpr int DEFAULT_SPEED_SPS = 4000;   // steps per second
    constexpr int DEFAULT_ACCEL_SPS2 = 16000; // steps per second^2
    constexpr long DEFAULT_OVERSHOOT = 800;   // HOME overshoot (steps)
    constexpr long DEFAULT_BACKOFF = 50;      // HOME backoff (steps)

    // Runtime budget model (session-only, diagnostic only)
    // - Budget decreases while driver is awake; refills while asleep
    constexpr int32_t MAX_RUNNING_TIME_S = 90;    // maximum budget when fully "cool" in seconds
    constexpr int32_t FULL_COOL_DOWN_TIME_S = 60; // seconds required to cool down when asleep
    constexpr int32_t MAX_COOL_DOWN_TIME_S = 300; // cap cooldown to avoid unbounded values when overrun budget

    // Fixed-point (tenths of seconds) helpers for budget accounting
    constexpr int32_t BUDGET_TENTHS_MAX = MAX_RUNNING_TIME_S * 10; // helper const in tenths
    constexpr int32_t SPEND_TENTHS_PER_SEC = 10;                   // 1.0 s/s when ON
    constexpr int32_t REFILL_TENTHS_PER_SEC =                      // 1.5 s/s when OFF
        (MAX_RUNNING_TIME_S * 10) / FULL_COOL_DOWN_TIME_S;

    // Grace period beyond zero budget before forced auto-sleep (seconds)
    constexpr int32_t AUTO_SLEEP_IF_OVER_BUDGET_S = 5;

} // namespace MotorControlConstants
