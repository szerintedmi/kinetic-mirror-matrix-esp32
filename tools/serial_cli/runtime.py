from __future__ import annotations

import threading
import time
from dataclasses import dataclass, field
from typing import Callable, Dict, List, Optional, Sequence

try:
    import serial  # type: ignore
except Exception:  # pragma: no cover - handled by caller
    serial = None  # type: ignore

from .response_events import EventType, ResponseEvent, format_event, parse_serial_line

ParseStatusFn = Callable[[str], List[Dict[str, str]]]
ParseKvFn = Callable[[str], Dict[str, str]]
ParseThermalFn = Callable[[str], Optional[tuple]]


@dataclass
class PendingCommand:
    name: str
    is_poll: bool
    started_at: float
    deadline: float
    stream_deadline: float = 0.0
    ack_seen: bool = False
    done_seen: bool = False
    last_rx: float = 0.0
    lines: List[str] = field(default_factory=list)
    events: List[ResponseEvent] = field(default_factory=list)
    cmd_id: Optional[str] = None
    ack_latency_ms: Optional[float] = None


class SerialWorker(threading.Thread):
    """Line-oriented command/response pump for the interactive CLI."""

    def __init__(
        self,
        port: str,
        baud: int,
        timeout: float,
        poll_rate_hz: float,
        *,
        serial_module,
        parse_status: ParseStatusFn,
        parse_kv: ParseKvFn,
        parse_thermal: ParseThermalFn,
    ) -> None:
        super().__init__(daemon=True)
        self._serial = serial_module
        self.port = port
        self.baud = baud
        self.base_timeout = max(0.5, timeout)
        self.poll_interval = 1.0 / max(0.1, poll_rate_hz)
        self._lock = threading.Lock()
        self._stop_event = threading.Event()
        self._log: List[str] = []
        self._cmdq: List[str] = []
        self._ser = None
        self._connected_err: Optional[str] = None
        self._pending: Optional[PendingCommand] = None
        self._buffer: str = ""
        self._background_ids: Dict[str, float] = {}
        self._last_status_rows: List[Dict[str, str]] = []
        self._last_status_text: str = ""
        self._net_info: Dict[str, str] = {}
        self._device_id: Optional[str] = None
        self._thermal_state: Optional[tuple] = None
        self._need_thermal_refresh: bool = True
        self._need_net_status_refresh: bool = False
        self._initial_net_status_requested: bool = False
        self._next_poll_ts: float = time.monotonic()
        self._last_update_ts: float = 0.0
        self._last_help_text: str = ""
        self._had_disconnect: bool = False
        self._reconnect_dots: int = 0
        self._net_status_interval = 10.0
        self._last_net_status_poll = 0.0

        self._parse_status = parse_status
        self._parse_kv = parse_kv
        self._parse_thermal = parse_thermal

    # ------------------------------------------------------------------
    # External API (used by CLI / TUI)
    # ------------------------------------------------------------------
    def queue_cmd(self, cmd: str) -> None:
        with self._lock:
            line = cmd if cmd.endswith("\n") else cmd + "\n"
            self._log.append("> " + cmd.strip())
            self._cmdq.append(line)

    def stop(self) -> None:
        self._stop_event.set()

    def get_state(self) -> tuple:
        with self._lock:
            return (
                list(self._last_status_rows),
                list(self._log[-800:]),
                self._connected_err,
                self._last_update_ts,
                self._last_help_text,
                {**self._net_info, "device": self._device_id or ""},
            )

    def get_net_info(self) -> Dict[str, str]:
        with self._lock:
            info = dict(self._net_info)
            if self._device_id:
                info.setdefault("device", self._device_id)
            return info

    def get_thermal_state(self):
        with self._lock:
            return self._thermal_state

    def get_thermal_status_text(self) -> str:
        st = self.get_thermal_state()
        if not st:
            return ""
        enabled, max_budget = st
        prefix = "thermal limiting=ON" if enabled else "thermal limiting=OFF"
        if isinstance(max_budget, int):
            return f"{prefix} (max={max_budget}s)"
        return prefix

    # ------------------------------------------------------------------
    # Thread run loop
    # ------------------------------------------------------------------
    def run(self) -> None:  # pragma: no cover - exercised via integration
        while not self._stop_event.is_set():
            now = time.monotonic()
            try:
                if self._ensure_open():
                    continue

                self._maybe_send_user_command()
                self._maybe_issue_poll(now)
                if self._read_and_process():
                    continue
                self._maybe_finish_pending(now)
                time.sleep(0.02)
            except Exception as exc:
                self._record_disconnect(exc)
                self._close()
                time.sleep(0.5)

    # ------------------------------------------------------------------
    # Transport helpers
    # ------------------------------------------------------------------
    def _ensure_open(self) -> bool:
        if self._ser is not None:
            return False
        if self._serial is None:
            raise RuntimeError("pyserial not installed; install 'pyserial'")
        self._ser = self._serial.Serial(self.port, self.baud, timeout=0.02)
        try:
            self._ser.reset_input_buffer()
            self._ser.reset_output_buffer()
        except Exception:
            pass
        with self._lock:
            if self._had_disconnect:
                self._log.append("[reconnected]")
            self._had_disconnect = False
            self._reconnect_dots = 0
            self._need_net_status_refresh = True
            self._initial_net_status_requested = False
        return True

    def _close(self) -> None:
        try:
            if self._ser:
                self._ser.close()
        except Exception:
            pass
        self._ser = None

    # ------------------------------------------------------------------
    # Command scheduling
    # ------------------------------------------------------------------
    def _maybe_send_user_command(self) -> None:
        if self._pending is not None:
            return
        cmd = None
        with self._lock:
            if self._cmdq:
                cmd = self._cmdq.pop(0)
        if cmd is None:
            return
        try:
            assert self._ser is not None
            self._ser.write(cmd.encode())
            self._pending = PendingCommand(
                name=cmd.strip(),
                is_poll=False,
                started_at=time.monotonic(),
                deadline=time.monotonic() + self.base_timeout,
            )
        except Exception as exc:
            with self._lock:
                self._connected_err = str(exc)
            raise

    def _maybe_issue_poll(self, now: float) -> None:
        if self._pending is not None or self._stop_event.is_set():
            return
        if not self._initial_net_status_requested:
            self._send_poll("NET:STATUS\n")
            self._initial_net_status_requested = True
            self._need_net_status_refresh = False
            self._last_net_status_poll = now
            return
        if self._need_thermal_refresh:
            self._send_poll("GET THERMAL_LIMITING\n")
            self._need_thermal_refresh = False
            return
        if self._need_net_status_refresh:
            if now - self._last_net_status_poll >= self._net_status_interval:
                self._send_poll("NET:STATUS\n")
                self._need_net_status_refresh = False
                self._last_net_status_poll = now
                return
            return
        if now < self._next_poll_ts:
            return
        # Poll STATUS followed by NET:STATUS on alternating cycles
        self._send_poll("STATUS\n")
        self._next_poll_ts = now + self.poll_interval

    def _send_poll(self, line: str) -> None:
        try:
            assert self._ser is not None
            self._ser.write(line.encode())
            self._pending = PendingCommand(
                name=line.strip(),
                is_poll=True,
                started_at=time.monotonic(),
                deadline=time.monotonic() + self.base_timeout,
            )
            if line.strip().upper().startswith("NET:STATUS"):
                self._last_net_status_poll = time.monotonic()
        except Exception as exc:
            with self._lock:
                self._connected_err = str(exc)
            raise

    # ------------------------------------------------------------------
    # Serial reading / parsing
    # ------------------------------------------------------------------
    def _read_and_process(self) -> bool:
        assert self._ser is not None
        available = 0
        try:
            available = int(getattr(self._ser, "in_waiting", 0))
        except Exception:
            available = 0
        size = available if available > 0 else 1
        chunk = self._ser.read(size)
        if not chunk:
            return False
        try:
            text = chunk.decode(errors="replace")
        except Exception:
            text = chunk.decode(errors="ignore")
        self._buffer += text
        while True:
            idx = self._buffer.find("\n")
            if idx == -1:
                break
            line = self._buffer[:idx].strip()
            self._buffer = self._buffer[idx + 1 :]
            if line:
                self._handle_line(line)
        return True

    def _handle_line(self, line: str) -> None:
        pending = self._pending
        if pending is None:
            self._log_async(line)
            return

        # Drop device echo
        if line == pending.name:
            return

        now = time.monotonic()
        pending.lines.append(line)
        pending.last_rx = now
        upper = line.upper()

        event = parse_serial_line(line)
        handled_event = False
        if event is not None:
            if event.cmd_id and not pending.cmd_id:
                pending.cmd_id = event.cmd_id
            if event.event_type == EventType.ACK:
                pending.ack_seen = True
                pending.ack_latency_ms = (now - pending.started_at) * 1000.0
                pending.deadline = max(pending.deadline, now + self.base_timeout)
            if event.event_type == EventType.DONE:
                pending.done_seen = True
            pending.events.append(event)
            self._log_event(event, pending)
            handled_event = True
        else:
            if upper.startswith("CTRL:ACK") or upper.startswith("CTRL:ERR"):
                pending.ack_seen = True
                msg_id = self._extract_msg_id(line)
                if msg_id is not None and msg_id in self._background_ids:
                    self._background_ids.pop(msg_id, None)
            if upper.startswith("CTRL:DONE"):
                pending.done_seen = True
            self._log_line(line, pending)

        if "SCANNING=1" in upper:
            pending.stream_deadline = max(
                pending.stream_deadline, now + max(3.0, self.base_timeout * 2.0)
            )
            pending.deadline = max(pending.deadline, now + max(6.0, self.base_timeout * 3.0))
        elif upper.startswith("NET:LIST"):
            pending.stream_deadline = max(
                pending.stream_deadline, now + max(2.0, self.base_timeout * 1.5)
            )
            pending.deadline = max(pending.deadline, now + max(4.0, self.base_timeout * 2.0))
        elif upper.startswith("SSID="):
            pending.stream_deadline = max(
                pending.stream_deadline, now + max(0.5, self.base_timeout / 3.0)
            )

        should_log = True
        if pending.is_poll:
            # Keep NET async state changes, drop simple ACKs/data rows
            if not (
                upper.startswith("CTRL: NET:")
                or (upper.startswith("CTRL:ERR") and " NET_" in upper)
            ):
                should_log = False
        if handled_event:
            should_log = False
        if should_log or upper.startswith("NET:LIST") or upper.startswith("SSID="):
            self._log_line(line, pending)

    def _maybe_finish_pending(self, now: float) -> None:
        pending = self._pending
        if pending is None:
            return
        idle = now - (pending.last_rx or pending.started_at)
        if pending.stream_deadline and now < pending.stream_deadline:
            return
        if pending.done_seen:
            if idle >= 0.05:
                self._finalize_pending()
            return
        if pending.is_poll:
            if pending.ack_seen and idle >= 0.05:
                self._finalize_pending()
            elif now >= pending.deadline:
                self._finalize_pending(timeout=True)
            return
        if pending.ack_seen:
            if now >= pending.deadline:
                self._finalize_pending(timeout=True)
            return
        if idle >= 0.1:
            self._finalize_pending()
        elif now >= pending.deadline:
            self._finalize_pending(timeout=True)

    # ------------------------------------------------------------------
    # Pending completion
    # ------------------------------------------------------------------
    def _finalize_pending(self, timeout: bool = False) -> None:
        pending = self._pending
        if pending is None:
            return
        now = time.monotonic()
        lines = pending.lines
        text = "\n".join(lines).strip()
        name = pending.name.upper()
        if not lines and not pending.is_poll:
            self._log_line("> (no response)")
        elif not pending.ack_seen and not pending.is_poll and timeout:
            self._log_line("> (no response)")

        if name.startswith("STATUS"):
            self._update_status_cache(text)
        elif name.startswith("NET:STATUS"):
            self._update_net_cache(text)
        elif name.startswith("HELP"):
            with self._lock:
                if text:
                    self._last_help_text = text
        elif name.startswith("GET THERMAL"):
            parsed = self._parse_thermal(text)
            if parsed:
                with self._lock:
                    self._thermal_state = parsed
        elif name.startswith("SET THERMAL"):
            self._need_thermal_refresh = True

        if name.startswith("STATUS"):
            # Queue NET:STATUS on the next idle loop to keep pace
            if now - self._last_net_status_poll >= self._net_status_interval:
                self._need_net_status_refresh = True

        self._pending = None

    def _update_status_cache(self, text: str) -> None:
        raw_lines = [ln.strip() for ln in text.splitlines() if ln.strip()]
        payload_lines = [ln for ln in raw_lines if not ln.upper().startswith("CTRL:")]
        payload = "\n".join(payload_lines)
        rows = self._parse_status(payload)
        metadata: Dict[str, str] = {}
        for ln in raw_lines:
            for tok in ln.split():
                low = tok.lower()
                if low.startswith("device="):
                    metadata["device"] = _strip_quotes(tok.split("=", 1)[1])
                elif low.startswith("ssid="):
                    metadata.setdefault("ssid", _strip_quotes(tok.split("=", 1)[1]))
                elif low.startswith("ip="):
                    metadata.setdefault("ip", _strip_quotes(tok.split("=", 1)[1]))
                elif low.startswith("node_state="):
                    metadata.setdefault("node_state", _strip_quotes(tok.split("=", 1)[1]))
        with self._lock:
            self._last_status_rows = rows
            self._last_status_text = payload
            self._last_update_ts = time.time()
            for row in rows:
                dev = row.get("device")
                if dev:
                    self._device_id = dev
                    break
            ssid = None
            ip = None
            for row in rows:
                ssid = row.get("ssid") or ssid
                ip = row.get("ip") or ip
            device_meta = metadata.get("device")
            if device_meta:
                self._device_id = device_meta
            ssid = metadata.get("ssid", ssid)
            ip = metadata.get("ip", ip)
            if ssid or ip:
                net = dict(self._net_info)
                if ssid:
                    net["ssid"] = ssid
                if ip:
                    net["ip"] = ip
                node_state = metadata.get("node_state")
                if node_state:
                    net.setdefault("node_state", node_state)
                self._net_info = net

    def _update_net_cache(self, text: str) -> None:
        raw_lines = [ln.strip() for ln in text.splitlines() if ln.strip()]
        net = {}
        if raw_lines:
            net = self._parse_kv(raw_lines[-1])
        with self._lock:
            merged = dict(self._net_info)
            for key, value in net.items():
                merged[key] = _strip_quotes(value)
            self._net_info = merged
            device = merged.get("device")
            if device:
                self._device_id = device

    # ------------------------------------------------------------------
    # Logging helpers
    # ------------------------------------------------------------------
    def _log_async(self, line: str) -> None:
        event = parse_serial_line(line)
        if event is not None:
            self._log_event(event, None)
            return
        msg_id = self._extract_msg_id(line)
        self._maybe_track_background_id(msg_id)

    def _log_line(self, line: str, pending: Optional[PendingCommand] = None) -> None:
        if pending is not None and pending.is_poll:
            upper = line.upper()
            if not upper.startswith("CTRL:") and not upper.startswith("NET:"):
                return
        self._maybe_track_background_id(self._extract_msg_id(line))
        with self._lock:
            self._log.append(line)

    def _log_event(self, event: ResponseEvent, pending: Optional[PendingCommand]) -> None:
        msg_id = event.msg_id or event.cmd_id
        self._maybe_track_background_id(msg_id)
        latency = None
        if pending and pending.is_poll:
            if event.event_type == EventType.DATA:
                return
            if event.event_type in (EventType.ACK, EventType.DONE) and not event.code:
                return
            if event.event_type == EventType.ACK:
                pending.ack_latency_ms = None
        elif pending and event.event_type == EventType.ACK and pending.ack_latency_ms is not None:
            latency = pending.ack_latency_ms
        formatted = format_event(event, latency_ms=latency)
        with self._lock:
            self._log.append(formatted)

    def _maybe_track_background_id(self, msg_id: Optional[str]) -> None:
        if not msg_id:
            return
        now = time.monotonic()
        self._background_ids[msg_id] = now + 2.0
        stale = [k for k, t in self._background_ids.items() if t < now]
        for k in stale:
            self._background_ids.pop(k, None)

    # ------------------------------------------------------------------
    def _extract_msg_id(self, line: str) -> Optional[str]:
        for tok in line.split():
            if tok.startswith("msg_id="):
                value = tok.split("=", 1)[1]
                if value:
                    return value
        return None

    def _record_disconnect(self, exc: Exception) -> None:
        with self._lock:
            self._connected_err = str(exc)
            if not self._had_disconnect:
                self._log.append(f"[disconnect] {exc}")
                self._had_disconnect = True
                self._reconnect_dots = 0
            recon_prefix = f"Reconnecting to {self.port} "
            dots = "." * (self._reconnect_dots + 1)
            if self._log and self._log[-1].startswith(recon_prefix):
                self._log[-1] = recon_prefix + dots
            else:
                self._log.append(recon_prefix + dots)
            self._reconnect_dots += 1
def _strip_quotes(value: str) -> str:
    if len(value) >= 2 and value[0] == value[-1] == '"':
        return value[1:-1]
    return value
