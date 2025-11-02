# Bench Validation Checklist — Commands via MQTT (2025-10-30)

Reference: @agent-os/standards/testing/hardware-validation.md

## Pre-Test Setup
- [ ] Flash latest firmware build (record git hash and config revision on log sheet)
- [ ] Connect bench ESP32 to broker and confirm MQTT presence heartbeat
- [ ] Run `scripts/smoke_test.py` (or lab equivalent) to verify baseline serial command path

## Command Parity Exercises
- [ ] Send `MOVE:0,200` over MQTT; confirm ACK < 100 ms and completion payload reports `status=done`
- [ ] Immediately queue overlapping `MOVE:0,150`; verify completion arrives with `status=error` and `code=E04`
- [ ] Observe `CTRL:INFO MQTT_DUPLICATE` logged when resending the previous `cmd_id`
- [ ] Issue `GET`/`STATUS` command pair and compare table output with MQTT telemetry (position, moving, budget)

## Configuration Persistence
- [ ] Execute `MQTT:SET_CONFIG host=test-broker.local port=1885 user=bench pass=benchpass`
- [ ] Power-cycle the node twice; confirm reconnect to new broker without additional commands
- [ ] Run `MQTT:GET_CONFIG` after reboot and verify stored credentials match expected values

## Safety & Recovery
- [ ] Trigger BUSY condition during active motion and ensure no unintended motor movement occurs
- [ ] Validate watchdog/reset path: invoke manual reset and confirm node returns to READY state with MQTT presence online
- [ ] Capture log excerpt including firmware hash, broker host, and validation timestamp for traceability

## Sign-Off
- [ ] Archive MQTT and serial logs in bench run folder with device ID and date
- [ ] Update lab checklist row with operator initials and test outcome
