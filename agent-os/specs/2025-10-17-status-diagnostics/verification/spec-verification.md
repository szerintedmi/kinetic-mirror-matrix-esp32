# Spec Verification Report — Status & Diagnostics

Spec path: `agent-os/specs/2025-10-17-status-diagnostics`

## Basic Structural Verification

### Check 1: Requirements Accuracy
- Source: `agent-os/specs/2025-10-17-status-diagnostics/planning/requirements.md`
- Findings:
  - Keep a single STATUS verb; extend with new fields: ✅ captured
  - Add `steps_since_home` for drift-related rehoming later: ✅ captured
  - Focus only on thermal/runtime metrics now (no fault counters): ✅ captured
  - Per-motor thermal metrics only; in-memory only (no persistence): ✅ captured
  - CLI output as compact table; no summary line initially: ✅ captured
  - Positions remain in steps (no degrees): ✅ captured
  - 2 Hz polling cadence acceptable: ✅ captured
  - STATUS returns all motors (no new command variants): ✅ captured
  - No enforcement/limiting in this item; metrics collection only: ✅ captured
  - Thermal model parameters and logic (90 s run, 60 s full cooldown, spend/refill rates): ✅ captured

Conclusion: Requirements reflect the user Q&A. Minor alignment note below (decimal formatting and key order evolved during spec discussion).

### Check 2: Visual Assets
- Folder: `agent-os/specs/2025-10-17-status-diagnostics/planning/visuals/`
- Files found: none
- Requirements mention visuals: N/A (none present)

## Content Validation (Checks 3–7)

### Check 3: Visual Design Tracking
- Not applicable (no visual assets present).

### Check 4: Requirements Coverage
- Explicit features requested: STATUS field extensions (`homed`, `steps_since_home`, thermal budget metrics), 2 Hz host-side view, in-memory-only metrics.
- Constraints stated: integer/low-precision time output, low overhead, steps-only units, no new commands.
- Out-of-scope items: enforcement/limiting, persistence, JSON output, degrees.
- Reusability opportunities: reuse existing firmware STATUS plumbing and Python CLI; documented implicitly via references to `MotorCommandProcessor` and `serial_cli.py` in spec.
- Implicit needs: consistent per-motor bookkeeping, event-driven updates, clear CLI defaults.

Coverage assessment: ✅ Items are represented across requirements, spec.md, and tasks.md. See minor alignment notes.

### Check 5: Core Specification Validation
- Goal alignment: ✅ Matches diagnostics and host-side view objective.
- User stories: ✅ Relevant to operators and developers.
- Core requirements: ✅ Only features requested (no additional fault counters or persistence added).
- Out of scope: ✅ Matches requirements (no enforcement, no JSON, no degrees).
- Reusability notes: ✅ References firmware/controller/CLI components to leverage.

Issues (minor):
- Requirements list shows `budget_s`/`ttfc_s` as integers; spec clarifies up to 1 decimal place for time fields. Recommend updating requirements to note 1-decimal formatting.
- Requirements state “append fields”; later preference allows reordering keys for readability. Recommend updating requirements to reflect reordering allowed.
- Host CLI integration: spec moves “watch” into `serial_cli.py` interactive mode; requirements mention a table at 2 Hz but not explicitly the interactive consolidation. Recommend adding a line in requirements for interactive mode in `serial_cli.py`.

### Check 6: Task List Detailed Validation
- File: `agent-os/specs/2025-10-17-status-diagnostics/tasks.md`
- Test Writing Limits:
  - ✅ TG1: 2–8 Unity tests
  - ✅ TG2: 2–6 Python tests for parsing/render helpers
  - ✅ TG3: 3–4 assertions extending existing formatting tests
  - ✅ TG4: ≤ 8 additional tests
  - ✅ Tasks instruct running ONLY the newly written tests per group
- Reusability References:
  - ✅ TG2 explicitly extends existing `serial_cli.py`
  - ✅ Firmware tasks extend existing `MotorState`/STATUS path
- Specificity & Traceability:
  - ✅ Tasks map directly to requirements and spec fields (`homed`, `steps_since_home`, `budget_s`, `ttfc_s`)
- Visual References:
  - N/A (no visuals)
- Task Count:
  - ✅ Balanced scope, no over-engineering flags

### Check 7: Reusability and Over‑Engineering
- Unnecessary new components: ❌ None (interactive mode added to existing CLI, no new tool).
- Duplicated logic: ❌ None indicated.
- Missing reuse: ✅ Existing protocol/CLI components referenced and extended.

## Critical Issues
- None blocking implementation identified.

## Minor Issues
1. Requirements should be updated to allow 1-decimal formatting for time fields and to allow key reordering for readability.
2. Requirements should explicitly mention integrating the 2 Hz view into `serial_cli.py` interactive mode (for clarity).

## Over‑Engineering Concerns
- None. Scope aligns with roadmap item and remains focused.

## Recommendations
1. Amend requirements.md to reflect 1-decimal time formatting and key reordering allowance.
2. Add a line noting “serial_cli.py interactive mode provides the 2 Hz table and accepts commands concurrently.”
3. In tests, include a basic invariant for `ttfc_s` ≥ 0 and `budget_s` ∈ [0,90].

## Conclusion
Ready for implementation. Spec, requirements, and tasks are aligned with user inputs; minor documentation syncs recommended before or alongside implementation.

*** End of Report ***

