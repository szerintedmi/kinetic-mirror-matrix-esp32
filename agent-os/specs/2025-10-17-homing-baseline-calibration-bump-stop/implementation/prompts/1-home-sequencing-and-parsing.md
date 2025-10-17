We're continuing our implementation of Homing & Baseline Calibration (Bump‑Stop) by implementing task group number 1:

## Implement this task and its sub-tasks:

#### Task Group 1: HOME sequencing and parsing
Assigned implementer: api-engineer
Dependencies: None
Standards: `@agent-os/standards/backend/hardware-abstraction.md`, `@agent-os/standards/testing/unit-testing.md`, `@agent-os/standards/global/conventions.md`

- [ ] 1.0 Complete HOME implementation end‑to‑end
  - [ ] 1.1 Write 4–6 focused unit tests (native)
    - Parsing: `HOME:<id|ALL>[,<overshoot>][,<backoff>][,<speed>][,<accel>][,<full_range>]` with comma‑skips (e.g., `HOME:2,,50`)
    - Busy rule: reject HOME when any targeted motor is already moving
    - Concurrency: `HOME:ALL` starts all eight with the stub backend (no deadlocks)
    - Post‑state: after simulated completion, `position==0`, `moving==0`, `awake==0` for targeted motors
    - HELP includes HOME grammar line
  - [ ] 1.2 Implement controller HOME
    - In `HardwareMotorController::homeMask`: orchestrate negative run (full_range+overshoot), backoff, center to soft midpoint, then `setCurrentPosition(0)`; ignore soft limits during HOME
    - Use absolute targets computed from current positions if only abs moves are available; otherwise relative moves are fine
    - Mirror MOVE semantics for auto‑WAKE/SLEEP and WAKE override handling
  - [ ] 1.3 Align defaults and parsing
    - Ensure MOVE defaults apply to HOME: speed=4000, accel=16000
    - Derive `full_range = kMaxPos - kMinPos` (currently 2400)
  - [ ] 1.4 Ensure unit tests pass
    - Run ONLY the tests from 1.1

Acceptance Criteria
- 4–6 unit tests from 1.1 pass (parsing, busy rule, concurrency, post‑state, HELP grammar)
- HOME command sets runtime zero and ignores normal soft limits during its sequence
- WAKE/SLEEP behavior matches MOVE semantics

### Build & Bench Validation

#### Task Group 2: Build validation and homing bench checks

## Understand the context

Read @agent-os/specs/2025-10-17-homing-baseline-calibration-bump-stop/spec.md to understand the context for this spec and where the current task fits into it.

## Perform the implementation

Implement all tasks assigned to you in your task group.

Focus ONLY on implementing the areas that align with areas of specialization (your "areas of specialization" are defined above).

Guide your implementation using:
- The existing patterns that you've found and analyzed.
- User Standards & Preferences which are defined below.

Self-verify and test your work by:
- Running ONLY the tests you've written (if any) and ensuring those tests pass.

## Update tasks.md task status

In the current spec's `tasks.md` find YOUR task group that's been assigned to YOU and update this task group's parent task and sub-task(s) checked statuses to complete for the specific task(s) that you've implemented.

Mark your task group's parent task and sub-task as complete by changing its checkbox to `- [x]`.

DO NOT update task checkboxes for other task groups that were NOT assigned to you for implementation.

## Document your implementation

Using the task number and task title that's been assigned to you, create a file in the current spec's `implementation` folder called `[task-number]-[task-title]-implementation.md`.

Use the following structure for the content of your implementation documentation:

```
## Summary
[Short summary of what you implemented]

## Files Touched
- `path/to/file1` - [Short description]
- `path/to/file2` - [Short description]

## Implementation Details

### [Component/Feature 1]
**Location:** `path/to/file.ext`
[Detailed explanation]
**Rationale:** [Why this approach]

### [Component/Feature 2]
**Location:** `path/to/file.ext`
[Detailed explanation]
**Rationale:** [Why this approach]

## Testing

### Test Files Created/Updated
- `path/to/test` - [What is being tested]

### Test Coverage
- Unit tests: [status]
- Integration tests: [status]
- Edge cases covered: [list]

### Manual Testing Performed
[Steps and results]

## User Standards & Preferences Compliance
[List specific standards followed and how]

## Known Issues & Limitations
[List any]
```

## User Standards & Preferences Compliance

@agent-os/standards/global/coding-style.md
@agent-os/standards/global/conventions.md
@agent-os/standards/global/platformio-project-setup.md
@agent-os/standards/global/resource-management.md
@agent-os/standards/backend/data-persistence.md
@agent-os/standards/backend/hardware-abstraction.md
@agent-os/standards/backend/task-structure.md
@agent-os/standards/testing/build-validation.md
@agent-os/standards/testing/hardware-validation.md
@agent-os/standards/testing/unit-testing.md
