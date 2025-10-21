## Summary
- Added µs-scale DIR guard constants and a scheduling helper to compute safe SLEEP gating windows around direction flips, aligned between STEP edges.
- Wrote focused unit tests validating guard feasibility and that scheduled windows never touch STEP edges.
- Confirmed 74HC595 driver already batches DIR+SLEEP byte updates and latches once per change cycle.

## Files Changed
- include/MotorControl/SharedStepGuards.h — Guard constants (`DIR_GUARD_US_PRE/POST`) and `compute_flip_window()` returning a `DirFlipWindow` (SLEEP low → DIR flip → SLEEP high) aligned mid-gap between edges.
- test/test_MotorControl/test_SharedStepGuards.cpp — 3 focused tests covering constants feasibility and scheduling behavior.
- test/test_MotorControl/test_main.cpp — Registered the new tests in the MotorControl suite.

## Implementation Details
### Guard constants and scheduling
**Location:** `include/MotorControl/SharedStepGuards.h`
- Defaults: `DIR_GUARD_US_PRE=3`, `DIR_GUARD_US_POST=3` (overridable at build time).
- Uses `SharedStepTiming::align_to_next_edge_us()` to schedule mid-gap (`edge + period/2`), ensuring guards are well away from edges.
**Rationale:** Deterministic, testable scheduling without hardware coupling; suitable for integration with RMT ISR or controller loop in later tasks.

### 74HC595 latching
**Location:** `src/drivers/Esp32/Shift595Vspi.cpp`
- Already transfers DIR then SLEEP and latches once per update; this matches "batched + single latch per change cycle" requirement.

## Testing
### Test Files Created/Updated
- `test/test_MotorControl/test_SharedStepGuards.cpp` — Guard feasibility and window placement.
- `test/test_MotorControl/test_main.cpp` — Invokes guard tests.

### Test Coverage
- Unit tests: ✅ Complete for scheduling logic and constants.
- Integration tests: ⚠️ Defer until TG3 when we wire scheduling into controller/generator flow.

### Manual Testing Performed
- None required for this logic (host-validated). On-device verification will occur post-integration.

## User Standards & Preferences Compliance
### Coding Style & Conventions
**Refs:** `agent-os/standards/global/coding-style.md`, `agent-os/standards/global/conventions.md`
**Compliance:** Small, focused helpers; explicit constants; pure functions for testability.

### Task Structure & Hardware Abstraction
**Refs:** `agent-os/standards/backend/task-structure.md`, `agent-os/standards/backend/hardware-abstraction.md`
**Compliance:** Timing and scheduling isolated from hardware drivers; defers ISR specifics to RMT integration.

### Testing Standards
**Refs:** `agent-os/standards/testing/unit-testing.md`
**Compliance:** 3 focused unit tests; host-native; no exhaustive suites.

## Known Issues & Limitations
1. Scheduling not yet invoked at runtime; integration with generator/controller is planned in TG3.
2. Assumes STEP edges are spaced by `period_us`; duty cycle specifics handled later with RMT items if needed.

## Dependencies for Other Tasks
- TG3 (Integration + Protocol Simplification) — will call `compute_flip_window()` to time SLEEP/DIR updates relative to STEP edges.

