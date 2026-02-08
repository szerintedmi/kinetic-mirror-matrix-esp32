# Local Mosquitto Setup (macOS)

Follow these steps to stand up a local Mosquitto broker for the MQTT telemetry steel thread.

## 1. Install Mosquitto via Homebrew

- Ensure Homebrew is up to date: `brew update`
- Install Mosquitto: `brew install mosquitto`

## 2. Create a Minimal Configuration

Save the following as `/opt/homebrew/etc/mosquitto/mosquitto.conf` (replace the file if it already exists). This is the path `brew services` loads by default and it survives Homebrew upgrades.

```conf
# ===============================
# Minimal Mosquitto Config (Local)
# ===============================

# Listen on the default MQTT port (IPv4 only)
listener 1883 0.0.0.0
socket_domain ipv4

# WebSocket MQTT (for the controller UI)
listener 9001 0.0.0.0
protocol websockets
socket_domain ipv4

password_file /opt/homebrew/etc/mosquitto/passwd
allow_anonymous false

# Logging
log_type all
log_timestamp true
log_dest stdout

# Optional: persistence (stores retained messages)
persistence true
persistence_location /opt/homebrew/var/lib/mosquitto/

# Optional: keepalive (default 60s)
connection_messages true
```

> Tip: On Intel macOS installs, replace `/opt/homebrew` with `/usr/local`.

## 3. Create Broker Credentials

Create a password file so the broker can authenticate clients:

```bash
# Create the file and add the first user (prompts for password)
mosquitto_passwd -c /opt/homebrew/etc/mosquitto/passwd mirror

# Add another user later (omit -c so you don't overwrite the file)
mosquitto_passwd /opt/homebrew/etc/mosquitto/passwd <another-user>

# Delete a user
mosquitto_passwd -D /opt/homebrew/etc/mosquitto/passwd <user-to-remove>
```

> **Warning:** The `-c` flag overwrites the entire file. Only use it once for initial setup.
>
> On Intel macOS, replace `/opt/homebrew` with `/usr/local`.

## 4. Start (or Restart) the Broker

- Start as a background service: `brew services start mosquitto`
- After config edits, restart: `brew services restart mosquitto`
- To see logs in real time, run in the foreground instead:
  ```bash
  mosquitto -c /opt/homebrew/etc/mosquitto/mosquitto.conf -v
  ```

## 5. Update Firmware Secrets

Edit `include/secrets.h` so the broker host and credentials match your local setup:

```c
#ifndef MQTT_BROKER_HOST
#define MQTT_BROKER_HOST "192.168.1.25" // Set to your Mac's IP or hostname
#endif

#ifndef MQTT_BROKER_USER
#define MQTT_BROKER_USER "mirror" // Matches mosquitto_passwd entry
#endif

#ifndef MQTT_BROKER_PASS
#define MQTT_BROKER_PASS "steelthread" // Matches stored password
#endif
```

Commit updated secrets only if they contain non-sensitive defaults; otherwise keep local changes untracked.

## 6. Smoke Test the Broker

- Verify the port is open: `nc -zv localhost 1883`
- Publish a retained test payload: `mosquitto_pub -u mirror -P '<password>' -t test/ping -r -m 'pong'`
- Confirm it is retained: `mosquitto_sub -u mirror -P '<password>' -t test/#`
- With firmware running, watch for telemetry snapshots: `mosquitto_sub -u mirror -P '<password>' -t 'devices/+/status'`
- Use the timestamped monitor script for debugging (reads credentials from `include/secrets.h`):

  ```bash
  poetry run python -m tools.mqtt_monitor                    # watch command topics
  poetry run python -m tools.mqtt_monitor -t 'devices/+/cmd/#'  # watch commands & responses
  poetry run python -m tools.mqtt_monitor -t 'devices/+/status'  # watch status
  poetry run python -m tools.mqtt_monitor -t '#'             # all topics
  ```

- A healthy node immediately publishes `{"node_state":"ready","ip":"<ipv4>","motors":{...}}` at 1 Hz idle / 5 Hz motion, and the broker delivers the LWT payload `{"node_state":"offline","motors":{}}` if the node disconnects unexpectedly.
- Altenratively you can also monitor the MQTT broker with `mosquitto_sub` and `gdate` to get timestamps for events:

```bash
 mosquitto_sub -h 192.168.1.25 -p 1883 -u mirror -P <yourpass> -t "devices/+/cmd/#" -v \
| while IFS= read -r line; do
      printf "%s %s\n" "$(gdate +'%H:%M:%S.%3N')" "$line"
    done
```

## 7. Troubleshooting Checklist

- Restart Mosquitto after editing `mosquitto.conf` or the password file.
- Ensure your Mac firewall allows inbound connections on TCP 1883.
- Confirm the device and Mac share a subnet or that routing is configured for the broker's IP.
- If connections fail, watch firmware logs for `CTRL: MQTT_CONNECT_FAILED` and broker logs for authentication errors.
- Verify the node's active broker settings with `MQTT:GET_CONFIG` and push updates with `MQTT:SET_CONFIG host=<ip-or-host> port=<port> user=<user> pass=<pass>`—no rebuild or flash required when the broker address or credentials change.
