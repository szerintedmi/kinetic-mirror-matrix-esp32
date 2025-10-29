# Specification: Command Pipeline Modularization

## Goal
Establish a layered command-processing pipeline that replaces the monolithic `MotorCommandProcessor`, preserves all existing command semantics, and positions the firmware for future transports such as MQTT without duplicating logic.

## User Stories
- As a firmware maintainer, I want command parsing, routing, and execution isolated into small units so that adding or modifying verbs no longer risks regressions across unrelated behavior.
- As a test engineer, I want injectable collaborators for CID and timing so unit tests can cover edge cases without touching hardware.
- As a future MQTT implementer, I want a transport-agnostic entry point that returns structured results so new adapters can reuse the same command semantics.

## Core Requirements
### Functional Requirements
- Introduce a `CommandExecutionContext` that owns the single `MotorController` instance and exposes the minimal shared services (CID allocator, clock access, net onboarding helpers) needed by handlers; transports remain stateless.
- Replace file-scope helper functions with reusable utilities (e.g., trim, CSV parsing, ID mask parsing) in a dedicated `motor::command_utils` module so all transports share validation logic.
- Implement a `CommandParser` that converts raw lines into `ParsedCommand` objects, supporting single commands and batched payloads while reproducing today’s overlap detection, alias handling, and whitespace rules.
- Provide a `CommandRouter` that maps verbs to dedicated handler classes (`MotorCommandHandler`, `QueryCommandHandler`, `NetCommandHandler`, etc.) and delegates execution using the shared context.
- Add a `BatchExecutor` that replicates current multi-command semantics (BUSY checks, CID reuse, aggregated `est_ms`, warning propagation) without relying on global flags.
- Create a `CommandResult` structure (fields for CID, response lines, optional `est_ms`, optional warning lines) and a formatter that renders existing serial string responses exactly; no new metadata or severity levels yet.
- Offer a façade object (retaining or replacing `MotorCommandProcessor::processLine` as needed) that coordinates parser, router, batch executor, and response formatter, guaranteeing identical observable behavior for serial consumers and tests.

### Non-Functional Requirements
- Preserve timing, memory footprint, and response formatting to ensure compatibility with existing serial tooling and automated tests.
- Minimize blast radius: confine changes to the motor control command pipeline and only touch adjacent code when it simplifies the design.
- Keep the new abstractions lightweight and compliant with `@agent-os/standards/global/coding-style.md`, `@agent-os/standards/global/conventions.md`, and embedded resource constraints from `@agent-os/standards/global/resource-management.md`.
- Provide only the interfaces needed for current unit tests (e.g., injectable CID allocator or clock) in line with `@agent-os/standards/testing/unit-testing.md`.

## Visual Design
No visual assets provided for this firmware-focused refactor.

## Reusable Components
### Existing Code to Leverage
- `lib/MotorControl/src/MotorCommandProcessor.cpp` and `MotorCommandProcessor.h` for existing verb handling, validation rules, and response wording.
- `MotorControl/MotorController.h`, `MotionKinematics.h`, and `MotorControlConstants.h` for motion execution and timing estimates.
- `net_onboarding` modules (`NetOnboarding`, `NetSingleton`, `Cid`, `SerialImmediate`) for CID allocation and NET command behaviors.
- Current unit tests under `test/test_MotorControl/` and `test/test_NetCommands/` to confirm behavior parity.

### New Components Required
- `CommandExecutionContext` struct/class encapsulating shared services and controller access.
- `command_utils` module exposing parsing/validation helpers.
- `CommandParser`, `ParsedCommand`, and supporting batch utilities.
- `CommandRouter` with handler classes (`MotorCommandHandler`, `QueryCommandHandler`, `NetCommandHandler`, plus any helper structs).
- `CommandResult` and `ResponseFormatter` (or equivalent) that produce the exact serial output strings expected today.

## Technical Approach
- Database: No new persistence. All state remains in firmware memory managed by `MotorController`.
- API: Maintain the existing serial command surface; any signature changes to the façade must keep observable request/response semantics identical.
- Frontend: Not applicable; embedded serial interface remains the user-facing surface.
- Firmware Architecture: Place new headers under `lib/MotorControl/include/MotorControl/command/` and implementations under `lib/MotorControl/src/command/` (subfolders for handlers/utilities as needed) to keep translation units focused and follow existing include conventions.
- Testing: Extend unit coverage only where new abstractions require it—e.g., parser edge cases, router dispatch, batch executor CID aggregation—while ensuring all current tests continue to pass without modification. Follow `@agent-os/standards/testing/unit-testing.md` for structure and naming.

## Out of Scope
- Implementing MQTT transport logic, JSON formatting, or additional transport metadata.
- Changing validation strictness, response wording, or command semantics beyond what is necessary to reorganize the code.
- Introducing broad dependency injection frameworks or new global services beyond minimal test hooks.

## Success Criteria
- All existing command-processor unit and integration tests pass without requiring changes to expected outputs.
- New modular components compile within current resource limits and demonstrate clearer separation of concerns (reviewable via code structure and documentation).
- Serial clients observe identical command responses, including multi-command batches, CID sequencing, and warning/ACK ordering.
- Documentation (code comments or README notes) describes how transports integrate with the new pipeline, enabling future MQTT work without revisiting core parsing logic.
