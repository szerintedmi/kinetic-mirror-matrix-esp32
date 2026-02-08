# Motion Control

## Position Space

- User-space positions: commands use logical steps
- Hardware-space: `hw_pos = user_pos * microstep_multiplier`
- Speed and accel also scaled by multiplier to maintain physical velocity
- STATUS divides back to user-space (integer division, lossy by design — sub-step positions shouldn't exist)
- Soft limits (`MIN_POS_STEPS`/`MAX_POS_STEPS`) enforced on MOVE only; HOME and dither ignore them

## Microstepping

- `MicrostepMode` enum values = log₂(multiplier): FULL=0, HALF=1, QUARTER=2, etc.
- Shared across all motors (one set of M0/M1/M2 pins)
- Changing mode requires **all motors stopped and asleep**
- Default: 1/32 at boot
- Accepts 3 input formats: name (`HALF`), fraction (`1/2`), multiplier (`2`)

## Homing

3-phase state machine with **group barriers**:

1. **Phase 0**: Move negative by `full_range + overshoot`
2. **Phase 1**: Move positive by `backoff`
3. **Phase 2**: Move to center (`full_range / 2`)

- All motors in phase N must complete before any advance to phase N+1
- Center is always `full_range / 2` (not configurable)
- Default `full_range = MAX_POS - MIN_POS = 2400`
- On completion: position reset to 0, `homed = true`

## Direction Latch

- DIR must be pre-latched via SPI **before** `moveTo()` — library callback is too slow (~5-7ms)
- Atomic dirty flag with exchange loop prevents dropped updates during SPI transfer
- Only latches when bits actually changed (reduces SPI traffic)

## Timing Estimation

- Asymmetric accel/decel supported; `decel = 0` models instant stops
- All estimates use ceiling division (conservative)
- Shared-step adds empirical overhead: 10ms base for MOVE, 40ms for HOME
