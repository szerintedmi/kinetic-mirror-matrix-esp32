# Specification: MQTT Steel Thread: Presence

## Goal
Deliver a minimal MQTT presence pathway that broadcasts node availability with retained messages and allows the existing CLI/TUI to monitor nodes over MQTT without disrupting current serial workflows.

## User Stories
- As a bench operator, I want the controller to announce when it is online over MQTT so I can see presence flips instantly in the CLI.
- As a firmware engineer, I want presence payloads to include the node's IP address so I can discover devices on the LAN without walking the network.
- As a CLI user, I want to select MQTT as the transport so the interactive TUI shows presence based on broker messages even if command execution still goes over serial.

## Core Requirements
### Functional Requirements
- Firmware connects to the MQTT broker using the existing Wi-Fi session and the default broker host/user/pass compiled in `include/secrets.h`; no new credential storage is introduced yet.
- Use the device MAC (lowercase hex without separators) to form the topic `devices/<mac>/state`; publish a retained QoS1 birth message immediately after connect with JSON payload `{"state":"ready","ip":"<ipv4>","msg_id":"<uuid>"}`, where `<ipv4>` is the dotted quad pulled from NetOnboarding status.
- Configure a retained QoS1 LWT payload of `{"state":"offline"}` on the same topic; no intermediate "connecting" state is emitted.
- Push presence updates at ~1 Hz while connected and immediately on state changes (wake, sleep, motion start/stop) so the CLI/TUI sees flips within the `<25 ms` / `<100 ms` SLA when the broker is reachable.
- When the broker connection fails, log `CTRL:WARN MQTT_CONNECT_FAILED …` once, then enter a non-blocking reconnect loop capped by an exponential backoff (start ≥1 s, max ≤30 s) so presence recovers automatically while motor work continues.
- Emit `CTRL: MQTT_DISCONNECTED …` immediately when the MQTT session drops and `CTRL: MQTT_RECONNECT delay=<ms>` whenever a retry is scheduled; on success, continue to log the existing `CTRL: MQTT_CONNECTED broker=…`.
- Generate a UUIDv4 `msg_id` for every MQTT publication using an ESP32-safe entropy source (e.g., `esp_random` via `k0i05/esp_uuid`), attach it to presence payloads as the JSON field `"msg_id":"<uuid>"`, and expose the generator through the existing command pipeline context.
- After the MQTT pathway is validated, switch serial command acknowledgments to reuse the same UUIDs instead of the current monotonic CID to maintain correlation parity across transports.
- Extend `tools/serial_cli` so both the command-line utilities and interactive TUI accept `--transport {serial,mqtt}` (default serial). When `mqtt` is selected, connect with `paho-mqtt` using the same default host/user/pass, subscribe to `devices/+/state`, and drive the TUI status table from MQTT presence messages.
- While in MQTT mode, non-presence CLI commands (`move`, `home`, `wake`, etc.) may respond with `error: MQTT transport not implemented yet` without attempting broker command dispatch; STATUS views must be populated exclusively from MQTT data.
- Surface presence activity in the CLI log stream by debouncing duplicate payloads and tagging entries with the node MAC and latest IP address for quick operator diagnostics.
- Reconnect attempts must respect Wi-Fi state (only retry while `NetOnboarding` is CONNECTED) and backoff timers must reset after a successful connection.

### Non-Functional Requirements
- Firmware-side MQTT integration must add <15 kB flash and keep incremental RAM usage under 4 kB to preserve resource headroom.
- Presence latency budget: end-to-end publish to CLI render median <25 ms, p95 <100 ms on a local broker (<5 ms LAN RTT) using retained QoS1 messages.
- Ensure UUID generation and MQTT callbacks run outside timing-sensitive motor control loops (e.g., dedicated connectivity task) per RTOS structure guidance.
- Maintain plaintext MQTT on trusted LAN only; TLS, cloud brokers, and credential mutation flows remain out of scope for this iteration.

## Visual Design
No visual assets provided; reuse existing CLI/TUI styling and layouts.

## Reusable Components
### Existing Code to Leverage
- Firmware command pipeline: `lib/MotorControl` command parser/router and `CommandExecutionContext` for CID management.
- Networking utilities: `lib/net_onboarding` (`NetOnboarding`, `Cid`, `SerialImmediate`) for MAC/IP access and serial event logging.
- Serial CLI runtime: `tools/serial_cli/runtime.SerialWorker`, status parsers, and `tools/serial_cli/tui/*` Interactive Textual UI scaffolding.
- Logging conventions already emitting `CTRL: NET:*` lines in `src/main.cpp` for network status.

### New Components Required
- `MqttPresenceClient` (firmware) encapsulating AsyncMqttClient setup, payload formatting, and UUID tagging because no MQTT layer exists yet.
- `tools/serial_cli/mqtt_runtime.py` (or equivalent) implementing a lightweight MQTT subscriber/poller to feed the existing TUI abstractions without rewriting them.

## Technical Approach
- Database: No new storage; broker defaults remain compiled in `include/secrets.h`. Document follow-up task for runtime SET/GET support using Preferences once MQTT path is proven.
- API: Topics `devices/<mac>/state` (QoS1 retained). Payload schema `{"state":"ready","ip":"<ipv4>","msg_id":"<uuid>"}` for ready publications and `{"state":"offline"}` for the LWT.
- Frontend: Extend `serial_cli` argument parsing to add `--transport`, reuse existing renderers to display latest MQTT-derived status rows, and annotate unavailable commands when MQTT mode is active.
- Testing: Add PlatformIO native unit tests for UUID formatting and payload builder; add host-side integration test (mock MQTT broker or loopback) that publishes synthetic presence messages and asserts CLI/TUI parses them; perform bench validation that a node connect/disconnect flips state within latency targets.

## Out of Scope
- Runtime broker credential management commands (tracked separately as follow-up task).
- MQTT command execution (`devices/<id>/cmd` topics) and STATUS parity payloads beyond presence.
- TLS, authenticated cloud brokers, or multi-tenant topic hierarchies.
- Automated broker reconnection logic or reboot-triggered retries beyond the initial connection attempt.

## Success Criteria
- Node publishes retained JSON presence `{"state":"ready","ip":"<ipv4>","msg_id":"<uuid>"}` immediately after broker connection and retains `{"state":"offline"}` as the LWT when powered down or disconnected.
- CLI/TUI invoked with `--transport mqtt` shows presence rows updating from MQTT messages and reports `error: MQTT transport not implemented yet` when other commands are issued.
- Serial acknowledgments switch to UUID-based IDs without breaking existing tests, and UUIDs match those used in MQTT presence publications once the migration lands.
- Bench test verifies presence flips meet the `<25 ms` / `<100 ms` performance target on a local Mosquitto broker, with firmware continuing motor operations when the broker is unavailable.
