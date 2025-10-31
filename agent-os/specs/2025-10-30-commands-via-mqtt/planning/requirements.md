# Requirements Research

## Clarifying Questions & Answers

**Question 1:** I assume each MQTT command payload should mirror the current serial action syntax but wrapped in JSON with fields like `cmd_id`, `action`, and parameters—does that match what you expect, or should the payload stay as the raw serial string?
**Answer:** JSON payload. Consolidate data schema, field names, statuses, error code types, and generic envelopes per topic. Retrofit serial commands so the MQTT protocol stays consistent and concise while still mapping well to serial commands.

**Question 2:** I’m thinking the node should publish responses synchronously after executing (or rejecting) the command so the CLI can await a single `{ ok|err, est_ms, details }` message—should we instead allow intermediate progress events before the final response?
**Answer:** Provide an immediate ACK (QoS1) similar to serial, then send a QoS1 completion message when execution finishes. Both messages must include the original `cmd_id`. Reuse the existing UUID generator for message IDs.

**Question 3:** I assume we’ll keep QoS1 for both request and response topics and rely on the `cmd_id` for de-duplication when messages retry; is that correct, or do you prefer adding explicit sequence numbers or timestamps too?
**Answer:** Yes, use `cmd_id` only; no need for additional sequence numbers or timestamps for now.

**Question 4:** For `BUSY` handling, should the node check only active motion or also pending safety timers (e.g., thermal cooldown) before rejecting, or do you want the master to handle those cases without node-side gates?
**Answer:** Keep the current behavior. No changes to BUSY handling are required for MQTT.

**Question 5:** I’m planning to surface MQTT command telemetry through the existing CLI/TUI, with serial transport available via a `--transport serial` override—do you also want an automatic fallback to serial if MQTT fails to connect?
**Answer:** No automatic fallback to serial is needed in the TUI.

**Question 6:** I assume error codes and messages should stay identical to the serial protocol so documentation remains unified—should we introduce MQTT-specific error metadata (e.g., broker errors, auth failures) in the same response envelope?
**Answer:** Error codes can be renamed or restructured to consolidate protocols or accommodate MQTT needs. Ensure docs and tests are updated.

**Question 7:** For configuration, do you want broker credentials, client ID, and topic prefixes stored in NVS alongside Wi‑Fi onboarding, or should we rely on compile-time defaults with optional overrides from the CLI?
**Answer:** Treat MQTT configuration as a separate task after command support ships. Add commands (usable via serial) to get and set MQTT broker/port/credentials, storing values in Preferences with code defaults as fallback.

**Question 8:** Are there any operations or command actions you explicitly want to exclude or defer in this MQTT phase (e.g., HOME overrides, thermal toggles, low-level diagnostics)?
**Answer:** Maintain full feature parity with serial. Command and parameter naming, response content, and workflows must be consistent (JSON on MQTT, line format on serial).

### Existing Code to Reference
No similar existing features identified for reference.

## Visual Assets

No visual assets provided.

## Requirements Summary

### Functional Requirements
- Provide MQTT command handling that mirrors the serial command set with full action and parameter parity.
- Define a consolidated JSON schema with consistent field names, statuses, error codes, and envelope structures per MQTT topic.
- On receiving a command, publish an immediate QoS1 ACK message containing the generated `cmd_id`, then publish a QoS1 completion message when execution finishes (success or failure).
- Generate `cmd_id` values using the existing UUID generator and reuse the same identifier in ACK and completion messages.
- Preserve existing BUSY logic; MQTT transport must not alter motion scheduling or safety gating behavior.
- Keep CLI/TUI MQTT-first without automatic serial fallback, while still allowing manual `--transport serial` selection.
- Update documentation and tests to reflect any consolidated or renamed error codes across serial and MQTT.
- Ensure serial responses remain line-based while MQTT responses use the new JSON envelope, keeping semantics aligned.
- After achieving basic parity, add MQTT configuration get/set commands (usable via serial and MQTT) to read and persist broker/port/credential settings in Preferences, falling back to code defaults when unset.

### Non-Functional Requirements
- Maintain QoS1 for command and response topics with idempotent handling based on `cmd_id`.
- Design the JSON schema to be concise and consistent for future reuse across additional MQTT features.
- Guarantee backward compatibility for existing serial clients by preserving their request/response structure during consolidation.

### Reusability Opportunities
- Leverage the current serial command processing pipeline when defining shared schemas and error catalogs.
- Reuse the existing UUID message ID generator within the firmware for `cmd_id` assignments.

### Scope Boundaries
**In Scope:**
- Implementing MQTT request/response handling for all existing serial commands.
- Defining and documenting a unified command schema and error model.
- Delivering ACK and completion messaging patterns over MQTT.
- Adding MQTT configuration commands right after command parity work within this roadmap item.

**Out of Scope:**
- Any changes to BUSY logic or thermal scheduling behavior.
- Automatic transport failover in the CLI/TUI.

### Technical Considerations
- Align topic payload structures with prior MQTT telemetry work to simplify client parsing.
- Ensure firmware leverages Preferences storage only when the future configuration commands are added, while retaining compile-time defaults today.
- Plan documentation updates to cover both MQTT JSON schemas and serial line formats post-consolidation.
