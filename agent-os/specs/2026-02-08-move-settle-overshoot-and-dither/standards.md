# Standards for Move Settle: Overshoot & Dither

The following standards apply to this work.

---

## backend/command-pipeline

- Commands split by `;` for batching: `MOVE:0,100;HOME:1`
- Colon syntax preferred: `MOVE:0,100`. Space syntax (`MOVE 0,100`) is legacy — colon takes precedence when both present
- Action name auto-uppercased; args preserved as-is
- `CommandExecutionContext` holds **references** to `MotorCommandProcessor` state (speed, accel, thermal flag, batch state)
- Handlers mutate parent state through these references — no return-value propagation
- Message IDs from global singleton `transport::message_id::Next()` (UUID v4)
- Conflict detection via bitmask: two commands targeting the same motor in one batch → `E03 BAD_PARAM MULTI_CMD_CONFLICT`
- Batched commands run **in parallel** — aggregated `est_ms` uses `max()`, not sum
- `CommandResult::mergeFrom()` — errors are "contagious": if any merged result has `is_error=true`, the whole result becomes error

---

## backend/motion-control

- User-space positions: commands use logical steps
- Hardware-space: `hw_pos = user_pos * microstep_multiplier`
- Speed and accel also scaled by multiplier to maintain physical velocity
- STATUS divides back to user-space (integer division, lossy by design)
- Soft limits (`MIN_POS_STEPS`/`MAX_POS_STEPS`) enforced on MOVE only; HOME and dither ignore them
- `MicrostepMode` enum values = log2(multiplier): FULL=0, HALF=1, QUARTER=2, etc.
- Shared across all motors (one set of M0/M1/M2 pins)
- Changing mode requires **all motors stopped and asleep**
- Homing: 3-phase state machine with **group barriers** (Phase 0: negative overshoot, Phase 1: positive backoff, Phase 2: center)
- DIR must be pre-latched via SPI **before** `moveTo()` — library callback is too slow
- Timing estimation: asymmetric accel/decel supported; all estimates use ceiling division (conservative)

---

## backend/thermal-budget

- Budget tracked in **tenths of seconds** (100ms granules) with sub-ms accumulator
- Per-motor, not persisted across reboots
- Constants: `MAX_RUNNING_TIME_S = 90`, `SPEND_TENTHS_PER_SEC = 10`, `REFILL_TENTHS_PER_SEC = 15`, `AUTO_SLEEP_IF_OVER_BUDGET_S = 5`
- Awake motors: budget decreases at 1x rate; asleep: refills at 1.5x rate
- **Limits enabled** (production): thermal violations return errors (E10, E11, E12)
- **Limits disabled** (dev/test): violations execute with WARN, not errors
- Auto-sleep triggers after 5s overrun when limits enabled

---

## backend/error-codes

Central catalog: `lib/transport/src/CommandSchema.cpp`

| Code | Reason | Description |
|------|--------|-------------|
| E01 | BAD_CMD | Unknown action |
| E02 | BAD_ID | Invalid motor ID or mask |
| E03 | BAD_PARAM | Parameter validation failed |
| E04 | BUSY | Motor/controller busy |
| E07 | POS_OUT_OF_RANGE | Position outside travel range |
| E10 | THERMAL_REQ_GT_MAX | Move exceeds max thermal budget |
| E11 | THERMAL_NO_BUDGET | Insufficient thermal budget |

New motor errors: use next available E## code. All codes must be added to the catalog.

---

## frontend/python-cli

- `SerialWorker` and `MqttWorker` implement the same interface via duck typing
- Shared methods: `queue_cmd()`, `get_state()`, `stop()`
- Default transport: MQTT
- ACK reception extends deadline by another base_timeout window
- Batch splitting: `;` delimiter, respects quoted strings
- Optional positional params: empty strings for skipped fields (`HOME:0,,100` = no overshoot, backoff=100)
- MQTT pending commands triple-indexed: by cmd_id, by local handle, by FIFO order

---

## testing/unit-testing

- **Framework**: Unity (`<unity.h>`). Used for both host and device tests.
- **Test on host first**: `[env:native]` uses `-DUSE_STUB_BACKEND` and `-fsanitize=address`
- **Keep suites fast**: Target sub-2s native runtimes. Use `TEST_TIMEOUT_GUARD(ms)` for hung tests.
- **Assertion messages**: Use `_MESSAGE` variants when macros don't print expected/actual.
- **Shared helpers**: `test_common/TestHelpers.h` (`SplitLines`, `FindStatusLineForId`, `ParseEstMs`, `ParseMsgId`).
- **File organization**: Large suites split tests across multiple `.cpp` files grouped by feature, with central `test_main.cpp`.
