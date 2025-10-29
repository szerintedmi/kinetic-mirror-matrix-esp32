# Spec Requirements: mqtt-steel-thread-presence

## Initial Description
9. [ ] MQTT Steel Thread: Presence — Connect to a local broker with username/password; publish retained birth message and LWT on `devices/<id>/state`. Acceptance: node publishes “ready” immediately after connect; CLI shows online/offline flips within a few milliseconds on a local broker (target <25 ms median, <100 ms p95). Depends on: 8. `S`

## Requirements Discussion

### First Round Questions

**Q1:** I assume the firmware will use `AsyncMqttClient` on the ESP32, authenticating with the Wi-Fi credentials already stored in NVS plus a broker-specific username/password also kept in NVS. Is that correct, or should broker credentials live separately (e.g., compiled constants or OTA provisioning)?
**Answer:** yes, rely on existing WiFi connection, no change need for there. for broker address and user/pass use code defaults from secrets.h to start with and add a task for the ability to create SETs/GETs to manipulate them

**Q2:** I’m thinking the retained birth payload on `devices/<id>/state` should stay a simple UTF-8 string like "ready" to minimize parsing overhead. Should we instead publish a JSON blob with fields such as `{ "state": "ready", "fw": "<version>" }`?
**Answer:** keep it simpe for now but i think we should include node's IP address in the birth payload for discoverability

**Q3:** I assume the LWT payload will be a retained "offline" published with QoS1 to the same topic, triggered automatically on disconnect. Should we also publish a transitional "connecting" state during reconnect attempts so the CLI can differentiate?
**Answer:** yes, LWT should be retained offline and QoS1. I don't see the need for a separate "connecting" state for now, keep it simple.

**Q4:** I’m planning to reuse the existing node identifier that today appears in `/api/status` and serial `STATUS` output for the MQTT topic `<id>`. Do we need an override or alias mechanism so multiple clusters on one bench can be renamed without reflashing?
**Answer:** node identifier should be the mac address of the node. There is no "id" field in api/status currently but the mac address is there.

**Q5:** I assume we’ll initiate connection retries with an exponential backoff (e.g., 500 ms → 2 s → 5 s capped) and reset the backoff after 30 s of stable connectivity. Should we target a different retry policy to stay within the `<25 ms` median presence flip requirement?
**Answer:** is it for QoS1 retries? please clarify. I think we need to discuss these requirements a bit more - could the node be operational while can't connect to MQTT broker? If so then we shouldn't retry too aggressively, rather loggin MQTT fail and maybe introduce a command to reconnect? reconnect command can come later as to start we could just reboot the node

**Q6:** I’m thinking the CLI will subscribe to `devices/+/state`, debounce duplicates, and emit presence events to both the interactive TUI and log output. Should we also expose a one-shot `mirrorctl presence` command for scripting acceptance tests?
**Answer:** CLI yes subscribe to necessary events and debounce. Re: "emit presence events" if you mean the node does it right? I think regular interval (1s or even less) state event update + immedate on state change (eg a move start or end)

**Q7:** Are there any scenarios or integrations we must explicitly exclude for this steel thread (e.g., TLS brokers, cloud MQTT services, multi-tenant topics)?
**Answer:** we currently generate CID as a monothonic squence starting from 0 after every reboot. This raises potential issues of correlation mismatch after reboot - it can cause issues in the interactive CLI already but even more issues with the MQTT messaging. Idea (clarify / challenge if needed): We should generate UUID for mqqt msg_id, include it in every MQTT msg the node generates. (don't expect/rely for now that the cmds recevied have a msg id! ). This msg_id could be used for the as CID - keeping current CID logic but using msg_id is to reference to the original cmd ACK msg id we generated. We will also need to update the Serial cmds to work the same way for consitency. k0i05/esp_uuid ESP-IDF component is a good candidate for UUID generation but we must be sure we seed the random generator with a good entropy source.

### Existing Code to Reference
- `include/secrets.h` default broker credentials (seed tasks for future SET/GET commands)
- Existing serial command CID generator (to be migrated to UUID-based IDs)

### Follow-up Questions

**Follow-up 1:** On reconnect behavior: should the node keep running moves and other work even if the broker is down, emitting a single MQTT failure log and then retrying on a relaxed interval (e.g., every 30–60 s), or do you want a different cadence or operator-triggered reconnect flow for this steel thread?
**Answer:** yes, don't interrupt anything if broker is down. No need for automatic retry for now

**Follow-up 2:** For host tooling: beyond the continuous subscription with debounced presence events, do you also want a one-shot `mirrorctl presence` command in this spec, and can you confirm the node should continue sending 1 Hz (or faster on change) state updates while the CLI just listens?
**Answer:** no need for mirrorctl presence for now

**Follow-up 3:** Regarding the UUID-based `msg_id` plan: shall we include in this spec both (a) generating UUIDs for every MQTT message the node publishes and (b) updating the serial command pathway to mirror that behavior so ACKs reference the same IDs? Also, please confirm there are no additional scope exclusions (e.g., TLS brokers, cloud endpoints) we should note for this iteration.
**Answer:** yes include switching serial CIDs to UUID after we have the MQQT path working

## Visual Assets
No visual assets provided.

## Requirements Summary

### Functional Requirements
- Firmware connects to MQTT broker using existing Wi-Fi session, with broker host/user/password sourced from current `secrets.h` defaults; add future task for runtime SET/GET management.
- Publish retained birth message on `devices/<mac>/state` using node MAC address in topic; payload includes readiness state and current IP address for discoverability.
- Configure retained QoS1 LWT payload of `"offline"` on the same presence topic; no intermediate "connecting" state.
- Node continues normal operation when broker unavailable, logs the failure once, and does not attempt automatic reconnect loops for this steel thread.
- Node periodically (≈1 Hz) and on state change publishes presence/state updates so host tools observe quick flips; CLI subscribes to `devices/+/state`, debounces duplicates, and surfaces events to interactive TUI/log stream (no dedicated `mirrorctl presence` command yet).
- Introduce UUID-based `msg_id` for every MQTT message emitted by the node, using a proven ESP32 UUID source seeded with high-quality entropy.
- After MQTT pathway is working, migrate serial command acknowledgments to use the same UUIDs for consistent correlation.

### Non-Functional Requirements
- Presence flips must remain responsive enough to meet `<25 ms` median / `<100 ms` p95 latency target when broker reachable.
- Keep implementation simple: plaintext MQTT on trusted LAN, no TLS or cloud broker support in this iteration.
- Ensure UUID generation has minimal boot-time overhead and does not block primary motion tasks.

### Reusability Opportunities
- Leverage existing MQTT/serial command abstractions introduced in command pipeline modularization.
- Extend current CID handling logic to accept UUIDs without rewriting downstream consumers.
- Reuse logging facilities to report broker connectivity failures without spamming logs.

### Scope Boundaries
**In Scope:**
- MQTT presence connection with retained birth/LWT payloads and MAC-based topics.
- Inclusion of node IP in birth payload and consistent presence publishing cadence.
- UUID generation for MQTT messages and planned serial CID migration once MQTT path validated.

**Out of Scope:**
- Runtime SET/GET commands for broker credentials (tracked as follow-up task).
- TLS-secured or remote/cloud MQTT brokers.
- Dedicated CLI presence command or reconnect command.
- Automated broker reconnect logic beyond initial connect attempt.

### Technical Considerations
- Must read MAC address from existing system info source for topic naming.
- Secret defaults remain in `include/secrets.h`; ensure no new secrets leak to repo.
- Evaluate `k0i05/esp_uuid` (ESP-IDF component) integration under Arduino core; verify entropy source (e.g., `esp_random` seeded by hardware RNG).
- Ensure payload schema change (adding IP) keeps CLI parsing resilient; document expected format for downstream tooling.

