# Command Pipeline

## Parsing

- Commands split by `;` for batching: `MOVE:0,100;HOME:1`
- Colon syntax preferred: `MOVE:0,100`. Space syntax (`MOVE 0,100`) is legacy — colon takes precedence when both present
- Action name auto-uppercased; args preserved as-is

## Execution Context

- `CommandExecutionContext` holds **references** to `MotorCommandProcessor` state (speed, accel, thermal flag, batch state)
- Handlers mutate parent state through these references — no return-value propagation
- Message IDs from global singleton `transport::message_id::Next()` (UUID v4)

## Batch Execution

- Conflict detection via bitmask: two commands targeting the same motor in one batch → `E03 BAD_PARAM MULTI_CMD_CONFLICT`
- Batched commands run **in parallel** — aggregated `est_ms` uses `max()`, not sum
- Individual `est_ms` values stripped from response; one aggregate added

## Error Propagation

- `CommandResult::mergeFrom()` — errors are "contagious": if any merged result has `is_error=true`, the whole result becomes error
- A result can contain both ACK and WARN lines while `is_error=false`
