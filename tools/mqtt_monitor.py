#!/usr/bin/env python3
"""Subscribe to MQTT command topics and print timestamped messages.

Uses MQTT broker settings from include/secrets.h (same config as mirror_cli).

Usage:
    python -m tools.mqtt_monitor              # defaults to devices/+/cmd/#
    python -m tools.mqtt_monitor -t 'devices/+/status'
    python -m tools.mqtt_monitor -t '#'       # all topics
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from datetime import datetime
from shutil import which

# Reuse existing config loader from mirror_cli
from tools.mirror_cli.mqtt_runtime import load_mqtt_defaults


def main() -> int:
    parser = argparse.ArgumentParser(description="Subscribe to MQTT topics with timestamped output")
    parser.add_argument(
        "-t",
        "--topic",
        default="devices/+/cmd/#",
        help="MQTT topic pattern (default: devices/+/cmd/#)",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="Show topic with each message (mosquitto_sub -v)",
    )
    args = parser.parse_args()

    # Check mosquitto_sub is available
    if not which("mosquitto_sub"):
        print(
            "error: mosquitto_sub not found. Install with: brew install mosquitto",
            file=sys.stderr,
        )
        return 1

    # Load broker config from include/secrets.h
    config = load_mqtt_defaults()
    host = str(config.get("host", "127.0.0.1"))
    port = str(config.get("port", 1883))
    user = config.get("user", "")
    password = config.get("password", "")

    cmd = [
        "mosquitto_sub",
        "-h",
        host,
        "-p",
        port,
        "-t",
        args.topic,
        "-v",  # always show topic for clarity
    ]
    if user:
        cmd.extend(["-u", user])
    if password:
        cmd.extend(["-P", password])

    print(f"Subscribing to {args.topic} on {host}:{port}", file=sys.stderr)

    try:
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )
        assert proc.stdout is not None
        for line in proc.stdout:
            timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]  # HH:MM:SS.mmm
            print(f"{timestamp} {line.rstrip()}")
    except KeyboardInterrupt:
        print("\nInterrupted", file=sys.stderr)
        proc.terminate()
        return 0
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    return proc.wait()


if __name__ == "__main__":
    sys.exit(main())
