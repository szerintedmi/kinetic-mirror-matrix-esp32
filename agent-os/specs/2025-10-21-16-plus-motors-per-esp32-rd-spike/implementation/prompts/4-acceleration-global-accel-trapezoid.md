We're continuing our implementation of 16+ Motors per ESP32 (Shared STEP Spike) by implementing task group number 4:

## Implement this task and its sub-tasks:

#### Task Group 4: Acceleration (Global ACCEL + Trapezoid)
**Assigned implementer:** api-engineer
**Dependencies:** Task Group 3 (constant speed integrated; RMT ISR edges available)

- [ ] 4.0 Implement global acceleration support
  - [ ] 4.1 Add `GET ACCEL` / `SET ACCEL=<a>`; reject `SET ACCEL` while moving
  - [ ] 4.2 Add trapezoidal ramp scheduling in the shared STEP generator (period updates)
  - [ ] 4.3 Update estimates to use MotionKinematics with global `SPEED`/`ACCEL`
  - [ ] 4.4 Write 2-6 unit tests for ramp period scheduling and corner cases (short moves, accel-limited)
  - [ ] 4.5 CLI smoke (manual):
        - `SET SPEED=4000`, `SET ACCEL=16000`
        - `MOVE:0,800` → `CTRL:OK est_ms=...` then `GET LAST_OP_TIMING:0` to compare
        - While moving, `SET ACCEL=8000` → error (reject while moving)

## Understand the context

Read @agent-os/specs/2025-10-21-16-plus-motors-per-esp32-rd-spike/spec.md to understand the context for this spec and where the current task fits into it.

## Perform the implementation

Implement all tasks assigned to you in your task group.

Focus ONLY on implementing the areas that align with **areas of specialization** (your "areas of specialization" are defined above).

Guide your implementation using:
- **The existing patterns** that you've found and analyzed.
- **User Standards & Preferences** which are defined below.

Self-verify and test your work by:
- Running ONLY the tests you've written (if any) and ensuring those tests pass.
- Use the CLI smoke steps above to verify behavior end-to-end.
- Ensure guard scheduling remains aligned when periods change mid-motion.

## Update tasks.md task status

Update this task group's checkboxes in `tasks.md` upon completion of sub-tasks you implemented.

## Document your implementation

Create: `agent-os/specs/2025-10-21-16-plus-motors-per-esp32-rd-spike/implementation/4-acceleration-implementation.md`

Structure: see template in implement-spec guide.

## User Standards & Preferences Compliance

@agent-os/standards/global/coding-style.md
@agent-os/standards/global/conventions.md
@agent-os/standards/global/platformio-project-setup.md
@agent-os/standards/global/resource-management.md
@agent-os/standards/backend/data-persistence.md
@agent-os/standards/backend/hardware-abstraction.md
@agent-os/standards/backend/task-structure.md
@agent-os/standards/testing/unit-testing.md
@agent-os/standards/testing/hardware-validation.md
@agent-os/standards/testing/build-validation.md

