# Thermal Budget

## Model

- Budget tracked in **tenths of seconds** (100ms granules) with sub-ms accumulator to prevent drift
- Per-motor, not persisted across reboots
- Constants in `MotorControlConstants.h`:
  - `MAX_RUNNING_TIME_S = 90` (full budget)
  - `SPEND_TENTHS_PER_SEC = 10` (1.0 s/s when awake)
  - `REFILL_TENTHS_PER_SEC = 15` (1.5 s/s when asleep) — practical tuning, not spec-based
  - `AUTO_SLEEP_IF_OVER_BUDGET_S = 5` (grace period before forced sleep)

## Spend/Refill

- Awake motors: budget decreases at 1x rate
- Asleep motors: budget refills at 1.5x rate (faster recovery)
- Floor capped to prevent unbounded negative drift
- Minimum refill of 1 tenth per tick to avoid stall at low dt

## Enforcement

- **Limits enabled** (production): thermal violations return errors (E10, E11, E12)
- **Limits disabled** (dev/test): violations execute with WARN, not errors
- Toggled via `SET THERMAL_LIMITING=ON|OFF`
- Auto-sleep triggers after 5s overrun when limits enabled

## Status Reporting

- `budget_s` = current budget in seconds
- `ttfc_s` = time-to-full-capacity (estimated cooldown remaining)
