import argparse
import sys
import time
import threading
from typing import List, Dict, Optional, Tuple

try:
    import serial  # type: ignore
except Exception:
    serial = None


def build_command(ns: argparse.Namespace) -> str:
    cmd = ns.command
    if cmd == "help":
        return "HELP\n"
    if cmd == "status" or cmd == "st":
        return "STATUS\n"
    if cmd == "wake":
        return f"WAKE:{ns.id}\n"
    if cmd == "sleep":
        return f"SLEEP:{ns.id}\n"
    if cmd == "move" or cmd == "m":
        parts = [str(ns.id), str(ns.abs_steps)]
        if ns.speed is not None:
            parts.append(str(ns.speed))
        if ns.accel is not None:
            parts.append(str(ns.accel))
        return f"MOVE:{','.join(parts)}\n"
    if cmd == "home" or cmd == "h":
        parts = [str(ns.id)]
        if ns.overshoot is not None:
            parts.append(str(ns.overshoot))
        if ns.backoff is not None:
            parts.append(str(ns.backoff))
        if ns.speed is not None:
            parts.append(str(ns.speed))
        if ns.accel is not None:
            parts.append(str(ns.accel))
        if ns.full_range is not None:
            parts.append(str(ns.full_range))
        return f"HOME:{','.join(parts)}\n"
    raise SystemExit(f"Unknown command: {cmd}")


def read_response(ser, timeout: float, idle_grace: float = 0.06) -> str:
    ser.timeout = max(0.01, timeout)
    chunks = []
    start = time.time()
    last_rx = None
    while time.time() - start < timeout:
        data = ser.read(1024)
        if data:
            s = data.decode(errors="replace")
            chunks.append(s)
            last_rx = time.time()
            if "CTRL:OK" in s or "CTRL:ERR" in s:
                time.sleep(0.02)
                more = ser.read(4096)
                if more:
                    chunks.append(more.decode(errors="replace"))
                break
        else:
            if last_rx is not None and (time.time() - last_rx) >= idle_grace:
                break
            time.sleep(0.01)
    return "".join(chunks).strip()


def _add_common(sub):
    sub.add_argument("--port", "-p", help="Serial port (e.g., /dev/ttyUSB0, COM3)")
    sub.add_argument("--baud", "-b", type=int, default=115200, help="Baud rate (default 115200)")
    sub.add_argument("--timeout", "-t", type=float, default=2.0, help="Read timeout seconds (default 2.0)")
    sub.add_argument("--dry-run", action="store_true", help="Print command only; do not open serial")
    return sub


def make_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="serial_cli", description="Serial CLI for Mirror Array protocol v1")
    sp = p.add_subparsers(dest="command", required=True)

    _add_common(sp.add_parser("help", help="Print device HELP"))
    _add_common(sp.add_parser("status", help="Print STATUS"))
    _add_common(sp.add_parser("st", help="Alias for status"))

    for name in ("wake", "sleep"):
        s = _add_common(sp.add_parser(name, help=f"{name.upper()} a motor or ALL"))
        s.add_argument("id", help="Motor id 0-7 or ALL")

    s = _add_common(sp.add_parser("move", help="MOVE absolute position"))
    s.add_argument("id", help="Motor id 0-7 or ALL")
    s.add_argument("abs_steps", type=int, help="Absolute target steps (-1200..1200)")
    s.add_argument("--speed", type=int, help="Optional speed (steps/s)")
    s.add_argument("--accel", type=int, help="Optional accel (steps/s^2)")
    s_alias = _add_common(sp.add_parser("m", help="Alias for move"))
    s_alias.add_argument("id", help="Motor id 0-7 or ALL")
    s_alias.add_argument("abs_steps", type=int, help="Absolute target steps (-1200..1200)")
    s_alias.add_argument("--speed", type=int, help="Optional speed (steps/s)")
    s_alias.add_argument("--accel", type=int, help="Optional accel (steps/s^2)")

    s = _add_common(sp.add_parser("home", help="HOME with optional params"))
    s.add_argument("id", help="Motor id 0-7 or ALL")
    s.add_argument("--overshoot", type=int, help="Optional overshoot steps")
    s.add_argument("--backoff", type=int, help="Optional backoff steps")
    s.add_argument("--speed", type=int, help="Optional speed (steps/s)")
    s.add_argument("--accel", type=int, help="Optional accel (steps/s^2)")
    s.add_argument("--full-range", type=int, dest="full_range", help="Optional full_range steps")
    s_alias = _add_common(sp.add_parser("h", help="Alias for home"))
    s_alias.add_argument("id", help="Motor id 0-7 or ALL")
    s_alias.add_argument("--overshoot", type=int, help="Optional overshoot steps")
    s_alias.add_argument("--backoff", type=int, help="Optional backoff steps")
    s_alias.add_argument("--speed", type=int, help="Optional speed (steps/s)")
    s_alias.add_argument("--accel", type=int, help="Optional accel (steps/s^2)")
    s_alias.add_argument("--full-range", type=int, dest="full_range", help="Optional full_range steps")

    # interactive TUI
    s = _add_common(sp.add_parser("interactive", help="Interactive TUI (poll STATUS and accept commands)"))
    s.add_argument("--rate", "-r", type=float, default=2.0, help="Refresh rate in Hz (default 2.0)")

    return p


def parse_status_lines(text: str) -> List[Dict[str, str]]:
    lines = [ln.strip() for ln in text.splitlines() if ln.strip()]
    if not lines:
        return []
    if "," in lines[0] and "=" not in lines[0]:
        headers = [h.strip() for h in lines[0].split(",")]
        rows = []
        for ln in lines[1:]:
            parts = [p.strip() for p in ln.split(",")]
            row = {headers[i]: parts[i] if i < len(parts) else "" for i in range(len(headers))}
            rows.append(row)
        return rows
    out = []
    for ln in lines:
        row: Dict[str, str] = {}
        for tok in ln.split():
            if "=" in tok:
                k, v = tok.split("=", 1)
                row[k] = v
        if row:
            out.append(row)
    return out


def render_table(rows: List[Dict[str, str]]) -> str:
    cols = [
        ("id", 2, "id"),
        ("pos", 6, "pos"),
        ("moving", 6, "moving"),
        ("awake", 6, "awake"),
        ("homed", 6, "homed"),
        ("steps_since_home", 18, "steps_since_homed"),
        ("budget_s", 15, "budget_s"),
        ("ttfc_s", 7, "ttfc_s"),
    ]
    header = " ".join([label[:width].rjust(width) for key, width, label in cols])
    lines = [header, "-" * len(header)]
    for r in rows:
        line = []
        for key, width, _label in cols:
            val = r.get(key, "")
            line.append(str(val).rjust(width))
        lines.append(" ".join(line))
    return "\n".join(lines)


class _SerialWorker(threading.Thread):
    def __init__(self, port: str, baud: int, timeout: float, rate_hz: float):
        super().__init__(daemon=True)
        self.port = port
        self.baud = baud
        self.timeout = timeout
        self.period = 1.0 / max(0.1, rate_hz)
        self._lock = threading.Lock()
        self._stop_event = threading.Event()
        self._last_status_rows: List[Dict[str, str]] = []
        self._last_status_text: str = ""
        self._log: List[str] = []
        self._cmdq: List[str] = []
        self._connected_err: Optional[str] = None
        self._ser = None
        self._last_update_ts: float = 0.0
        self._pending_cmd: Optional[str] = None
        self._pending_buf: str = ""
        self._pending_deadline: float = 0.0
        self._pending_last_rx: float = 0.0
        self._last_help_text: str = ""
        self._had_disconnect: bool = False
        self._reconnect_dots: int = 0

    def get_state(self) -> Tuple[List[Dict[str, str]], List[str], Optional[str], float, str]:
        with self._lock:
            return (
                list(self._last_status_rows),
                list(self._log[-800:]),
                self._connected_err,
                self._last_update_ts,
                self._last_help_text,
            )

    def queue_cmd(self, cmd: str) -> None:
        with self._lock:
            self._cmdq.append(cmd if cmd.endswith("\n") else cmd + "\n")

    def stop(self) -> None:
        self._stop_event.set()

    def _open(self):
        if serial is None:
            raise RuntimeError("pyserial not installed")
        self._ser = serial.Serial(self.port, self.baud, timeout=self.timeout)
        try:
            self._ser.reset_input_buffer()
            self._ser.reset_output_buffer()
            self._ser.inter_byte_timeout = 0.02
            self._ser.write_timeout = 0
        except Exception:
            pass
        self._connected_err = None
        with self._lock:
            if self._had_disconnect:
                self._log.append("[reconnected]")
            self._had_disconnect = False
            self._reconnect_dots = 0

    def _close(self):
        try:
            if self._ser:
                self._ser.close()
        except Exception:
            pass
        self._ser = None

    def run(self):
        next_tick = time.time()
        while not self._stop_event.is_set():
            now = time.time()
            try:
                if self._ser is None:
                    self._open()

                if self._pending_cmd is not None:
                    self._ser.timeout = 0
                    available = 0
                    try:
                        available = int(getattr(self._ser, "in_waiting", 0))
                    except Exception:
                        available = 0
                    if available > 0:
                        data = self._ser.read(available)
                        if data:
                            self._pending_buf += data.decode(errors="replace")
                            self._pending_last_rx = time.time()
                    complete = False
                    if ("CTRL:OK" in self._pending_buf) or ("CTRL:ERR" in self._pending_buf):
                        complete = True
                    elif self._pending_buf and (time.time() - self._pending_last_rx) >= 0.06:
                        complete = True
                    elif time.time() >= self._pending_deadline:
                        complete = True
                    if complete:
                        resp_text = self._pending_buf.strip()
                        raw_lines = resp_text.splitlines()
                        lines = (["> " + raw_lines[0]] + raw_lines[1:]) if raw_lines else ["> (no response)"]
                        with self._lock:
                            self._log.extend(lines)
                            try:
                                if self._pending_cmd and self._pending_cmd.strip().upper().startswith("HELP"):
                                    self._last_help_text = resp_text
                            except Exception:
                                pass
                        self._pending_cmd = None
                        self._pending_buf = ""
                        self._pending_deadline = 0.0
                        self._pending_last_rx = 0.0
                    else:
                        time.sleep(0.01)
                        continue

                cmd = None
                with self._lock:
                    if self._cmdq:
                        cmd = self._cmdq.pop(0)
                if cmd is not None:
                    self._ser.write(cmd.encode())
                    self._pending_cmd = cmd
                    self._pending_buf = ""
                    self._pending_deadline = time.time() + max(0.25, min(self.timeout, 2.0))
                    self._pending_last_rx = 0.0
                    time.sleep(0.01)
                    continue

                if now >= next_tick:
                    self._ser.write(b"STATUS\n")
                    status_text = read_response(self._ser, min(self.timeout, self.period * 0.8))
                    rows = parse_status_lines(status_text)
                    with self._lock:
                        self._last_status_text = status_text
                        self._last_status_rows = rows
                        self._last_update_ts = time.time()
                    next_tick = now + self.period
                else:
                    time.sleep(min(0.02, max(0.0, next_tick - now)))

            except Exception as e:
                with self._lock:
                    self._connected_err = str(e)
                    if not self._had_disconnect:
                        self._log.append(f"[disconnect] {e}")
                        self._had_disconnect = True
                        self._reconnect_dots = 0
                    recon_prefix = f"Reconnecting to {self.port} "
                    dots = "." * (self._reconnect_dots + 1)
                    if self._log and self._log[-1].startswith(recon_prefix):
                        self._log[-1] = recon_prefix + dots
                    else:
                        self._log.append(recon_prefix + dots)
                    self._reconnect_dots += 1
                self._close()
                time.sleep(0.5)
                continue


def main(argv=None) -> int:
    ns = make_parser().parse_args(argv)
    if ns.command == "interactive":
        return run_interactive(ns)

    cmd = build_command(ns)
    if ns.dry_run:
        sys.stdout.write(cmd)
        return 0
    if serial is None:
        print("pyserial not installed. Use --dry-run or install 'pyserial'.", file=sys.stderr)
        return 2
    if not ns.port:
        print("--port is required without --dry-run", file=sys.stderr)
        return 2
    try:
        with serial.Serial(ns.port, ns.baud, timeout=ns.timeout) as ser:
            ser.write(cmd.encode())
            resp = read_response(ser, ns.timeout)
            print(resp)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1
    return 0


def run_interactive(ns: argparse.Namespace) -> int:
    if ns.dry_run:
        print("'interactive' ignores --dry-run; requires a serial port.", file=sys.stderr)
        return 2
    if serial is None:
        print("pyserial not installed. Install 'pyserial' to use interactive mode.", file=sys.stderr)
        return 2
    if not ns.port:
        print("--port is required for interactive mode", file=sys.stderr)
        return 2
    try:
        from .tui.curses_ui import CursesUI  # type: ignore
    except Exception as e:
        print("curses UI is required. On Windows install: pip install windows-curses", file=sys.stderr)
        print(f"Import error: {e}", file=sys.stderr)
        return 2

    worker = _SerialWorker(ns.port, ns.baud, ns.timeout, ns.rate)
    worker.start()
    try:
        ui = CursesUI(worker, render_table)
        rc = ui.run()
    finally:
        worker.stop()
        worker.join(timeout=1.0)
    return rc
