# Local Mosquitto Setup (macOS)

Follow these steps to stand up a local Mosquitto broker for the MQTT telemetry steel thread.

## 1. Install Mosquitto via Homebrew

- Ensure Homebrew is up to date: `brew update`
- Install Mosquitto: `brew install mosquitto`

## 2. Create a Minimal Configuration

Save the following as `$(brew --prefix mosquitto)/etc/mosquitto/mosquitto.conf` (replace the file if it already exists). Adjust the persistence path if your Homebrew prefix differs.

```conf
# ===============================
# Minimal Mosquitto Config (Local)
# ===============================

# Listen on the default MQTT port
listener 1883 0.0.0.0

password_file /opt/homebrew/opt/mosquitto/etc/mosquitto/passwd
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

- From the Mosquitto etc directory: `cd $(brew --prefix mosquitto)/etc/mosquitto`
- Create the password database: ``mosquitto_passwd -c ./passwd <mqtt-user>``
- Re-run without `-c` to add additional users later. The repo defaults assume the user `mirror`.

## 4. Start (or Restart) the Broker

- Start the service: `brew services start mosquitto`
- After config edits, restart: `brew services restart mosquitto`
- To tail logs: `brew services info mosquitto` (shows log path) or launch foreground with `mosquitto -c ... -v`

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
- A healthy node immediately publishes `{"node_state":"ready","ip":"<ipv4>","motors":{...}}` at 1 Hz idle / 5 Hz motion, and the broker delivers the LWT payload `{"node_state":"offline","motors":{}}` if the node disconnects unexpectedly.

## 7. Troubleshooting Checklist

- Restart Mosquitto after editing `mosquitto.conf` or the password file.
- Ensure your Mac firewall allows inbound connections on TCP 1883.
- Confirm the device and Mac share a subnet or that routing is configured for the broker's IP.
- If connections fail, watch firmware logs for `CTRL: MQTT_CONNECT_FAILED` and broker logs for authentication errors.
- Verify the node's active broker settings with `MQTT:GET_CONFIG` and push updates with `MQTT:SET_CONFIG host=<ip-or-host> port=<port> user=<user> pass=<pass>`—no rebuild or flash required when the broker address or credentials change.
