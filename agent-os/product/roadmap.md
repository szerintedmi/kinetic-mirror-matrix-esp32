# Product Roadmap

1. [x] Serial Command Protocol v1 — Implement human‑readable commands over USB serial with stub backend for early testing: `HELP`, `MOVE:<id|ALL>,<abs_steps>[,<speed>][,<accel>]` (auto‑WAKE before move, auto‑SLEEP after), `HOME:<id|ALL>[,<overshoot>][,<backoff>][,<speed>][,<accel>][,<full_range>]`, `STATUS`, `WAKE:<id|ALL>`, `SLEEP:<id|ALL>`. Return `CTRL:OK` / `CTRL:ERR <code> ...`. Provide a small Python CLI for end‑to‑end tests; include shortcuts `M`/`H`/`ST`. `S`
2. [x] ESP32 8‑Motor Bring‑Up (FastAccelStepper + 74HC595) — Drive eight DRV8825 steppers in full‑step mode from one ESP32 using eight STEP pins and two daisy‑chained 74HC595s for per‑motor DIR and SLEEP (ENABLE semantics). Implement auto‑WAKE/SLEEP in firmware via FastAccelStepper external‑pin callback (enable just‑in‑time before motion; disable immediately after completion). Latch DIR at motion start only, validate concurrent motion and stable SR timing. Integrate with Serial v1. Depends on: 1. `M`
3. [x] Homing & Baseline Calibration (Bump‑Stop) — Implement bump‑stop homing without switches: drive toward an end for `full_range + overshoot` steps, back off `backoff` steps, then move `full_range/2` to midpoint and set zero. Parameters have safe defaults and can be overridden via `HOME` (order: overshoot, backoff, speed, accel, full_range). Enforce slow, conservative speed during HOME (defaults may be overridden). Depends on: 2. `M`
4. [x] Status & Diagnostics — Extend `STATUS` to report per‑motor position, moving/sleeping, homed flag, steps since last home, and thermal runtime metrics (`budget_s`, `ttfc_s`). Add a concise summary view in the host CLI with an interactive mode that polls STATUS (~2 Hz). Expose `GET LAST_OP_TIMING[:<id|ALL>]` for device‑side timing when validating estimates. Depends on: 2. `S`
5. [x] Thermal Limits Enforcement — Enforce per‑motor runtime/cooldown budgets (derived from STATUS metrics) to cap continuous motion and require cooldown before resuming. Thresholds are compile‑time constants for now; enforcement can be toggled at runtime via `SET THERMAL_LIMITING=OFF|ON` and queried with `GET THERMAL_LIMITING`. MOVE/HOME return `est_ms` in `CTRL:OK`, and pre‑flight checks reject or warn when budgets are exceeded. Depends on: 4. `S`
6. [x] Shared-STEP spike for 8 motors on one ESP32 to prep scaling beyond with a single MCU. backend with a hardware-timed shared STEP generator,  DIR/SLEEP on 74HC595, and layered per-motor gating/guard windows so eight motors run in lockstep while we validate patterns for future >8 builds.  Depends on: 2, 4. `S`
   - Status: Parked (2025-10-25). See shared-step status note: [docs/sharedstep-status.md](./docs/sharedstep-status.md)
7. [x] Wi‑Fi Onboarding via NVS — Provide SoftAP onboarding for SSID/PSK, persist credentials in NVS Preferences, and auto‑connect on boot. Adds `/api/status` identity fields, `/api/reset`, AP-only scanning, serial `NET:LIST` guard, and mode-aware portal UX with reset guidance. Acceptance: cold boot → AP provision → joins LAN; reset paths verified. Depends on: none. `S`
8. [x] Command Pipeline Modularization — Break the legacy MotorCommandProcessor into reusable parsing/router/handler layers so Serial and upcoming MQTT transports share validation, CID sequencing, and response formatting. Reason: de-risk transport parity and cut integration friction before we add a second control plane. Depends on: 7. `S`
9. [x] MQTT Steel Thread: Presence — Connect to a local broker with username/password; publish retained birth message and LWT on `devices/<id>/state`. Acceptance: node publishes “ready” immediately after connect; CLI shows online/offline flips within a few milliseconds on a local broker (target <25 ms median, <100 ms p95). Depends on: 8. `S`
10. [x] Telemetry via MQTT (STATUS Parity) — Publish an aggregate JSON snapshot on `devices/<id>/status` containing node state, IP, and the full set of serial `STATUS` fields for every motor, replacing the prior retained `devices/<id>/state` presence message. Maintain 1 Hz idle / 5 Hz motion cadence with change suppression so traffic stays lean while CLI/TUI defaults to MQTT transport for live status views; serial remains selectable as a debug backdoor. Acceptance: aggregated MQTT snapshots match serial `STATUS` across ≥2 motors during motion, offline transitions surface via the broker will payload on the same topic, and the TUI reflects updates within a few milliseconds on a local broker. Depends on: 9. `S`
11. [ ] Commands via MQTT — Implement command request/response with strict `cmd_id` correlation (no QoS2) and full feature parity with the current serial command set (all actions and parameters). Requests: `devices/<id>/cmd` (QoS1). Responses: `devices/<id>/cmd/resp` (QoS1) with `{ ok|err, est_ms }`. Node does not queue; if busy, immediately replies `BUSY`. CLI/TUI defaults to MQTT transport (serial selectable via `--transport serial`). Acceptance: every serial command and parameter works end‑to‑end over MQTT with correlated acks; concurrent `MOVE` rejected with `BUSY`; events/status reflect progress. Depends on: 10. `M`
12. [ ] Multi‑Node Basics + Registry — Master maintains a live registry from retained `state`; broadcast simple commands to selected nodes. Acceptance: two nodes visible; broadcast `HOME` executes on both with distinct acks. Depends on: 11. `S`
13. [ ] Geometry & Mapping — Master converts pattern targets to per‑motor angles/steps; persist mirror layout (motor‑pair→mirror, arrangement), homing offsets, and reachability limits. Storage mechanism TBD (not necessarily a file). Acceptance: choose a simple pattern in the TUI; nodes move to derived angles respecting limits. Depends on: 12. `M`
14. [ ] Preset Replay (MVP) — Store and play sequences of high‑level targets on the master; pause/stop; progress via events. Acceptance: canned multi‑step demo plays on ≥2 nodes; resumes after transient broker disconnect. Depends on: 13 (or 12 for raw commands first). `S`
15. [ ] Master Scheduler v1 (Thermal/Current Batching) — Stagger `MOVE`s by node busy state and thermal headroom from telemetry; provide dry‑run preview in CLI. Acceptance: batch runs without `BUSY` rejections; thermal warnings avoided. Depends on: 14, 10. `M`
16. [ ] Broker Strategy & Deployment — MVP uses a developer‑machine broker and MQTT UI; later package for a site gateway (container/systemd), with runbooks and backup/restore. Acceptance: documented runbooks; CLI flags to target broker host. Depends on: 9–12. `S`

> Notes
>
- Now includes 16 items ordered by control surface → hardware bring‑up → diagnostics → limits → network onboarding → transport unification → presence → telemetry → commands → multi‑node → geometry → presets → scheduling → deployment.
> - Each item delivers an end‑to‑end, testable outcome without requiring repo bootstrapping.
> - Effort scale: `S` 2–3 days, `M` ~1 week, `L` ~2 weeks.

> Resolved Decisions
>
> - Driver & stepping: DRV8825, full‑step only for v1; DIR is latched at motion start and not toggled mid‑move.
> - Homing: Bump‑stop algorithm with parameters `overshoot`, `backoff`, `speed`, `accel`, `full_range`; compute midpoint as zero at `full_range/2`. Parameters default in code and may be overridden via `HOME`.
> - Commands: `MOVE` uses absolute steps with optional `speed` (steps/s) and `accel` (steps/s^2); MOVE auto‑WAKEs before stepping and auto‑SLEEPs after completion. `WAKE`/`SLEEP` accept `<id|ALL>`. `HELP` lists all commands and parameters. Transport parity: all serial commands and parameters are available over MQTT with a 1:1 field mapping; new serial actions must be mirrored on MQTT.
> - Budgets: Electrical budget not enforced on the node (master responsibility). Thermal budget must be enforced on both master and node; node uses a higher threshold. Start with lab‑safe defaults and refine later.
> - Networking: MQTT is the primary control plane. Aggregate status + presence publishes on `devices/<id>/status` (QoS0 live snapshots with an offline Last Will on the same topic); commands `devices/<id>/cmd` QoS1 with strict `cmd_id` correlation; responses `devices/<id>/cmd/resp` QoS1. No QoS2.
> - Wi‑Fi credentials: stored in NVS Preferences; onboarding via SoftAP; resets via serial `NET:RESET` and long‑press button.
- CLI/TUI transport: defaults to MQTT from item 10 onward; serial remains a selectable debug/diagnostic path.
> - Node queuing: none; nodes reject commands with `BUSY` while executing; master owns scheduling.
> - Geometry & storage: defined on the master; storage mechanism TBD (do not assume file‑based yet).
> - Broker location: developer machine/local broker for MVP; site gateway packaging later.
> - Multi‑node: Prefer batched dispatch via master scheduling over shared timebase.
> - Scaling: Motors can share STEP frequency (same speed), but have independent DIR and target step counts; stop per motor by gating its STEP (not SLEEP/ENBL) once its count completes.

> Ordering Rationale
>
> - 1 defines and tests the serial command surface quickly; 2 integrates hardware bring‑up behind a stable interface; 3–5 add reliability, observability, and protection.
- 7 delivers Wi‑Fi onboarding; 8 modularizes the command pipeline ahead of additional transports.
- 9 establishes MQTT presence; 10 provides STATUS parity so the TUI becomes useful without commanding.
- 11 unlocks full control via MQTT with clear request/response semantics and `cmd_id` correlation.
- 12 enables multi‑node operations; 13 adds geometry mapping for target conversion; 14 delivers a demo‑ready preset flow.
- 15 adds master scheduling informed by real telemetry.
- 6 isolates scaling risks early; 16 formalizes broker deployment when needed.
