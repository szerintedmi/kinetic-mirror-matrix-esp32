# Initial Spec Idea

## User's Initial Description
Current MotorCommandProcessor  tries to own every concern—transport-specific parsing, validation, command dispatch, motor/net side effects, batching— it is a monolithic class. To make room for MQTT while keeping logic shared, we need to carve the pipeline into  layers and introduce smaller roles.

We need a structure which keeps command semantics identical for Serial and MQTT, removes the current 800+ line god class, and makes CID handling, validation, and response formatting single-sourced.

Idea for considertion - feel free to simplify / suggest improvements.  Three layers:  Transport-Agnostic Core , Command Pipeline and Transport Adapters.

1. Transport-Agnostic Core

Create a CommandExecutionContext struct that wraps the shared dependencies today hidden inside MCP (controller reference, net_onboarding services, clock accessor, CID generator). Both Serial and future MQTT channels will pass this context to the core.
Extract CID allocation and error/ACK formatting into a CtrlResponseBuilder. That keeps CID sequencing uniform and testable, and avoids re-implementing response strings per transport.
Move the CSV/trim/int parsing helpers and id-mask validation into a CommandParsers namespace with small pure helpers. They can be reused anywhere we need to validate input (serial, MQTT, tests).

2. Command Pipeline

Introduce a lightweight ParsedCommand DTO produced by a CommandParser class: it should hold verb, argument tokens, and original raw text (for diagnostics). The parser also handles multi-command batches and overlap detection so transports just hand over raw lines.
Split execution into a CommandRouter that maps verbs to handler objects implementing CommandHandler::execute(const ParsedCommand&, CommandExecutionContext&, uint32_t now_ms).
MotorCommandHandler covers MOVE/HOME/WAKE/SLEEP plus thermal checks; internally we can further split MOVE/HOME logic into helpers if needed.
QueryCommandHandler deals with GET/STATUS/HELP and aggregates status formatting.
NetCommandHandler encapsulates NET:* commands.
A BatchExecutor class can live alongside the router to preserve the multi-command semantics (est_ms aggregation, shared BUSY checks). This keeps processLine focused on orchestration rather than string surgery.
Keep the existing MotorController dependency untouched; handlers call into it using methods on the context, so adding MQTT doesn’t change controller-level code.

3. Transport Adapters

Define an interface like ICommandChannel with std::string handleCommand(const std::string&, uint32_t now_ms). Serial console and future MQTT subscriber both take the same core CommandProcessor object.
The Serial adapter keeps current buffering behavior but simply forwards complete lines to the processor. An MQTT adapter can receive payloads, normalize them to a command line (maybe JSON fields → raw string), and call the same interface, guaranteeing identical command handling.
If MQTT needs async responses, we can let CommandProcessor return a structured result (CommandResult with optional multi-line payload and severity) that transports render appropriately (Serial keeps newline-separated text; MQTT could publish a JSON payload without touching the business logic).


Incremental Path

Extract helper utilities (trim, split, parseCsvQuoted, parseInt, parseIdMask) into a new motor/command_utils.* module and migrate call sites.
Introduce CommandExecutionContext and thread it through the existing class, replacing direct global calls (net_onboarding::NextCid, etc.). This is mostly mechanical and unlocks unit testing.
Implement CommandRouter + handler classes while keeping the old processLine as a thin wrapper delegating to the router. Start with read-only verbs (HELP/GET/STATUS) to prove the pattern, then migrate MOVE/HOME, and finally NET commands.
Once all verbs live in handlers, rename the remaining shell to CommandProcessor and replace the global static batch flags with a small BatchExecutor.
Add tests around the new parser/router to ensure serial and future MQTT share coverage; refer to agent-os/standards/testing/ for the required cases.

first understand and validate the proposal. Suggest any improvements in approach , including implementation plan.

## Metadata
- Date Created: 2025-10-29
- Spec Name: command-pipeline-modularization
- Spec Path: agent-os/specs/2025-10-29-command-pipeline-modularization
