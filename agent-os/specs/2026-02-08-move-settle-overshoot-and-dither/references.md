# References for Move Settle: Overshoot & Dither

## Similar Implementations

### Homing State Machine

- **Location**: `lib/MotorControl/src/HardwareMotorController.cpp:431-480`
- **Relevance**: The 3-phase homing sequence (overshoot → backoff → center) is the closest existing pattern to the multi-phase move settle. It uses barrier-synchronized phase transitions driven by `tick()` polling `isMoving()`.
- **Key patterns**:
  - Phase transitions triggered when motor stops (`!running`)
  - New `startMoveAbs()` issued for each phase leg
  - DIR bits pre-latched before each sub-move
  - `moving` stays true throughout all phases
  - Completion marked only after final phase

### handleMove() — MOVE Command Handler

- **Location**: `lib/MotorControl/src/command/CommandHandlers.cpp:387-575`
- **Relevance**: Entry point for MOVE commands. Contains parameter parsing (comma-split, optional fields), microstep scaling, thermal pre-check, and `moveAbsMask()` call. New overshoot/dither params extend this parsing.
- **Key patterns**:
  - `Split(args, ',')` for positional params
  - Empty string = use default from context
  - `#if (USE_SHARED_STEP)` guards for rejecting unsupported params
  - Existing "reject non-empty parts > 4" guard at lines 450-456 must be removed/extended

### handleGet() / handleSet() — Settings

- **Location**: `lib/MotorControl/src/command/CommandHandlers.cpp:908-1154`
- **Relevance**: SPEED/ACCEL/DECEL/MICROSTEP GET/SET pattern is the template for adding MOVE_OVERSHOOT, DITHER_AMPLITUDE, DITHER_CYCLES, DITHER_MIN_AMPLITUDE.
- **Key patterns**:
  - `key == "SPEED"` exact match after `ToUpperCopy()`
  - Parse value with `ParseInt()`, validate (> 0 for speed/accel, >= 0 for decel)
  - Check all motors idle before allowing change
  - Assign via context reference: `context.defaultSpeed() = static_cast<int>(v)`
  - GET ALL returns all settings in one response

### CommandExecutionContext

- **Location**: `lib/MotorControl/include/MotorControl/command/CommandExecutionContext.h`
- **Relevance**: Reference-based context pattern. Constructor takes `int&` references to processor state. Handlers access/mutate defaults through these references. New settle defaults follow the same pattern.
- **Key patterns**:
  - Constructor: `int& default_speed_sps_` (reference to processor member)
  - Accessor: `int& defaultSpeed()` returns mutable reference
  - All 8 constructor params are references — handlers mutate processor state directly

### MotorCommandProcessor

- **Location**: `lib/MotorControl/include/MotorControl/MotorCommandProcessor.h`
- **Relevance**: Owns the default state variables and constructs `CommandExecutionContext` via `makeContext()`. New settle defaults are stored here and passed by reference.
- **Key patterns**:
  - Private members: `int default_speed_sps_`, `int default_accel_sps2_`, etc.
  - Read-only accessors for MQTT config publisher: `int defaultSpeedSps() const`
  - `makeContext()` passes all members by reference

### StubMotorController

- **Location**: `lib/MotorControl/src/StubMotorController.h/.cpp`
- **Relevance**: Test-only motor backend. Uses `MovePlan` with `end_ms` for time-based simulation. Multi-phase settle can be simulated as single timed block (total duration = sum of all leg estimates).
- **Key patterns**:
  - `moveAbsMask()` stores `plans_[i].end_ms = now_ms + dur_ms`
  - `tick()` completes move when `now_ms >= plans_[i].end_ms`
  - Duration estimated via `MotionKinematics::estimateMoveTimeMs()`
  - Aggregate init of `MotorState` — new fields need default member initializers

### Python CLI Move Command

- **Location**: `tools/mirror_cli/cli.py:52-55`, `tools/mirror_cli/command_builder.py:259-273`
- **Relevance**: CLI argument parsing and MQTT command building for MOVE. Currently no per-move speed/accel; HOME has the placeholder pattern we'll follow.
- **Key patterns**:
  - `cli.py`: `_join_home_with_placeholders()` builds `HOME:0,800,,2400` with empty fields
  - `command_builder.py`: `_parse_csv_arguments()` + index-based optional param extraction
  - MQTT params dict: `{"target_ids": 0, "position_steps": 800, "speed": 4000}`

### MotionKinematics

- **Location**: `lib/MotorControl/src/MotionKinematics.cpp:53-82`
- **Relevance**: `estimateMoveTimeMs(distance, speed, accel)` — reusable for each settle phase leg. Handles both trapezoidal and triangular motion profiles with ceiling division.
