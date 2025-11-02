from __future__ import annotations

import json
import os
import re
import threading
import time
import uuid
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Dict, List, Optional, Sequence, Tuple

try:
    import paho.mqtt.client as mqtt  # type: ignore
except Exception:  # pragma: no cover - dependency handled at runtime
    mqtt = None  # type: ignore

from .command_builder import (
    CommandParseError,
    CommandRequest,
    UnsupportedCommandError,
    build_requests,
)
from .response_events import EventType, ResponseEvent, format_event, parse_mqtt_payload


def _default_cmd_id() -> str:
    """Generate a UUIDv4 string compatible with firmware transport IDs."""
    return str(uuid.uuid4())


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


@dataclass
class PendingCommand:
    local_id: int
    node_id: str
    request: CommandRequest
    raw: str
    enqueued_at: float
    enqueued_monotonic: float
    cmd_id: Optional[str] = None
    ack_event: Optional[ResponseEvent] = None
    done_event: Optional[ResponseEvent] = None
    ack_latency_ms: Optional[float] = None
    completion_latency_ms: Optional[float] = None
    completed: bool = False
    events: List[ResponseEvent] = field(default_factory=list)
    completed_at: float = 0.0
    completed_monotonic: float = 0.0


class MqttWorker(threading.Thread):
    """MQTT telemetry subscriber that mirrors SerialWorker's interface."""

    def __init__(
        self,
        *,
        broker: Optional[Dict[str, str]] = None,
        client_factory: Optional[Callable[[], "mqtt.Client"]] = None,
        node_id: Optional[str] = None,
        cmd_id_factory: Optional[Callable[[], str]] = None,
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
        self._node_id = node_id
        self._condition = threading.Condition(self._lock)
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

        self._pending_by_cmd: Dict[str, PendingCommand] = {}
        self._pending_handles: Dict[int, PendingCommand] = {}
        self._pending_order: List[PendingCommand] = []
        self._next_local_id: int = 1
        self._net_state: Dict[str, str] = {}
        self._requested_net_status = False
        self._thermal_state: Optional[Tuple[bool, Optional[int]]] = None
        self._need_thermal_refresh = True
        self._requested_thermal_status = False
        self._help_text: str = ""
        self._cmd_id_factory: Callable[[], str] = cmd_id_factory or _default_cmd_id

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
    def queue_cmd(self, cmd: str, *, silent: bool = False) -> List[int]:
        stripped = cmd.strip()
        if not stripped:
            return []

        try:
            requests = build_requests(stripped)
        except UnsupportedCommandError as exc:
            if not silent:
                self._append_log(f"> {stripped}")
            self._append_log(f"error: {exc}")
            return []
        except CommandParseError as exc:
            if not silent:
                self._append_log(f"> {stripped}")
            self._append_log(f"error: {exc}")
            return []

        node_id = self._resolve_node_id()
        if not node_id:
            if not silent:
                self._append_log("error: specify --node <id> or wait for telemetry")
            return []

        handles: List[int] = []
        published = False
        for request in requests:
            cmd_id = request.cmd_id or self._cmd_id_factory()
            request.cmd_id = cmd_id
            if not silent:
                self._append_log(f"> {request.raw} (mqtt cmd_id={cmd_id})")
            with self._lock:
                connected = self._connected and self._client is not None
            if not connected or self._client is None:
                if not silent:
                    self._append_log("error: mqtt broker not connected")
                continue

            payload = {
                "action": request.action,
                "params": request.params if request.params else {},
            }
            payload["cmd_id"] = cmd_id
            try:
                data = json.dumps(payload, separators=(",", ":"), sort_keys=False)
            except (TypeError, ValueError) as exc:
                if not silent:
                    self._append_log(f"error: unable to encode payload: {exc}")
                continue

            topic = f"devices/{node_id}/cmd"
            publish_ok = False
            try:
                info = self._client.publish(topic, data, qos=1)  # type: ignore[union-attr]
                rc = getattr(info, "rc", 0) if info is not None else 0
                publish_ok = (rc == 0)
            except Exception as exc:
                if not silent:
                    self._append_log(f"[mqtt] publish error: {exc}")
                publish_ok = False
            if not publish_ok:
                if not silent:
                    self._append_log(f"[mqtt] publish failed topic={topic}")
                continue
            published = True

            if silent:
                continue

            pending = PendingCommand(
                local_id=self._next_local_id,
                node_id=node_id,
                request=request,
                raw=request.raw,
                enqueued_at=time.time(),
                enqueued_monotonic=time.monotonic(),
                cmd_id=cmd_id,
            )
            self._next_local_id += 1
            handles.append(pending.local_id)
            with self._lock:
                self._pending_handles[pending.local_id] = pending
                self._pending_order.append(pending)
                self._pending_by_cmd[cmd_id] = pending
        return handles

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
                self._help_text,
            )

    def get_net_info(self) -> Dict[str, str]:
        with self._lock:
            return {
                "transport": "mqtt",
                "host": str(self._broker.get("host", "")),
                "port": str(self._broker.get("port", "")),
                "state": self._net_state.get("state", ""),
                "ssid": self._net_state.get("ssid", ""),
                "ip": self._net_state.get("ip", ""),
                "device": self._net_state.get("device", ""),
            }

    def get_thermal_state(self):  # pragma: no cover - compatibility stub
        with self._lock:
            return self._thermal_state

    def get_thermal_status_text(self) -> str:  # pragma: no cover - compatibility stub
        state = self.get_thermal_state()
        if not state:
            return ""
        enabled, max_budget = state
        prefix = "thermal limiting=ON" if enabled else "thermal limiting=OFF"
        if isinstance(max_budget, int):
            return f"{prefix} (max={max_budget}s)"
        return prefix

    def _resolve_node_id(self) -> Optional[str]:
        if self._node_id:
            return self._node_id
        with self._lock:
            if not self._devices:
                return None
            if len(self._devices) == 1:
                return next(iter(self._devices.keys()))
            # Prefer the device with freshest telemetry
            freshest = sorted(
                self._devices.items(),
                key=lambda item: float(item[1].get("last_seen", 0.0)),
                reverse=True,
            )
            return freshest[0][0] if freshest else None

    def wait_for_completion(self, handles: Sequence[int], timeout: float = 5.0) -> Dict[int, PendingCommand]:
        deadline = time.time() + max(0.0, timeout)
        handles = list(handles)
        with self._condition:
            while True:
                now = time.time()
                pending_map = {h: self._pending_handles.get(h) for h in handles}
                incomplete = []
                for handle, pending in pending_map.items():
                    if not pending:
                        continue
                    if pending.completed:
                        continue
                    incomplete.append(handle)
                if not incomplete or now >= deadline:
                    break
                remaining = deadline - now
                if remaining <= 0:
                    break
                self._condition.wait(remaining)
            return {
                h: self._pending_handles.get(h)
                for h in handles
                if self._pending_handles.get(h) is not None
            }

    def wait_until_connected(self, timeout: float = 5.0) -> bool:
        deadline = time.time() + max(0.0, timeout)
        with self._condition:
            while not self._connected:
                remaining = deadline - time.time()
                if remaining <= 0:
                    return False
                self._condition.wait(remaining)
        return True

    # ------------------------------------------------------------------
    # MQTT callbacks
    # ------------------------------------------------------------------
    def _on_connect(self, client, userdata, flags, rc):
        topics = [("devices/+/status", 0), ("devices/+/cmd/resp", 1)]
        for topic, qos in topics:
            client.subscribe(topic, qos=qos)
        with self._lock:
            self._connected = True
            self._reconnect_backoff = 1.0
            self._next_connect_ts = float("inf")
            self._condition.notify_all()
        self._append_log("[mqtt] connected; subscribed to devices/+/status, devices/+/cmd/resp")

    def _on_disconnect(self, client, userdata, rc):
        with self._lock:
            self._connected = False
            self._condition.notify_all()
        if not self._stop_event.is_set():
            self._append_log("[mqtt] disconnected")
            with self._lock:
                delay = self._reconnect_backoff
                self._reconnect_backoff = min(self._reconnect_backoff * 2.0, 30.0)
                self._next_connect_ts = time.time() + delay
            self._append_log(f"[mqtt] reconnect in {delay:.1f}s")

    def _on_message(self, client, userdata, msg):
        payload_text = msg.payload.decode(errors="ignore") if isinstance(msg.payload, bytes) else str(msg.payload)
        if msg.topic.endswith("/status"):
            self.ingest_message(msg.topic, payload_text)
            return
        if msg.topic.endswith("/cmd/resp"):
            self.ingest_response(msg.topic, payload_text)
            return
        self._append_log(f"[mqtt] ignored topic {msg.topic}")

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

        should_request_net = False
        should_request_thermal = False
        with self._lock:
            entry = self._devices.get(mac, {})
            entry["node_state"] = str(obj.get("node_state", ""))
            entry["ip"] = str(obj.get("ip", ""))
            entry["motors"] = motors
            entry["last_seen"] = ts
            self._devices[mac] = entry
            self._last_update_ts = ts
            if not self._node_id:
                self._node_id = mac
            if not self._requested_net_status and self._node_id:
                self._requested_net_status = True
                should_request_net = True
            if (
                self._need_thermal_refresh
                and not self._requested_thermal_status
                and self._node_id
            ):
                self._requested_thermal_status = True
                should_request_thermal = True
        if should_request_net:
            handles = self.queue_cmd("NET:STATUS")
            if not handles:
                with self._lock:
                    self._requested_net_status = False
        if should_request_thermal:
            handles = self.queue_cmd("GET THERMAL_LIMITING")
            if not handles:
                with self._lock:
                    self._requested_thermal_status = False
            else:
                with self._lock:
                    self._need_thermal_refresh = False

    def ingest_response(self, topic: str, payload: str) -> None:
        node_id = topic.split("/")[1] if topic.startswith("devices/") else ""
        try:
            obj = json.loads(payload)
        except (ValueError, TypeError, json.JSONDecodeError):
            self._append_log(f"[mqtt] unable to parse command response from {topic}")
            return

        event = parse_mqtt_payload(obj)
        if event is None:
            self._append_log(f"[mqtt] unrecognized response payload from {topic}")
            return
        event.raw = payload
        if not event.action:
            event.action = obj.get("action") or None
        self._handle_event(node_id, event)

    def _handle_event(self, node_id: str, event: ResponseEvent) -> None:
        now_wall = time.time()
        now_mono = time.monotonic()
        latency = None
        with self._lock:
            pending = None
            if event.cmd_id and event.cmd_id in self._pending_by_cmd:
                pending = self._pending_by_cmd[event.cmd_id]
            else:
                # Try to match against earliest pending without cmd_id for this node
                for candidate in self._pending_order:
                    if candidate.completed:
                        continue
                    if candidate.cmd_id and event.cmd_id and candidate.cmd_id != event.cmd_id:
                        continue
                    if node_id and candidate.node_id != node_id:
                        continue
                    pending = candidate
                    break

            if pending:
                if event.cmd_id and pending.cmd_id is None:
                    pending.cmd_id = event.cmd_id
                    self._pending_by_cmd[event.cmd_id] = pending
                pending.events.append(event)
                if event.event_type == EventType.ACK:
                    pending.ack_event = event
                    pending.ack_latency_ms = max(0.0, (now_mono - pending.enqueued_monotonic) * 1000.0)
                    latency = pending.ack_latency_ms
                if event.event_type in (EventType.DONE, EventType.ERROR):
                    pending.done_event = event
                    pending.completion_latency_ms = max(0.0, (now_mono - pending.enqueued_monotonic) * 1000.0)
                    latency = pending.completion_latency_ms
                    pending.completed = True
                    pending.completed_at = now_wall
                    pending.completed_monotonic = now_mono
            if event.action and event.action.upper() == "HELP":
                text = event.attributes.get("text") if event.attributes else None
                self._help_text = text or event.raw or ""

            self._maybe_update_net_state(event)
            self._maybe_update_thermal_state_locked(event, pending)
            formatted = format_event(event, latency_ms=latency)
            self._log.append(formatted)
            if len(self._log) > 800:
                del self._log[: len(self._log) - 800]

            # Clean up completed commands
            if pending and pending.completed:
                if pending.cmd_id and pending.cmd_id in self._pending_by_cmd:
                    self._pending_by_cmd.pop(pending.cmd_id, None)
                self._pending_order = [p for p in self._pending_order if p.local_id != pending.local_id]
                self._cleanup_completed_locked(now_mono)

            self._condition.notify_all()

    def _cleanup_completed_locked(self, now_mono: float) -> None:
        stale_handles = [
            local_id
            for local_id, pending in list(self._pending_handles.items())
            if pending.completed and pending.completed_monotonic and pending.completed_monotonic < now_mono - 30.0
        ]
        for local_id in stale_handles:
            self._pending_handles.pop(local_id, None)

    def _maybe_update_net_state(self, event: ResponseEvent) -> None:
        attrs = event.attributes or {}
        state = attrs.get("state") or attrs.get("STATE")
        ssid = attrs.get("ssid") or attrs.get("SSID")
        ip = attrs.get("ip") or attrs.get("IP")
        device = attrs.get("device") or attrs.get("DEVICE")
        if not (state or ssid or ip or device):
            raw = event.raw or ""
            if raw:
                raw_upper = raw.upper()
                if "STATE=" in raw_upper or "SSID=" in raw_upper or " IP=" in raw_upper or "DEVICE=" in raw_upper:
                    tokens = {tok.split("=", 1)[0].upper(): tok.split("=", 1)[1] for tok in raw.split() if "=" in tok}
                    state = tokens.get("STATE", state)
                    ssid = tokens.get("SSID", ssid)
                    ip = tokens.get("IP", ip)
                    device = tokens.get("DEVICE", device)

        def _trim(value: Optional[str]) -> Optional[str]:
            if value is None:
                return None
            value = value.strip()
            if len(value) >= 2 and value[0] == value[-1] == '"':
                value = value[1:-1]
            return value

        if state or ssid or ip:
            if state:
                self._net_state["state"] = _trim(state) or ""
            if ssid:
                self._net_state["ssid"] = _trim(ssid) or ""
            if ip:
                self._net_state["ip"] = _trim(ip) or ""
            if device:
                self._net_state["device"] = _trim(device) or ""
            self._requested_net_status = True

    # ------------------------------------------------------------------
    def _append_log(self, line: str) -> None:
        with self._lock:
            self._log.append(line)
            if len(self._log) > 800:
                del self._log[: len(self._log) - 800]

    # ------------------------------------------------------------------
    def _maybe_update_thermal_state_locked(self, event: ResponseEvent, pending: Optional[PendingCommand]) -> None:
        attrs = event.attributes or {}
        thermal_key = None
        for key in ("THERMAL_LIMITING", "thermal_limiting"):
            if key in attrs:
                thermal_key = key
                break

        if thermal_key is not None:
            raw_value = str(attrs.get(thermal_key, "")).strip().upper()
            enabled = raw_value != "OFF"
            max_budget: Optional[int] = None
            budget_raw = attrs.get("max_budget_s") or attrs.get("MAX_BUDGET_S")
            if budget_raw is not None:
                try:
                    max_budget = int(float(str(budget_raw)))
                except (ValueError, TypeError):
                    max_budget = None
            self._thermal_state = (enabled, max_budget)
            self._need_thermal_refresh = False
            self._requested_thermal_status = False
            return

        # Fall back to command context to determine whether to refresh.
        command_name = ""
        if pending and pending.request:
            command_name = pending.request.raw.strip().upper()
        elif pending:
            command_name = pending.raw.strip().upper()
        action_name = (event.action or "").strip().upper()

        if command_name.startswith("GET THERMAL_LIMITING") or (
            not command_name and action_name == "GET"
        ):
            if event.event_type in (EventType.DONE, EventType.ERROR):
                self._requested_thermal_status = False
                if event.event_type == EventType.ERROR:
                    self._need_thermal_refresh = True
                else:
                    self._need_thermal_refresh = True
        elif command_name.startswith("SET THERMAL_LIMITING") or (
            not command_name and action_name == "SET" and "THERMAL_LIMITING" in (pending.request.params if pending and pending.request else {})
        ):
            if event.event_type in (EventType.DONE, EventType.ERROR):
                self._need_thermal_refresh = True
                self._requested_thermal_status = False

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
