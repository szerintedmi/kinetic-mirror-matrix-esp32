from __future__ import annotations

import json
import os
import re
import threading
import time
from pathlib import Path
from typing import Dict, Optional, Callable, List

try:
    import paho.mqtt.client as mqtt  # type: ignore
except Exception:  # pragma: no cover - dependency handled at runtime
    mqtt = None  # type: ignore


def load_mqtt_defaults() -> Dict[str, str]:
    """Load MQTT defaults from include/secrets.h, falling back to sane values."""
    defaults: Dict[str, str] = {
        "host": "192.168.1.10",
        "port": 1883,
        "user": "mirror",
        "password": "steelthread",
    }
    header_path = Path(__file__).resolve().parents[2] / "include" / "secrets.h"
    if not header_path.exists():
        return defaults
    text = header_path.read_text(errors="ignore")
    patterns = {
        "host": re.compile(r"#define\s+MQTT_BROKER_HOST\s+\"([^\"]+)\""),
        "port": re.compile(r"#define\s+MQTT_BROKER_PORT\s+(\d+)"),
        "user": re.compile(r"#define\s+MQTT_BROKER_USER\s+\"([^\"]*)\""),
        "password": re.compile(r"#define\s+MQTT_BROKER_PASS\s+\"([^\"]*)\""),
    }
    host_match = patterns["host"].search(text)
    if host_match:
        defaults["host"] = host_match.group(1)
    port_match = patterns["port"].search(text)
    if port_match:
        try:
            defaults["port"] = int(port_match.group(1))
        except ValueError:
            pass
    user_match = patterns["user"].search(text)
    if user_match:
        defaults["user"] = user_match.group(1)
    pass_match = patterns["password"].search(text)
    if pass_match:
        defaults["password"] = pass_match.group(1)
    return defaults


class MqttWorker(threading.Thread):
    """MQTT presence subscriber that mirrors SerialWorker's interface."""

    def __init__(
        self,
        *,
        broker: Optional[Dict[str, str]] = None,
        client_factory: Optional[Callable[[], "mqtt.Client"]] = None,
    ) -> None:
        super().__init__(daemon=True)
        self._broker = dict(broker or load_mqtt_defaults())
        self._lock = threading.Lock()
        self._stop_event = threading.Event()
        self.transport = "mqtt"
        self._presence: Dict[str, Dict[str, object]] = {}
        self._log: List[str] = []
        self._error: Optional[str] = None
        self._last_update_ts: float = 0.0
        self._client = None
        self._columns = (
            ("device", "device", 16),
            ("state", "state", 8),
            ("ip", "ip", 18),
            ("age_s", "age_s", 8),
            ("msg_id", "msg_id", 36),
        )

        if client_factory is not None:
            self._client_factory = client_factory
        else:
            if mqtt is None:
                raise RuntimeError("paho-mqtt not installed. Install 'paho-mqtt'.")

            def _factory() -> "mqtt.Client":
                client_id = self._broker.get("client_id")
                if not client_id:
                    client_id = f"mirrorctl-{os.getpid()}"
                client = mqtt.Client(client_id=client_id)  # type: ignore[attr-defined]
                return client

            self._client_factory = _factory

    # ------------------------------------------------------------------
    # Thread API
    # ------------------------------------------------------------------
    def run(self) -> None:  # pragma: no cover - exercised via integration/TUI
        try:
            client = self._client_factory()
            self._client = client
            client.on_connect = self._on_connect
            client.on_disconnect = self._on_disconnect
            client.on_message = self._on_message

            user = self._broker.get("user")
            password = self._broker.get("password")
            if user:
                client.username_pw_set(user, password or "")

            host = str(self._broker.get("host", "127.0.0.1"))
            port = int(self._broker.get("port", 1883))
            client.connect(host, port, keepalive=30)
            client.loop_forever()
        except Exception as exc:
            with self._lock:
                self._error = str(exc)
                self._append_log(f"[mqtt error] {exc}")

    def stop(self) -> None:
        self._stop_event.set()
        try:
            if self._client is not None:
                self._client.disconnect()
        except Exception:
            pass

    # ------------------------------------------------------------------
    # Control/compatibility helpers (mirrors SerialWorker)
    # ------------------------------------------------------------------
    def queue_cmd(self, cmd: str) -> None:
        stripped = cmd.strip()
        if stripped:
            self._append_log(f"> {stripped}")
        self._append_log("error: MQTT transport not implemented yet")

    def get_state(self) -> tuple:
        with self._lock:
            now = time.time()
            rows = []
            for device, data in self._presence.items():
                last_seen = float(data.get("last_seen", 0.0))
                age_s = max(0.0, now - last_seen) if last_seen else 0.0
                rows.append(
                    {
                        "device": device,
                        "state": data.get("state", ""),
                        "ip": data.get("ip", ""),
                        "msg_id": data.get("msg_id", ""),
                        "last_seen": last_seen,
                        "age_s": age_s,
                    }
                )
            rows.sort(key=lambda r: r["device"])
            return (
                rows,
                list(self._log[-800:]),
                self._error,
                self._last_update_ts,
                "",
            )

    def get_net_info(self) -> Dict[str, str]:
        with self._lock:
            return {
                "transport": "mqtt",
                "host": str(self._broker.get("host", "")),
                "port": str(self._broker.get("port", "")),
            }

    def get_thermal_state(self):  # pragma: no cover - compatibility stub
        return None

    def get_thermal_status_text(self) -> str:  # pragma: no cover - compatibility stub
        return ""

    # ------------------------------------------------------------------
    # MQTT callbacks
    # ------------------------------------------------------------------
    def _on_connect(self, client, userdata, flags, rc):
        topic = "devices/+/state"
        client.subscribe(topic, qos=1)
        self._append_log(f"[mqtt] connected; subscribed to {topic}")

    def _on_disconnect(self, client, userdata, rc):
        if not self._stop_event.is_set():
            self._append_log("[mqtt] disconnected")

    def _on_message(self, client, userdata, msg):
        payload = msg.payload.decode(errors="ignore") if isinstance(msg.payload, bytes) else str(msg.payload)
        self.ingest_message(msg.topic, payload)

    # ------------------------------------------------------------------
    def ingest_message(self, topic: str, payload: str, timestamp: Optional[float] = None) -> None:
        mac = topic.split("/")[1] if topic.startswith("devices/") else topic
        state = ""
        ip = ""
        msg_id = ""

        parsed = False
        try:
            obj = json.loads(payload)
            if isinstance(obj, dict):
                state_val = obj.get("state", "")
                ip_val = obj.get("ip", "")
                msg_val = obj.get("msg_id", "")
                state = str(state_val) if state_val is not None else ""
                ip = str(ip_val) if ip_val is not None else ""
                msg_id = str(msg_val) if msg_val is not None else ""
                parsed = True
        except (ValueError, TypeError, json.JSONDecodeError):
            parsed = False

        if not parsed:
            parts = {}
            for tok in payload.split():
                if "=" in tok:
                    key, value = tok.split("=", 1)
                    parts[key.strip()] = value.strip()
            state = parts.get("state", "")
            ip = parts.get("ip", "")
            msg_id = parts.get("msg_id", "")
        ts = timestamp if timestamp is not None else time.time()

        with self._lock:
            entry = self._presence.get(mac, {})
            entry.update(
                {
                    "state": state,
                    "ip": ip,
                    "msg_id": msg_id,
                    "last_seen": ts,
                }
            )
            self._presence[mac] = entry
            self._last_update_ts = ts

    # ------------------------------------------------------------------
    def _append_log(self, line: str) -> None:
        with self._lock:
            self._log.append(line)
            if len(self._log) > 800:
                del self._log[: len(self._log) - 800]

    # ------------------------------------------------------------------
    def get_table_columns(self):
        return list(self._columns)
