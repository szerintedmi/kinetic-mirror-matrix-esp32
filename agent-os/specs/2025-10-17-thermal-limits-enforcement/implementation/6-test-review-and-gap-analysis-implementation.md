Implementation Notes: Task Group 6 — Test Review & Gap Analysis

Summary
- Reviewed tests from Task Groups 1–5; added feature-level tests to assert WARN/OK ordering and ERR behavior with THERMAL_LIMITING toggles.

New Tests
- test/test_MotorControl/test_FeatureFlows.cpp
  - test_flow_set_off_move_exceeds_max_warn_ok: SET THERMAL_LIMITING=OFF, attempt long MOVE → expects first line WARN THERMAL_REQ_GT_MAX, last line CTRL:OK est_ms=...
  - test_flow_set_on_move_exceeds_max_err: SET THERMAL_LIMITING=ON, same MOVE → expects CTRL:ERR E10 THERMAL_REQ_GT_MAX.

Existing Coverage Highlights
- Preflight E10/E11 error paths and WARN variants when disabled.
- WAKE rejection/warn (E12) with and without limits.
- Auto-sleep after overrun cancels active move and sets awake=0.
- CLI helpers parse GET THERMAL_LIMITING and handle WARN+OK response shape.

Result
- Gaps addressed: end-to-end WARN/OK ordering and deterministic ERR when enabled.
- All native tests pass.
