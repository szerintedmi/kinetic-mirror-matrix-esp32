# Error Codes

Central catalog: `lib/transport/src/CommandSchema.cpp`

## Motor Commands (E## format)

| Code | Reason | Description |
|------|--------|-------------|
| E01 | BAD_CMD | Unknown action |
| E02 | BAD_ID | Invalid motor ID or mask |
| E03 | BAD_PARAM | Parameter validation failed |
| E04 | BUSY | Motor/controller busy |
| E07 | POS_OUT_OF_RANGE | Position outside travel range |
| E10 | THERMAL_REQ_GT_MAX | Move exceeds max thermal budget |
| E11 | THERMAL_NO_BUDGET | Insufficient thermal budget |
| E12 | THERMAL_NO_BUDGET_WAKE | Wake rejected, no thermal budget |

Gaps (E05-06, E08-09) are organic — don't reuse without checking for collisions.

## Network (NET_ prefix)

`NET_BAD_PARAM`, `NET_SAVE_FAILED`, `NET_SCAN_AP_ONLY`, `NET_BUSY_CONNECTING`, `NET_CONNECT_FAILED`

## MQTT (MQTT_ prefix)

`MQTT_BAD_PAYLOAD`, `MQTT_UNSUPPORTED_ACTION`, `MQTT_BAD_PARAM`, `MQTT_CONFIG_SAVE_FAILED`

## Rules

- New motor errors: use next available E## code
- New subsystem errors: use `SUBSYSTEM_REASON` format
- All codes must be added to the catalog in `CommandSchema.cpp`
- Response format: `{"status":"error", "errors":[{"code":"E03", "reason":"BAD_PARAM"}]}`
