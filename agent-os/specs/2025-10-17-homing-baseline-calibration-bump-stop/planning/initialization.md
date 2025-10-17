# Initial Spec Idea

## User's Initial Description
3. [ ] Homing & Baseline Calibration (Bump‑Stop) — Implement bump‑stop homing without switches: drive toward an end for `full_range + overshoot` steps, back off `backoff` steps, then move `full_range/2` to midpoint and set zero. Parameters have safe defaults and can be overridden via `HOME` (order: overshoot, backoff, speed, accel, full_range). Enforce slow, conservative speed during HOME (defaults may be overridden). Depends on: 2. `M`

## Metadata
- Date Created: 2025-10-17
- Spec Name: homing-baseline-calibration-bump-stop
- Spec Path: agent-os/specs/2025-10-17-homing-baseline-calibration-bump-stop

