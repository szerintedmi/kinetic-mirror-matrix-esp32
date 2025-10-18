# Spec Requirements: Status & Diagnostics

## Initial Description
4. [ ] Status & Diagnostics — Extend `STATUS` to report per‑motor position, moving/sleeping, homed flag, recent fault codes, and thermal counters; include a concise summary view in the CLI. Depends on: 2. `S`

## Requirements Discussion

### First Round Questions

**Q1:** I assume we keep the current one-line-per-motor STATUS format and append fields rather than change existing ones. Is that correct, or should we introduce a new verb (e.g., STATUSX) to preserve backward compatibility?
**Answer:** keep one STATUS command and we extend it with new fields. no need to worry about backward compatibility, no consumers yet

**Q2:** I’m thinking we add homed=<0|1> where homed=1 means the last HOME completed successfully. Should this flag reset on reboot and after any MOVE before HOME, or persist across reboots?
**Answer:** We also need a step counter since last homing so later we can add logic for auto rehoming if drift occurs after a lot of steps

**Q3:** For “recent fault codes,” I assume a simple per-motor counter map (e.g., faults=OT:2,OC:0) is sufficient instead of a timestamped ring buffer to keep effort small. Is that correct, or do you want a bounded “last N” list?
**Answer:** Realized we don't need fault count for now, just collecting thermal metrics (run budget, see logic below) and displaying them. we will add runtime / thermal limiting in a later roadmap item building on these. we also introduce current budgeting in later roadmap item

**Q4:** For thermal diagnostics, I propose per-motor counters like therm=overtemp:count and a node-level thermal events total. Should these counters be session-only (reset on reboot) or persisted in LittleFS?
**Answer:** no need node level thermal event counter just per motor. nothing needs to be persisted for now, in-memory is enough

**Q5:** For CLI output, I’m assuming a concise summary line highlighting anomalies first, e.g., “OK: 7/8 homed; Issues: M3 homed=0, M5 faults=OT:2”. Would you prefer a compact table (fixed-width) or minimal single-line summary?
**Answer:** compact table for now, we tune it later. no need for summary either, just adding a few more columns to start with

**Q6:** I assume positions remain in steps (pos=<steps>) and we do not add degrees/units conversion in this item. Is that correct, or should the CLI optionally show a derived column (steps only by default)?
**Answer:** still only steps, we will add degrees once we introduce geometries

**Q7:** Polling: I propose the CLI defaults to 2 Hz refresh for summary, with a flag to run once. Is that cadence acceptable, or do you want a lower/higher default or an adaptive backoff?
**Answer:** 2Hz is fine

**Q8:** Scope of command surface: I suggest no new commands, just enriched STATUS output; the CLI will filter/summarize locally. Is that okay, or do you want a STATUS:<id|ALL> variant now?
**Answer:** current behaviour of STATUS returns all motors is fine

**Q9:** Are there any explicit exclusions you want for this iteration (e.g., no persistent storage, no timestamped histories, no JSON output mode)?
**Answer:** we don't need to enforce limits, just collect metrics for now, we will build our limits on these metrics later.

### Existing Code to Reference
No similar existing features identified for reference.

### Follow-up Questions
None at this time.

## Visual Assets

No visual assets provided.

## Requirements Summary

### Functional Requirements
- Extend `STATUS` (single verb) to include new per-motor fields (appended to existing order):
  - `homed=<0|1>`: 1 after successful HOME; session-only (resets to 0 on reboot).
  - `steps_since_home=<steps>`: cumulative absolute steps since last successful HOME; resets to 0 on HOME and on reboot.
  - Thermal budget metrics (session-only, not persisted):
    - `budget_s=<sec.dec>`: remaining runtime budget, displayed with 1 decimal. Starts at `MAX_RUNNING_TIME_S=90`. While ON, spend 1.0 s/s; while OFF, refill at 1.5 s/s. No clamp at 0; negative values indicate over‑budget.
    - `ttfc_s=<sec.dec>`: time-to-full-cooldown if turned OFF now, displayed with 1 decimal. Computed from the true deficit (includes overrun beyond zero) and capped for display at `MAX_COOL_DOWN_TIME_S=300`.
- Definitions for ON/OFF state:
  - ON = motor is awake (driver enabled). OFF = motor is asleep (driver disabled). Auto‑WAKE during motion counts as ON for spend; auto‑SLEEP after completion counts as OFF for refill.
- Keep all existing fields unchanged: `id`, `pos`, `speed`, `accel`, `moving`, `awake`.
- CLI: render a compact fixed‑width table at ~2 Hz showing all motors and the added columns; no summary line for now.
- STATUS continues to return all motors; no new commands are introduced.

### Non-Functional Requirements
- No persistence: All metrics are in-memory per boot.
- Low runtime overhead: Updates must not interfere with motion timing.
- Portable across stub and hardware controllers.
- CLI watch at 2 Hz default; single-shot still available.

### Reusability Opportunities
- Reuse existing `MotorCommandProcessor` formatting pattern (key=value fields per motor).
- Extend `MotorState` in controllers to track `homed`, `steps_since_home`, and thermal budget bookkeeping.

### Scope Boundaries
**In Scope:**
- STATUS field extensions per above.
- CLI table view to display new fields at 2 Hz.

**Out of Scope:**
- Enforcing runtime/thermal limits (new roadmap item will cover this).
- Fault counters and node-level metrics.
- Persistence or JSON output mode.
- Unit conversion to degrees.

### Technical Considerations
- Budget parameters (constants):
  - `MAX_RUNNING_TIME_S = 90`
  - `FULL_COOL_DOWN_TIME_S = 60`
  - `REFILL_RATE = MAX_RUNNING_TIME_S / FULL_COOL_DOWN_TIME_S = 1.5`
  - `MAX_COOL_DOWN_TIME_S = 300` (display clamp for `ttfc_s`)
- Budget accounting per motor:
  - Track internal budget in tenths (`budget_tenths`) and `last_update_ms`.
  - Apply spend/refill in whole-second increments based on elapsed time (carry partial seconds); update on controller `tick()` and again just before STATUS.
    - If ON: `budget_tenths -= 10 * elapsed_seconds`.
    - If OFF: `budget_tenths += 15 * elapsed_seconds`, capped at `BUDGET_TENTHS_MAX = MAX_RUNNING_TIME_S*10`.
  - Report `budget_s = budget_tenths / 10` with one decimal (can be negative).
  - Compute `ttfc_s` from the true deficit, but cap maximum cooldown at runtime:
    - Enforce `budget_tenths >= BUDGET_TENTHS_MAX - REFILL_TENTHS_PER_SEC * MAX_COOL_DOWN_TIME_S` while ON.
    - `ttfc_s = ceil((BUDGET_TENTHS_MAX - budget_tenths)/REFILL_TENTHS_PER_SEC)`, then min with `MAX_COOL_DOWN_TIME_S` for display.
- Steps since home:
  - Reset to 0 when HOME completes successfully.
  - Accumulate absolute step deltas as positions change, only when `homed=1`.
- Homed flag:
  - Set to 1 when HOME completes successfully; reset to 0 on reboot. Remains 1 across MOVEs until next HOME or reboot.
- Implementation hooks:
  - Update budgets in controller layer on `tick(now_ms)` and just-in-time before STATUS; identical semantics for stub and hardware.

### Memory & Performance
- Per-motor memory additions (approx.):
  - `homed` (1 byte/bitfield), `steps_since_home` (int32), `budget_balance_s` (fixed-point or int32), `last_update_ms` (uint32).
  - For 8 motors: well under 1 KB additional state.
- Computation strategy:
  - Event-driven updates on MOVE/HOME/WAKE/SLEEP and just-in-time before STATUS; no tight polling loops.
  - Use integer or fixed-point arithmetic (e.g., milliseconds or Q16.16) to avoid float overhead; represent refill as 3/2 to compute via integer math.
  - O(1) per-motor update; negligible CPU for 8 motors. Avoid dynamic allocations and large string formatting inside loops.
- Timing safety:
  - Ensure updates run outside high-frequency ISR contexts; if invoked from callbacks, keep work minimal or defer via a lightweight flag.
  - Guard shared state with brief critical sections or atomic access patterns to avoid tearing without impacting step timing.
