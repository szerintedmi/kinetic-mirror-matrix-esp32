# Spec Verification Report — Homing & Baseline Calibration (Bump‑Stop)

Spec path: `agent-os/specs/2025-10-17-homing-baseline-calibration-bump-stop`

## Basic Structural Verification

### Check 1: Requirements Accuracy
- Source: `agent-os/specs/2025-10-17-homing-baseline-calibration-bump-stop/planning/requirements.md`
- Findings:
  - Direction default negative: ✅ captured
  - Defaults: overshoot=800, backoff=150; speed=4000; accel=16000; full_range derived from MOVE soft range (kMaxPos − kMinPos): ✅ captured
  - Concurrency: HOME ALL runs concurrently (like MOVE ALL): ✅ captured
  - Auto‑WAKE/SLEEP mirrors MOVE and respects manual WAKE: ✅ captured
  - No persistence (set FAS runtime zero only): ✅ captured
  - Parameter order `[overshoot, backoff, speed, accel, full_range]`, commas to skip: ✅ captured with example
  - No extra safety clamp during HOME: ✅ captured (ignore soft limits during HOME)
  - Homed flag/STATUS deferred to roadmap item 4: ✅ captured (no STATUS changes)
  - Same defaults for all motors, no per‑motor config: ✅ captured
  - Out‑of‑scope: no endstops, stall/current sensing, persistence, auto‑retries: ✅ captured
  - Existing code to reference (prototype StepperMotor.hpp/.cpp): ✅ documented

Conclusion: Requirements document accurately reflects user Q&A and notes.

### Check 2: Visual Assets
- Folder: `agent-os/specs/2025-10-17-homing-baseline-calibration-bump-stop/planning/visuals/`
- Files found: none
- Requirements mention visuals: N/A (none present)

## Content Validation (Checks 3–7)

### Check 3: Visual Design Tracking
- Not applicable (no visual assets present).

### Check 4: Requirements Coverage
- Explicit features requested: bump‑stop homing, negative direction default, concurrency for ALL, MOVE‑like WAKE/SLEEP, parameterized HOME with defaults and skip, no persistence.
- Constraints stated: ignore soft limits during HOME; rely on physical stop + current limiting; use MOVE defaults for speed/accel; full_range equals current soft range.
- Out‑of‑scope items: endstops, stall/current sensing, persistence, auto‑retries, STATUS changes.
- Reusability opportunities: reference prototype homing logic files.
- Implicit needs: ensure HOME uses relative motion segments or equivalent absolute computations; maintain busy rules consistent with MOVE.

Coverage assessment: ✅ All items are represented in spec.md and requirements.md.

### Check 5: Core Specification Validation
- Goal alignment: ✅ Spec goal aligns with homing baseline objective.
- User stories: ✅ Relevant (HOME id/ALL, param overrides, MOVE behavior parity).
- Core requirements: ✅ Only features agreed in requirements; no extras added.
- Out of scope: ✅ Matches requirements (no STATUS changes, no sensors, no persistence).
- Reusability notes: ✅ Mentions existing controller, adapter, and prototype references.

Issues: None.

### Check 6: Task List Detailed Validation
- File: `agent-os/specs/2025-10-17-homing-baseline-calibration-bump-stop/tasks.md`
- Test Writing Limits:
  - ✅ TG1 plans 4–6 focused unit tests (within 2–8 limit)
  - ✅ TG2 is a manual bench checklist (no additional unit tests)
  - ✅ Tasks instruct running ONLY the newly written tests for TG1
  - Note: Total planned tests <16 (approx. 4–6). Acceptable given firmware focus and single implementation group writing tests.
- Reusability References: 
  - ✅ Notes reference reusing existing parser/controller patterns and using the stub backend for tests
  - ⚠️ Consider explicitly referencing prototype files in TG1 notes for quick orientation (optional)
- Specificity & Traceability:
  - ✅ Tasks map directly to requirements: parsing, busy rule, concurrency, WAKE/SLEEP parity, final zero
- Scope & Visuals:
  - ✅ No out‑of‑scope tasks; no visuals to reference

### Check 7: Reusability and Over‑Engineering
- Unnecessary new components: ❌ None identified in spec.
- Duplicated logic: ❌ None indicated.
- Missing reuse: ✅ Spec references existing components (MotorCommandProcessor, HardwareMotorController, FAS adapter) and prototype code.

## Critical Issues
- None blocking implementation identified in spec.md or requirements.md.

## Minor Issues
- None.

## Over‑Engineering Concerns
- None. Spec remains focused and aligns with roadmap scope.

## Recommendations
1. Optional: In TG1 notes, add explicit reference to prototype homing files (`StepperMotor.hpp/.cpp`) for faster onboarding.
2. During bench, capture a brief log of HOME → STATUS states to document final zero and awake transitions.
3. Confirm HELP text includes the HOME grammar (covered in TG1 tests).

## Conclusion
Ready for implementation. Spec, requirements, and tasks are aligned with user inputs and standards. Test-writing limits are respected.

*** End of Report ***
