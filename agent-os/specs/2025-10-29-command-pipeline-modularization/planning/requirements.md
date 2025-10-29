# Command Pipeline Modularization — Requirements

## Clarifying Questions & Answers

1. **Question:** I’m planning to keep the public `MotorCommandProcessor::processLine` signature and exact text outputs so existing serial tools and tests stay untouched—can we treat any response-format changes as out of scope unless we add a new transport?
   **Answer:** Changing the signature is allowed if it makes the new structure more solid or simpler.

2. **Question:** I’m assuming the new `CommandExecutionContext` should own the single `MotorController` instance and expose read-only helpers for clock/CID, while transports remain stateless wrappers; should handlers ever instantiate their own controllers or are we centralizing all device state in the context?
   **Answer:** Yes, keep a single `MotorController` with state as it is now; transports remain stateless.

3. **Question:** For `CommandResult`, I’m leaning toward a structured payload (without severity) that adapters render into strings/JSON—does that cover MQTT and Serial needs, or do we anticipate richer metadata (e.g., transport hints, async tokens)?
   **Answer:** This is a good time to introduce a structured payload. Cover what we have today; no need for severity or richer metadata yet. Iterate later for MQTT roadmap items.

4. **Question:** Batch execution today aggregates `est_ms` and shares BUSY rules via globals; can we preserve the current semantics exactly (same warnings, same CID reuse) and treat any behavior change as a bug for this refactor?
   **Answer:** Preserve current semantics exactly.

5. **Question:** I intend to keep command semantics and validation identical, only relocating helpers into reusable modules; is there any appetite to tighten validation (e.g., stricter CSV errors) as part of this effort, or should we defer behavior tweaks?
   **Answer:** No need to tighten validation for now.

6. **Question:** For dependency injection/testing, I’m assuming we’ll introduce lightweight interfaces (e.g., `ICidAllocator`, `IClock`) so unit tests can stub them—are there other collaborators we should plan to inject now?
   **Answer:** Inject only the bare minimum needed for unit tests for now.

7. **Question:** The roadmap’s MQTT work needs mirrored command coverage; should the modularization also define an explicit adapter interface for future transports, or is the façade only expected to serve the firmware internals for now?
   **Answer:** The goal is to tidy up the command processor in preparation for future MQTT-specific needs, without starting MQTT work yet.

8. **Question:** Are there any areas you explicitly want to keep untouched or future enhancements we should avoid baking in at this stage?
   **Answer:** Limit blast radius, but we can touch other parts if it meaningfully simplifies things.

## Existing Code to Reference
No specific reuse targets were provided. Investigate the current `MotorCommandProcessor` implementation and unit tests for patterns to retain.

## Visual Assets
No visual assets provided. (Verified via `ls -la agent-os/specs/2025-10-29-command-pipeline-modularization/planning/visuals/`).

## Requirements Summary

### Functional Requirements
- Restructure command handling into modular layers (parsing, routing, handlers) while keeping behavior identical.
- Maintain a single controller instance managed through a shared execution context.
- Introduce a structured command result payload that represents today’s responses without changing semantics.
- Preserve existing batch semantics, CID sequencing, and validation logic.
- Provide transport-agnostic adapters that remain stateless and ready for future MQTT use without implementing MQTT-specific logic now.

### Non-Functional Requirements
- Minimize blast radius by limiting changes to areas that benefit from the refactor.
- Maintain current performance characteristics and response timing.
- Keep tests reliable by enabling targeted dependency injection only where necessary.

### Reusability Opportunities
- Extract shared parsing/validation helpers for reuse across future transports and tests.
- Leverage existing unit tests to ensure backward compatibility; extend only where coverage gaps appear after modularization.

### Scope Boundaries
**In Scope:**
- Refactoring the command pipeline structure, shared helpers, and response construction.
- Introducing minimal interfaces or abstractions required for testing and future transport adapters.

**Out of Scope:**
- MQTT transport implementation or enhancements beyond preparing the command processor for it.
- Behavioral changes to validation, semantics, or response formatting.
- Broad dependency injection frameworks or additional metadata beyond current needs.

### Technical Considerations
- Keep `MotorController` state centralized within `CommandExecutionContext`.
- Ensure the new structured payload maps cleanly back to existing serial output.
- Only inject the minimal collaborators needed for unit tests (e.g., clock or CID stubs).
- Any signature changes should demonstrably simplify the architecture without altering observable behavior.
