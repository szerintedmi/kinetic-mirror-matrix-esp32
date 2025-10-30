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
    """MQTT telemetry subscriber that mirrors SerialWorker's interface."""

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
        self._devices: Dict[str, Dict[str, object]] = {}
        self._log: List[str] = []
        self._error: Optional[str] = None
        self._last_update_ts: float = 0.0
        self._client = None
        self._connected = False
        self._reconnect_backoff = 1.0
        self._next_connect_ts = 0.0
        self._columns = (
            ("id", "id", 4),
            ("pos", "pos", 8),
            ("moving", "moving", 6),
            ("awake", "awake", 6),
            ("homed", "homed", 6),
            ("steps_since_home", "steps_since_home", 18),
            ("budget_s", "budget_s", 10),
            ("ttfc_s", "ttfc_s", 8),
            ("est_ms", "est_ms", 10),
            ("started_ms", "started_ms", 12),
            ("actual_ms", "actual_ms", 12),
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
        client = None
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

            client.loop_start()
            with self._lock:
                self._connected = False
                self._reconnect_backoff = 1.0
                self._next_connect_ts = 0.0

            while not self._stop_event.is_set():
                attempt = False
                with self._lock:
                    connected = self._connected
                    next_ts = self._next_connect_ts
                if not connected:
                    now = time.time()
                    if next_ts == 0.0 or now >= next_ts:
                        attempt = True
                if attempt:
                    try:
                        client.connect(host, port, keepalive=30)
                        with self._lock:
                            self._next_connect_ts = float("inf")
                    except Exception as exc:
                        delay = None
                        with self._lock:
                            delay = self._reconnect_backoff
                            self._reconnect_backoff = min(self._reconnect_backoff * 2.0, 30.0)
                            self._next_connect_ts = time.time() + delay
                        self._append_log(f"[mqtt error] {exc}")
                        self._append_log(f"[mqtt] reconnect in {delay:.1f}s")
                    time.sleep(0.1)
                else:
                    time.sleep(0.1)

        except Exception as exc:
            with self._lock:
                self._error = str(exc)
            self._append_log(f"[mqtt error] {exc}")
        finally:
            if client is not None:
                try:
                    client.loop_stop()
                except Exception:
                    pass
                try:
                    client.disconnect()
                except Exception:
                    pass

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
            for device, data in self._devices.items():
                last_seen = float(data.get("last_seen", 0.0))
                age_s = max(0.0, now - last_seen) if last_seen else 0.0
                motors = data.get("motors", {})
                for motor_id, motor in motors.items():
                    row = dict(motor)
                    row["device"] = device
                    row["node_state"] = data.get("node_state", "")
                    row["ip"] = data.get("ip", "")
                    row["last_seen"] = last_seen
                    row["age_s"] = age_s
                    rows.append(row)
            def _id_key(value: object) -> object:
                try:
                    return int(str(value))
                except (ValueError, TypeError):
                    return str(value)

            rows.sort(key=lambda r: (str(r.get("device", "")), _id_key(r.get("id", ""))))
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
        topic = "devices/+/status"
        client.subscribe(topic, qos=0)
        with self._lock:
            self._connected = True
            self._reconnect_backoff = 1.0
            self._next_connect_ts = float("inf")
        self._append_log(f"[mqtt] connected; subscribed to {topic}")

    def _on_disconnect(self, client, userdata, rc):
        with self._lock:
            self._connected = False
        if not self._stop_event.is_set():
            self._append_log("[mqtt] disconnected")
            with self._lock:
                delay = self._reconnect_backoff
                self._reconnect_backoff = min(self._reconnect_backoff * 2.0, 30.0)
                self._next_connect_ts = time.time() + delay
            self._append_log(f"[mqtt] reconnect in {delay:.1f}s")

    def _on_message(self, client, userdata, msg):
        payload = msg.payload.decode(errors="ignore") if isinstance(msg.payload, bytes) else str(msg.payload)
        self.ingest_message(msg.topic, payload)

    # ------------------------------------------------------------------
    def ingest_message(self, topic: str, payload: str, timestamp: Optional[float] = None) -> None:
        mac = topic.split("/")[1] if topic.startswith("devices/") else topic
        ts = timestamp if timestamp is not None else time.time()

        try:
            obj = json.loads(payload)
        except (ValueError, TypeError, json.JSONDecodeError):
            self._append_log(f"[mqtt] unable to parse status payload from {topic}")
            return

        if not isinstance(obj, dict):
            return

        motors_obj = obj.get("motors", {})
        if not isinstance(motors_obj, dict):
            motors_obj = {}

        def bool_to_flag(value: object) -> str:
            return "1" if bool(value) else "0"

        def format_fixed(value: object) -> str:
            try:
                return f"{float(value):.1f}"
            except (TypeError, ValueError):
                return ""

        motors: Dict[str, Dict[str, str]] = {}
        for key, motor in motors_obj.items():
            if not isinstance(motor, dict):
                continue
            motor_row: Dict[str, str] = {}
            motor_id = motor.get("id", key)
            try:
                motor_int = int(motor_id)
                motor_row["id"] = str(motor_int)
            except (TypeError, ValueError):
                motor_row["id"] = str(motor_id)
            motor_row["pos"] = str(motor.get("position", ""))
            motor_row["moving"] = bool_to_flag(motor.get("moving", False))
            motor_row["awake"] = bool_to_flag(motor.get("awake", False))
            motor_row["homed"] = bool_to_flag(motor.get("homed", False))
            motor_row["steps_since_home"] = str(motor.get("steps_since_home", ""))
            motor_row["budget_s"] = format_fixed(motor.get("budget_s", ""))
            motor_row["ttfc_s"] = format_fixed(motor.get("ttfc_s", ""))
            motor_row["speed"] = str(motor.get("speed", ""))
            motor_row["accel"] = str(motor.get("accel", ""))
            motor_row["est_ms"] = str(motor.get("est_ms", ""))
            motor_row["started_ms"] = str(motor.get("started_ms", ""))
            if "actual_ms" in motor and motor.get("actual_ms") is not None:
                motor_row["actual_ms"] = str(motor.get("actual_ms"))
            else:
                motor_row["actual_ms"] = ""
            motors[motor_row["id"]] = motor_row

        with self._lock:
            entry = self._devices.get(mac, {})
            entry["node_state"] = str(obj.get("node_state", ""))
            entry["ip"] = str(obj.get("ip", ""))
            entry["motors"] = motors
            entry["last_seen"] = ts
            self._devices[mac] = entry
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

    def get_device_summaries(self) -> Dict[str, Dict[str, object]]:
        with self._lock:
            now = time.time()
            summaries: Dict[str, Dict[str, object]] = {}
            for device, data in self._devices.items():
                last_seen = float(data.get("last_seen", 0.0))
                age_s = max(0.0, now - last_seen) if last_seen else float("inf")
                summaries[device] = {
                    "node_state": data.get("node_state", ""),
                    "ip": data.get("ip", ""),
                    "age_s": age_s,
                    "last_seen": last_seen,
                }
            return summaries
