# Product Roadmap

1. [ ] Serial Command Protocol v1 — Implement human‑readable commands over USB serial with stub backend for early testing: `HELP`, `MOVE <id|ALL> <abs_steps> [speed steps/s] [accel steps/s^2]` (auto‑WAKE before move, auto‑SLEEP after), `HOME [id|ALL] [overshoot] [backoff] [speed] [accel] [full_range]`, `STATUS`, `WAKE [id|ALL]`, `SLEEP [id|ALL]`. Return `OK`/`ERR` with codes. Provide a small CLI/Python tool for end‑to‑end tests. `S`
2. [ ] ESP32 8‑Motor Bring‑Up (FastAccelStepper + 74HC595) — Drive eight DRV8825 steppers in full‑step mode from one ESP32 using eight STEP pins and two daisy‑chained 74HC595s for per‑motor DIR and ENABLE. Implement auto‑WAKE/SLEEP semantics in firmware (ENABLE just‑in‑time before motion; DISABLE immediately after completion) to avoid overheating while overdriving. Latch DIR at motion start only, validate concurrent motion and stable SR timing. Integrate with Serial v1. Depends on: 1. `M`
3. [ ] Homing & Baseline Calibration (Bump‑Stop) — Implement bump‑stop homing without switches: drive toward an end for `full_range + overshoot` steps, back off `backoff` steps, then move `full_range/2` to midpoint and set zero. Parameters have safe defaults and can be overridden via `HOME` (order: overshoot, backoff, speed, accel, full_range). Enforce slow, conservative speed during HOME (defaults may be overridden). Depends on: 2. `M`
4. [ ] Status & Diagnostics — Extend `STATUS` to report per‑motor position, moving/sleeping, homed flag, recent fault codes, and thermal counters; include a concise summary view in the CLI. Depends on: 2. `S`
5. [ ] Preset Replay (MVP) — Add a minimal host‑side preset format and CLI to script, store, and replay sequences using the serial protocol (absolute positions with optional speed/accel), enabling sharable demos. Depends on: 1, 4. `S`
6. [ ] Master‑Side Scheduler (Thermal/Current Batching) — Implement a master scheduler that batches `MOVE` commands to respect current limits and thermal budgets, using node‑reported telemetry and configurable policies. Provides dry‑run checks and staggered dispatch. Depends on: 4. `M`
7. [ ] ESP‑Now Master/Node Broadcast — Add a master ESP32 that broadcasts protocol‑compatible commands (no acks/retries, no encryption initially). Support node IDs and group addressing. Depends on: 1, 2. `L`
8. [ ] Multi‑Cluster Dispatch — Batch and sequence broadcasts to multiple nodes without a shared timebase, focusing on dependable arrival and master‑side scheduling rather than exact frame sync. Validate latency/jitter under load. Depends on: 7, 6. `M`
9. [ ] 16+ Motors per ESP32 (R&D) — Evaluate FastAccelStepper vs. custom pulse gen to share STEP frequency across motors with identical speeds while gating per‑motor steps via ENABLE when targets complete; support independent DIR and different step counts. Document trade‑offs and migration path. Depends on: 2, 4. `M`
10. [ ] Targeting Geometry Core — Provide a shared math library and CLI to convert wall coordinates into mirror angles with reachability checks; feeds presets and live play. Depends on: 5. `M`
11. [ ] Creative Tooling: Live Play & Sandbox — Expose minimal live‑play hooks (controllers/sensors/OSC) while honoring scheduler limits, and add a desktop/browser sandbox to preview reachability and timing before hardware runs. Depends on: 6, 10. `M`

> Notes
> - Includes 11 items ordered by control surface → hardware bring‑up → diagnostics → presets → master scheduling → wireless broadcast → scaling → geometry → creative tooling.
> - Each item delivers an end‑to‑end, testable outcome without requiring repo bootstrapping.
> - Effort scale: `S` 2–3 days, `M` ~1 week, `L` ~2 weeks.

> Resolved Decisions
> - Driver & stepping: DRV8825, full‑step only for v1; DIR is latched at motion start and not toggled mid‑move.
> - Homing: Bump‑stop algorithm with parameters `overshoot`, `backoff`, `speed`, `accel`, `full_range`; compute midpoint as zero at `full_range/2`. Parameters default in code and may be overridden via `HOME`.
> - Commands: `MOVE` uses absolute steps with optional `speed` (steps/s) and `accel` (steps/s^2); MOVE auto‑WAKEs before stepping and auto‑SLEEPs after completion. `WAKE`/`SLEEP` accept `<id|ALL>`. `HELP` lists all commands and parameters.
> - Budgets: Electrical budget not enforced on the node (master responsibility). Thermal budget must be enforced on both master and node; node uses a higher threshold. Start with lab‑safe defaults and refine later.
> - ESP‑Now: Start with broadcast, no acks/retries and no encryption; add reliability/security later if needed.
> - Multi‑cluster: Prefer batched dispatch over shared timebase; master sequencing is sufficient for our non‑animation use case.
> - Scaling: Motors can share STEP frequency (same speed), but have independent DIR and target step counts; stop per motor via ENABLE gating when its count completes.

> Ordering Rationale
> - 1 first to define and test the command surface quickly (with stubs) so validation and tooling can proceed in parallel.
> - 2 integrates hardware bring‑up behind the stable serial interface; includes per‑motor ENABLE and auto‑WAKE/SLEEP to protect motors from overheating.
> - 3–4 add reliability and observability so later phases have stable baselines and quick debugging.
> - 5 enables sharable demos early without waiting for wireless.
> - 6 adds master‑side scheduling for safe batching before introducing wireless.
> - 7–8 introduce wireless control and multi‑node dispatch tuned for our non‑animation use case.
> - 9 isolates scaling risks; shared STEP strategy is investigated to inform PCB and pin planning.
> - 10–11 bring back the original geometry and creative tooling goals, aligned with the new control architecture.
