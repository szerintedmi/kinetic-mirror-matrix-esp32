We're continuing our implementation of 16+ Motors per ESP32 (Shared STEP Spike) by implementing task group number 3:

## Implement this task and its sub-tasks:

#### Task Group 3: Integration + Protocol Simplification (Constant Speed)
**Assigned implementer:** api-engineer
**Dependencies:** Task Groups 1-2

- [ ] 3.0 Integrate shared STEP into `HardwareMotorController` (constant speed only)
  - [ ] 3.1 Remove per-move/home `<speed>,<accel>` from parser and HELP output
  - [ ] 3.2 Add `GET SPEED` / `SET SPEED=<v>` (global); reject `SET SPEED` while any motor is moving
  - [ ] 3.3 Wire `MOVE`/`HOME` to shared STEP generator using global `SPEED`; ignore acceleration for now
  - [ ] 3.4 Maintain positions by counting pulses only while SLEEP is enabled per motor; start generator on first active motor, stop on last finish
  - [ ] 3.5 Enforce overlap rule: concurrent moves are allowed only at the single global `SPEED`
  - [ ] 3.6 Write 2-6 parser tests for simplified grammar and erroring on legacy params
  - [ ] 3.7 CLI smoke (manual): via `python -m serial_cli`
        - `GET SPEED` → defaults
        - `SET SPEED=4000` → `CTRL:OK`
        - `MOVE:0,200` then `STATUS` until `moving=0`
        - While moving, `SET SPEED=4500` → error (reject while moving)
        - `MOVE:0,200,4000` (legacy param) → `CTRL:ERR E03 BAD_PARAM`

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

## Update tasks.md task status

Update this task group's checkboxes in `tasks.md` upon completion of sub-tasks you implemented.

## Document your implementation

Create: `agent-os/specs/2025-10-21-16-plus-motors-per-esp32-rd-spike/implementation/3-integration-protocol-simplification-implementation.md`

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

