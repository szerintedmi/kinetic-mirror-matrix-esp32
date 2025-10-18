We're continuing our implementation of Status & Diagnostics by implementing task group number 2:

## Implement this task and its sub-tasks:

### Host Tooling (serial_cli.py)

#### Task Group 2: Interactive TUI with 2 Hz STATUS
Assigned implementer: ui-designer
Dependencies: Task Group 1
Standards: `@agent-os/standards/frontend/serial-interface.md`, `@agent-os/standards/testing/build-validation.md`

- [ ] 2.0 Complete interactive mode in existing CLI
  - [ ] 2.1 Write 2–6 focused tests
    - Argument parsing for `interactive` subcommand and options
    - Table rendering helper given sample STATUS lines (pure function; no serial)
    - Target 4 tests; run ONLY these tests for this group
  - [ ] 2.2 Add `interactive` subcommand to `tools/serial_cli.py`
    - Options: `--port`, `--baud`, `--timeout`, `--rate` (default ~2 Hz)
- [ ] 2.3 Implement TUI loop (Textual-based)
    - Non‑blocking input line for commands (`HELP`, `MOVE`, `HOME`, `WAKE`, `SLEEP`)
    - Poll STATUS at configured rate; render compact fixed‑width table
    - Show command responses in a log pane without breaking the table
  - [ ] 2.4 Robust serial I/O
    - Synchronize writes/reads; do not stall refresh while awaiting responses
    - Handle disconnects cleanly; show reconnect hints
  - [ ] 2.5 Ensure CLI tests pass
    - Run ONLY tests from 2.1

## Understand the context

Read @agent-os/specs/2025-10-17-status-diagnostics/spec.md to understand the context for this spec and where the current task fits into it.

## Perform the implementation

Implement all tasks assigned to you in your task group.

Focus ONLY on implementing the areas that align with areas of specialization (your "areas of specialization" are defined above).

Guide your implementation using:
- The existing patterns that you've found and analyzed.
- User Standards & Preferences which are defined below.

Self-verify and test your work by:
- Running ONLY the tests you've written (if any) and ensuring those tests pass.
- Manually run the TUI to verify refresh and interactions.

## Update tasks.md task status

In the current spec's `tasks.md` find YOUR task group that's been assigned to YOU and update this task group's parent task and sub-task(s) checked statuses to complete for the specific task(s) that you've implemented.

Mark your task group's parent task and sub-task as complete by changing its checkbox to `- [x]`.

DO NOT update task checkboxes for other task groups that were NOT assigned to you for implementation.

## Document your implementation

Using the task number and task title that's been assigned to you, create a file in the current spec's `implementation` folder called `[task-number]-[task-title]-implementation.md`.

Use the following structure for the content of your implementation documentation:

```markdown
## Summary
- What you implemented in serial_cli.py

## Changes
- Argument parsing additions
- Rendering helpers
- TUI loop structure and non-blocking input

## Testing
- Unit tests for parsing and table formatting
- Manual test steps and expected behavior
```

## User Standards & Preferences Compliance

IMPORTANT: Ensure that your implementation work is ALIGNED and DOES NOT CONFLICT with the user's preferences and standards as detailed in the following files:

@agent-os/standards/global/coding-style.md
@agent-os/standards/global/conventions.md
@agent-os/standards/frontend/serial-interface.md
@agent-os/standards/testing/build-validation.md
