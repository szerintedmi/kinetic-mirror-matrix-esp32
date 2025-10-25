# Product Roadmap

1. [x] Serial Command Protocol v1 — Implement human‑readable commands over USB serial with stub backend for early testing: `HELP`, `MOVE:<id|ALL>,<abs_steps>[,<speed>][,<accel>]` (auto‑WAKE before move, auto‑SLEEP after), `HOME:<id|ALL>[,<overshoot>][,<backoff>][,<speed>][,<accel>][,<full_range>]`, `STATUS`, `WAKE:<id|ALL>`, `SLEEP:<id|ALL>`. Return `CTRL:OK` / `CTRL:ERR <code> ...`. Provide a small Python CLI for end‑to‑end tests; include shortcuts `M`/`H`/`ST`. `S`
2. [x] ESP32 8‑Motor Bring‑Up (FastAccelStepper + 74HC595) — Drive eight DRV8825 steppers in full‑step mode from one ESP32 using eight STEP pins and two daisy‑chained 74HC595s for per‑motor DIR and SLEEP (ENABLE semantics). Implement auto‑WAKE/SLEEP in firmware via FastAccelStepper external‑pin callback (enable just‑in‑time before motion; disable immediately after completion). Latch DIR at motion start only, validate concurrent motion and stable SR timing. Integrate with Serial v1. Depends on: 1. `M`
3. [x] Homing & Baseline Calibration (Bump‑Stop) — Implement bump‑stop homing without switches: drive toward an end for `full_range + overshoot` steps, back off `backoff` steps, then move `full_range/2` to midpoint and set zero. Parameters have safe defaults and can be overridden via `HOME` (order: overshoot, backoff, speed, accel, full_range). Enforce slow, conservative speed during HOME (defaults may be overridden). Depends on: 2. `M`
4. [x] Status & Diagnostics — Extend `STATUS` to report per‑motor position, moving/sleeping, homed flag, steps since last home, and thermal runtime metrics (`budget_s`, `ttfc_s`). Add a concise summary view in the host CLI with an interactive mode that polls STATUS (~2 Hz). Expose `GET LAST_OP_TIMING[:<id|ALL>]` for device‑side timing when validating estimates. Depends on: 2. `S`
5. [x] Thermal Limits Enforcement — Enforce per‑motor runtime/cooldown budgets (derived from STATUS metrics) to cap continuous motion and require cooldown before resuming. Thresholds are compile‑time constants for now; enforcement can be toggled at runtime via `SET THERMAL_LIMITING=OFF|ON` and queried with `GET THERMAL_LIMITING`. MOVE/HOME return `est_ms` in `CTRL:OK`, and pre‑flight checks reject or warn when budgets are exceeded. Depends on: 4. `S`
6. [x] Shared-STEP spike for 8 motors on one ESP32 to prep scaling beyond with a single MCU. backend with a hardware-timed shared STEP generator,  DIR/SLEEP on 74HC595, and layered per-motor gating/guard windows so eight motors run in lockstep while we validate patterns for future >8 builds.  Depends on: 2, 4. `S`
   - Status: Parked (2025-10-25). See shared-step status note: [docs/sharedstep-status.md](./docs/sharedstep-status.md)
7. [ ] Preset Replay (MVP) — Add a minimal host‑side preset format and CLI to script, store, and replay sequences using the serial protocol (absolute positions with optional speed/accel), enabling sharable demos. Depends on: 1, 4. `S`
8. [ ] ESP‑Now Master/Node Broadcast — Add a master ESP32 that broadcasts protocol‑compatible commands (no acks/retries, no encryption initially). Support node IDs and group addressing. Depends on: 1, 2. `L`
9. [ ] Master‑Side Scheduler (Thermal/Current Batching) — Implement a master scheduler that batches `MOVE` commands to respect current limits and thermal budgets, using node‑reported telemetry and configurable policies. Provides dry‑run checks and staggered dispatch. Depends on: 4, 8. `M`
10. [ ] Multi‑Cluster Dispatch — Batch and sequence broadcasts to multiple nodes without a shared timebase, focusing on dependable arrival and master‑side scheduling rather than exact frame sync. Validate latency/jitter under load. Depends on: 8, 9. `M`
11. [ ] Targeting Geometry Core — Provide a shared math library and CLI to convert wall coordinates into mirror angles with reachability checks; feeds presets and live play. Depends on: 7. `M`
12. [ ] Creative Tooling: Live Play & Sandbox — Expose minimal live‑play hooks (controllers/sensors/OSC) while honoring scheduler limits, and add a desktop/browser sandbox to preview reachability and timing before hardware runs. Depends on: 9, 11. `M`

> Notes
>
> - Includes 12 items ordered by control surface → hardware bring‑up → diagnostics → limits → presets → wireless broadcast → master scheduling → scaling → geometry → creative tooling.
> - Each item delivers an end‑to‑end, testable outcome without requiring repo bootstrapping.
> - Effort scale: `S` 2–3 days, `M` ~1 week, `L` ~2 weeks.

> Resolved Decisions
>
> - Driver & stepping: DRV8825, full‑step only for v1; DIR is latched at motion start and not toggled mid‑move.
> - Homing: Bump‑stop algorithm with parameters `overshoot`, `backoff`, `speed`, `accel`, `full_range`; compute midpoint as zero at `full_range/2`. Parameters default in code and may be overridden via `HOME`.
> - Commands: `MOVE` uses absolute steps with optional `speed` (steps/s) and `accel` (steps/s^2); MOVE auto‑WAKEs before stepping and auto‑SLEEPs after completion. `WAKE`/`SLEEP` accept `<id|ALL>`. `HELP` lists all commands and parameters.
> - Budgets: Electrical budget not enforced on the node (master responsibility). Thermal budget must be enforced on both master and node; node uses a higher threshold. Start with lab‑safe defaults and refine later.
> - ESP‑Now: Start with broadcast, no acks/retries and no encryption; add reliability/security later if needed.
> - Multi‑cluster: Prefer batched dispatch over shared timebase; master sequencing is sufficient for our non‑animation use case.
> - Scaling: Motors can share STEP frequency (same speed), but have independent DIR and target step counts; stop per motor by gating its STEP (not SLEEP/ENBL) once its count completes.

> Ordering Rationale
>
> - 1 first to define and test the command surface quickly (with stubs) so validation and tooling can proceed in parallel.
> - 2 integrates hardware bring‑up behind the stable serial interface; includes per‑motor ENABLE and auto‑WAKE/SLEEP to protect motors from overheating.
> - 3–5 add reliability, observability, and protection so later phases have stable baselines and quick debugging.
> - 7 enables sharable demos early without waiting for wireless.
> - 8 introduces wireless control; establishes addressing/grouping for nodes.
> - 9 adds master‑side scheduling for safe batching once broadcast control exists.
> - 10 extends to multi‑node dispatch tuned for our non‑animation use case.
> - 6 isolates scaling risks early; shared STEP strategy is investigated to inform PCB and pin planning.
> - 11–12 bring back the original geometry and creative tooling goals, aligned with the new control architecture.
