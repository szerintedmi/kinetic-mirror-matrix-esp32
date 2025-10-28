from __future__ import annotations

import threading
import time
from dataclasses import dataclass, field
from typing import Callable, Dict, List, Optional, Sequence

try:
    import serial  # type: ignore
except Exception:  # pragma: no cover - handled by caller
    serial = None  # type: ignore


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
    last_rx: float = 0.0
    lines: List[str] = field(default_factory=list)


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
        self._background_cids: Dict[int, float] = {}
        self._last_status_rows: List[Dict[str, str]] = []
        self._last_status_text: str = ""
        self._net_info: Dict[str, str] = {}
        self._thermal_state: Optional[tuple] = None
        self._need_thermal_refresh: bool = True
        self._need_net_status_refresh: bool = False
        self._next_poll_ts: float = time.monotonic()
        self._last_update_ts: float = 0.0
        self._last_help_text: str = ""
        self._had_disconnect: bool = False
        self._reconnect_dots: int = 0

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
            )

    def get_net_info(self) -> Dict[str, str]:
        with self._lock:
            return dict(self._net_info)

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
        if self._need_thermal_refresh:
            self._send_poll("GET THERMAL_LIMITING\n")
            self._need_thermal_refresh = False
            return
        if self._need_net_status_refresh:
            self._send_poll("NET:STATUS\n")
            self._need_net_status_refresh = False
            return
        if now < self._next_poll_ts:
            return
        # Poll STATUS followed by NET:STATUS on alternating cycles
        self._send_poll("STATUS\n")
        self._need_net_status_refresh = True
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

        if upper.startswith("CTRL:ACK") or upper.startswith("CTRL:ERR"):
            pending.ack_seen = True
            cid = self._extract_cid(line)
            if cid is not None and cid in self._background_cids:
                self._background_cids.pop(cid, None)

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
        if should_log or upper.startswith("NET:LIST") or upper.startswith("SSID="):
            self._log_line(line)

    def _maybe_finish_pending(self, now: float) -> None:
        pending = self._pending
        if pending is None:
            return
        idle = now - (pending.last_rx or pending.started_at)
        if pending.stream_deadline and now < pending.stream_deadline:
            return
        if pending.ack_seen and idle >= 0.1:
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
            self._need_net_status_refresh = True

        self._pending = None

    def _update_status_cache(self, text: str) -> None:
        raw_lines = [ln.strip() for ln in text.splitlines() if ln.strip()]
        payload = "\n".join(
            [ln for ln in raw_lines if not ln.upper().startswith("CTRL:")]
        )
        rows = self._parse_status(payload)
        with self._lock:
            self._last_status_rows = rows
            self._last_status_text = payload
            self._last_update_ts = time.time()

    def _update_net_cache(self, text: str) -> None:
        raw_lines = [ln.strip() for ln in text.splitlines() if ln.strip()]
        net = {}
        if raw_lines:
            net = self._parse_kv(raw_lines[-1])
        with self._lock:
            self._net_info = net

    # ------------------------------------------------------------------
    # Logging helpers
    # ------------------------------------------------------------------
    def _log_async(self, line: str) -> None:
        # Async NET lines may arrive outside of active command sessions
        upper = line.upper()
        if not (
            upper.startswith("CTRL: NET:")
            or (upper.startswith("CTRL:ERR") and " NET_" in upper)
        ):
            self._maybe_track_background_cid(line)
            return
        self._log_line(line)

    def _log_line(self, line: str) -> None:
        self._maybe_track_background_cid(line)
        with self._lock:
            self._log.append(line)

    def _maybe_track_background_cid(self, line: str) -> None:
        cid = self._extract_cid(line)
        if cid is None:
            return
        now = time.monotonic()
        self._background_cids[cid] = now + 2.0
        stale = [k for k, t in self._background_cids.items() if t < now]
        for k in stale:
            self._background_cids.pop(k, None)

    # ------------------------------------------------------------------
    def _extract_cid(self, line: str) -> Optional[int]:
        for tok in line.split():
            if tok.startswith("CID="):
                try:
                    return int(tok.split("=", 1)[1])
                except Exception:
                    return None
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
