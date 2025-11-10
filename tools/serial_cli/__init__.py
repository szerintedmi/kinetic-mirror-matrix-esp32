import argparse
import shlex
import sys
import time
from typing import Dict, List, Optional, Tuple

try:
    import serial  # type: ignore
except Exception:
    serial = None

from .response_events import EventType, format_event
from .runtime import SerialWorker


def _join_home_with_placeholders(id_token: str, overshoot, backoff, full_range) -> str:
    # Build HOME payload supporting optional fields without legacy speed/accel.
    # Grammar: HOME:<id|ALL>[,<overshoot>][,<backoff>][,<full_range>]\n
    fields = [None, None, None]
    if overshoot is not None:
        fields[0] = str(overshoot)
    if backoff is not None:
        fields[1] = str(backoff)
    if full_range is not None:
        fields[2] = str(full_range)
    hi = -1
    for i in range(len(fields) - 1, -1, -1):
        if fields[i] is not None:
            hi = i
            break
    parts = [id_token]
    if hi >= 0:
        parts.extend([fields[i] if fields[i] is not None else "" for i in range(0, hi + 1)])
    return f"HOME:{','.join(parts)}\n"


def build_command(ns: argparse.Namespace) -> str:
    cmd = ns.command
    if cmd == "help":
        return "HELP\n"
    if cmd == "status" or cmd == "st":
        return "STATUS\n"
    if cmd == "last-op":
        if getattr(ns, "id", None) is None:
            return "GET LAST_OP_TIMING\n"
        return f"GET LAST_OP_TIMING:{ns.id}\n"
    if cmd == "wake":
        return f"WAKE:{ns.id}\n"
    if cmd == "sleep":
        return f"SLEEP:{ns.id}\n"
    if cmd == "move" or cmd == "m":
        # Simplified grammar: no per-move speed/accel
        parts = [str(ns.id), str(ns.abs_steps)]
        return f"MOVE:{','.join(parts)}\n"
    if cmd == "home" or cmd == "h":
        # Simplified grammar: no per-home speed/accel
        return _join_home_with_placeholders(str(ns.id), ns.overshoot, ns.backoff, ns.full_range)
    raise SystemExit(f"Unknown command: {cmd}")


def read_response(ser, timeout: float, idle_grace: float = 0.10) -> str:
    """Read until a quiet period (idle_grace) or timeout.

    This avoids truncating multi-line responses where lines arrive in bursts
    (e.g., multi-command batches returning multiple CTRL:OK lines).
    """
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
        else:
            # If we've received something and the line has gone quiet long enough,
            # treat the response as complete.
            if last_rx is not None and (time.time() - last_rx) >= idle_grace:
                break
            time.sleep(0.01)
    return "".join(chunks).strip()


def parse_thermal_get_response(text: str) -> Optional[Tuple[bool, Optional[int]]]:
    """Parse GET THERMAL_LIMITING response.
    Returns (enabled, max_budget_s) or None if not parseable.
    """
    if not text:
        return None
    enabled = None
    max_budget = None
    try:
        # Expect line like: CTRL:OK THERMAL_LIMITING=ON max_budget_s=90
        for tok in text.strip().split():
            if tok.startswith("THERMAL_LIMITING="):
                val = tok.split("=", 1)[1].strip().upper()
                enabled = val == "ON"
            elif tok.startswith("max_budget_s="):
                try:
                    max_budget = int(tok.split("=", 1)[1])
                except Exception:
                    max_budget = None
        if enabled is None:
            return None
        return (enabled, max_budget)
    except Exception:
        return None


def extract_est_ms_from_ctrl_ok(text: str) -> Optional[int]:
    """Extract est_ms=N from the final CTRL:ACK (or legacy CTRL:OK) line in a response.
    Safe when WARN lines precede ACK; returns None if not present.
    """
    if not text:
        return None
    lines = text.splitlines()
    for ln in reversed(lines):
        ln = ln.strip()
        if ln.startswith("CTRL:ACK") or ln.startswith("CTRL:OK"):
            for part in ln.split():
                if part.startswith("est_ms="):
                    try:
                        return int(part.split("=", 1)[1])
                    except Exception:
                        return None
            return None
    return None


def _add_common(sub):
    sub.add_argument("--port", "-p", help="Serial port (e.g., /dev/ttyUSB0, COM3)")
    sub.add_argument("--baud", "-b", type=int, default=115200, help="Baud rate (default 115200)")
    sub.add_argument(
        "--timeout", "-t", type=float, default=2.0, help="Read timeout seconds (default 2.0)"
    )
    sub.add_argument(
        "--dry-run", action="store_true", help="Print command only; do not open serial"
    )
    sub.add_argument(
        "--transport",
        choices=("serial", "mqtt"),
        default="mqtt",
        help="Transport to use (serial or mqtt). Default mqtt.",
    )
    sub.add_argument("--node", help="Target node id (MQTT transport)")
    return sub


def make_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="serial_cli", description="Serial CLI for Mirror Array protocol v1"
    )
    sp = p.add_subparsers(dest="command", required=True)

    _add_common(sp.add_parser("help", help="Print device HELP"))
    _add_common(sp.add_parser("status", help="Print STATUS"))
    _add_common(sp.add_parser("st", help="Alias for status"))
    s_lo = _add_common(sp.add_parser("last-op", help="Show last MOVE/HOME timing (device)"))
    s_lo.add_argument("id", nargs="?", help="Motor id 0-7 or ALL; omit to list all")

    for name in ("wake", "sleep"):
        s = _add_common(sp.add_parser(name, help=f"{name.upper()} a motor or ALL"))
        s.add_argument("id", help="Motor id 0-7 or ALL")

    s = _add_common(sp.add_parser("move", help="MOVE absolute position (uses global SPEED/ACCEL)"))
    s.add_argument("id", help="Motor id 0-7 or ALL")
    s.add_argument("abs_steps", type=int, help="Absolute target steps (-1200..1200)")
    s_alias = _add_common(sp.add_parser("m", help="Alias for move"))
    s_alias.add_argument("id", help="Motor id 0-7 or ALL")
    s_alias.add_argument("abs_steps", type=int, help="Absolute target steps (-1200..1200)")

    s = _add_common(
        sp.add_parser("home", help="HOME with optional params (overshoot/backoff/full_range)")
    )
    s.add_argument("id", help="Motor id 0-7 or ALL")
    s.add_argument("--overshoot", type=int, help="Optional overshoot steps")
    s.add_argument("--backoff", type=int, help="Optional backoff steps")
    s.add_argument("--full-range", type=int, dest="full_range", help="Optional full_range steps")
    s_alias = _add_common(sp.add_parser("h", help="Alias for home"))
    s_alias.add_argument("id", help="Motor id 0-7 or ALL")
    s_alias.add_argument("--overshoot", type=int, help="Optional overshoot steps")
    s_alias.add_argument("--backoff", type=int, help="Optional backoff steps")
    s_alias.add_argument(
        "--full-range", type=int, dest="full_range", help="Optional full_range steps"
    )

    # estimation/measurement helpers
    s = _add_common(
        sp.add_parser("check-move", help="Compute estimate and measure actual MOVE duration")
    )
    s.add_argument("id", type=str, help="Motor id 0-7")
    s.add_argument("abs_steps", type=int, help="Absolute target steps (-1200..1200)")
    s.add_argument("--speed", type=int, default=4000, help="Speed (steps/s)")
    s.add_argument("--accel", type=int, default=16000, help="Accel (steps/s^2)")
    s.add_argument("--tolerance", type=float, default=0.25, help="Allowed relative deviation")

    s = _add_common(
        sp.add_parser("check-home", help="Compute estimate and measure actual HOME duration")
    )
    s.add_argument("id", type=str, help="Motor id 0-7 or ALL")
    s.add_argument("--overshoot", type=int, default=800, help="Overshoot steps")
    s.add_argument("--backoff", type=int, default=50, help="Backoff steps")
    s.add_argument("--speed", type=int, default=4000, help="Speed (steps/s)")
    s.add_argument("--accel", type=int, default=16000, help="Accel (steps/s^2)")
    s.add_argument(
        "--full-range", type=int, default=2400, dest="full_range", help="Full range steps"
    )
    s.add_argument("--tolerance", type=float, default=0.25, help="Allowed relative deviation")

    # interactive TUI
    s = _add_common(
        sp.add_parser("interactive", help="Interactive TUI (poll STATUS and accept commands)")
    )
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


def parse_kv_line(text: str) -> Dict[str, str]:
    out: Dict[str, str] = {}
    if not text:
        return out
    # take last line beginning with CTRL:ACK (or legacy CTRL:OK) if present
    lines = [ln.strip() for ln in text.splitlines() if ln.strip()]
    if not lines:
        return out
    ln = lines[-1]
    upper = ln.upper()
    if upper.startswith("CTRL:"):
        try:
            parts = shlex.split(ln)
        except ValueError:
            parts = ln.split()
        for tok in parts[1:]:
            if "=" in tok:
                k, v = tok.split("=", 1)
                out[k] = v
    return out


def _render_presence_table(rows: List[Dict[str, str]]) -> str:
    cols = [
        ("device", 18, "device"),
        ("state", 8, "state"),
        ("ip", 16, "ip"),
        ("age_s", 8, "age_s"),
        ("msg_id", 36, "msg_id"),
    ]
    header = " ".join([label[:width].rjust(width) for key, width, label in cols])
    lines = [header, "-" * len(header)]
    for entry in rows:
        line_parts = []
        for key, width, label in cols:
            if key == "age_s":
                val = entry.get("age_s", "")
                if (val is None or val == "") and "last_seen" in entry:
                    try:
                        now = time.time()
                        val = max(0.0, now - float(entry["last_seen"]))
                    except Exception:
                        val = ""
                try:
                    val = f"{float(val):.1f}"
                except Exception:
                    val = ""
            else:
                val = entry.get(label, "")
            line_parts.append(str(val)[:width].rjust(width))
        lines.append(" ".join(line_parts))
    if len(lines) == 2:
        lines.append("(no presence updates yet)".rjust(len(header)))
    return "\n".join(lines)


def render_table(rows: List[Dict[str, str]]) -> str:
    if rows and isinstance(rows[0], dict) and "device" in rows[0] and "id" not in rows[0]:
        return _render_presence_table(rows)
    cols = [
        ("id", 2, "id"),
        ("pos", 6, "pos"),
        ("moving", 6, "moving"),
        ("awake", 6, "awake"),
        ("homed", 6, "homed"),
        ("steps_since_home", 18, "steps_since_home"),
        ("budget_s", 15, "budget_s"),
        ("ttfc_s", 7, "ttfc_s"),
        # New timing columns (mirrors GET LAST_OP_TIMING fields)
        ("est_ms", 8, "est_ms"),
        ("started_ms", 10, "started_ms"),
        ("actual_ms", 9, "actual_ms"),
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


def _make_mqtt_worker(
    *, broker_overrides: Optional[Dict[str, str]] = None, node: Optional[str] = None, **kwargs
):
    from .mqtt_runtime import MqttWorker, load_mqtt_defaults

    broker = load_mqtt_defaults()
    if broker_overrides:
        broker.update(broker_overrides)
    return MqttWorker(broker=broker, node_id=node, **kwargs)


def _run_status_mqtt(ns) -> int:
    wait_seconds = max(0.5, float(getattr(ns, "timeout", 1.0)))
    try:
        worker = _make_mqtt_worker(node=getattr(ns, "node", None))
    except RuntimeError as exc:
        print(exc, file=sys.stderr)
        return 2
    worker.start()
    try:
        time.sleep(wait_seconds)
        rows, log, err, _last_update, _ = worker.get_state()
        if err:
            print(f"error: {err}", file=sys.stderr)
            return 1
        table = render_table(rows)
        if table:
            print(table)
        else:
            print("No MQTT telemetry received yet.")
        if log:
            print("\nRecent MQTT events:")
            for ln in log[-10:]:
                print(ln)
    finally:
        worker.stop()
        worker.join(timeout=1.0)
    return 0


def run_command_mqtt(worker, ns) -> int:
    cmd = build_command(ns).strip()
    if ns.dry_run:
        sys.stdout.write(cmd + "\n")
        return 0

    timeout = max(1.0, float(getattr(ns, "timeout", 3.0)))
    worker.start()
    try:
        if not worker.wait_until_connected(timeout):
            print("error: mqtt broker not connected", file=sys.stderr)
            return 1

        handles = worker.queue_cmd(cmd)
        if not handles:
            print("error: unable to publish command", file=sys.stderr)
            return 1

        summary = worker.wait_for_completion(handles, timeout)
        exit_code = 0
        lines: List[str] = []
        for handle in handles:
            pending = summary.get(handle)
            if not pending:
                lines.append("warning: no response received")
                exit_code = max(exit_code, 1)
                continue
            events = pending.events
            if not events:
                lines.append("warning: command produced no events")
                exit_code = max(exit_code, 1)
                continue
            for event in events:
                lines.append(format_event(event))
            has_error = any(ev.event_type == EventType.ERROR for ev in events)
            if has_error:
                exit_code = max(exit_code, 1)

        output = "\n".join(lines)
        if output:
            print(output)
        return exit_code
    finally:
        worker.stop()
        worker.join(timeout=1.0)


def main(argv=None) -> int:
    ns = make_parser().parse_args(argv)
    if ns.command in ("status", "st") and ns.transport == "mqtt":
        return _run_status_mqtt(ns)
    if ns.command == "interactive":
        return run_interactive(ns)
    if ns.transport == "mqtt":
        try:
            worker = _make_mqtt_worker(node=getattr(ns, "node", None))
        except RuntimeError as exc:
            print(exc, file=sys.stderr)
            return 2
        return run_command_mqtt(worker, ns)

    # Estimation + measurement utilities
    if ns.command in ("check-move", "check-home"):
        if serial is None:
            print("pyserial not installed. Install 'pyserial'.", file=sys.stderr)
            return 2
        if not ns.port:
            print("--port is required for this command", file=sys.stderr)
            return 2
        import time as _t

        def _ceil_div(a: int, b: int) -> int:
            return (a + b - 1) // b if b > 0 else 0

        def estimate_move_ms(distance: int, speed: int, accel: int) -> int:
            d = abs(int(distance))
            v = max(1, int(speed))
            a = max(1, int(accel))
            d_thresh = _ceil_div(v * v, a)
            if d >= d_thresh:
                return _ceil_div(d * 1000, v) + _ceil_div(v * 1000, a)
            # triangular profile
            import math as _m

            scaled = _ceil_div(d * 1_000_000, a)
            s_ms = _m.isqrt(max(0, scaled))
            if s_ms * s_ms < scaled:
                s_ms += 1
            return 2 * s_ms

        def estimate_home_ms(o: int, b: int, fr: int, v: int, a: int) -> int:
            return (
                estimate_move_ms(fr + o, v, a)
                + estimate_move_ms(b, v, a)
                + estimate_move_ms(fr // 2, v, a)
            )

        try:
            with serial.Serial(ns.port, ns.baud, timeout=ns.timeout) as ser:
                est_ms = None
                if ns.command == "check-move":
                    target = int(ns.abs_steps)
                    # Send simplified MOVE without legacy per-move params
                    ser.write(f"MOVE:{ns.id},{target}\n".encode())
                    resp = read_response(ser, ns.timeout)
                    est_ms = extract_est_ms_from_ctrl_ok(resp)
                else:
                    o = int(ns.overshoot)
                    b = int(ns.backoff)
                    fr = int(ns.full_range)
                    # Send simplified HOME without legacy per-home params
                    cmd = _join_home_with_placeholders(str(ns.id), o, b, fr)
                    ser.write(cmd.encode())
                    resp = read_response(ser, ns.timeout)

                # Extract est_ms from CTRL:OK line (tolerant of preceding WARN)
                if est_ms is None:
                    est_ms = extract_est_ms_from_ctrl_ok(resp)
                if est_ms is None:
                    print("Device did not return est_ms in CTRL:OK", file=sys.stderr)
                    return 1

                start = _t.time()
                # Poll device-side timing endpoint for accurate duration
                target_id = str(ns.id)
                if not target_id:
                    target_id = "0"
                deadline = start + (est_ms / 1000.0) + 5.0
                last_ms_val = None
                while _t.time() < deadline:
                    ser.write(f"GET LAST_OP_TIMING:{target_id}\n".encode())
                    txt = read_response(ser, ns.timeout)
                    ongoing = None
                    last_ms = None
                    for tok in txt.strip().split():
                        if tok.startswith("ongoing="):
                            try:
                                ongoing = int(tok.split("=", 1)[1])
                            except Exception:
                                continue
                        elif tok.startswith("actual_ms="):
                            try:
                                last_ms = int(tok.split("=", 1)[1])
                            except Exception:
                                continue
                    if ongoing == 0 and last_ms is not None:
                        last_ms_val = last_ms
                        break
                    _t.sleep(0.05)
                if last_ms_val is None:
                    print("Timeout waiting for LAST_OP_TIMING", file=sys.stderr)
                    return 1
                actual_ms = int(last_ms_val)
                tol = max(0.0, float(getattr(ns, "tolerance", 0.25)))
                margin = max(100, int(est_ms * tol))
                lower = max(0, est_ms - margin)
                upper = est_ms + margin
                print(f"estimate_ms={est_ms} actual_ms={actual_ms} acceptable=[{lower},{upper}]")
                if actual_ms < lower or actual_ms > upper:
                    return 1
        except Exception as e:
            print(f"Error: {e}", file=sys.stderr)
            return 1
        return 0

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
    try:
        from .tui.textual_ui import TextualUI as UIClass  # type: ignore
    except Exception as e:
        print(
            "textual is required for interactive mode. Install with: pip install textual rich",
            file=sys.stderr,
        )
        print(f"Import error: {e}", file=sys.stderr)
        return 2
    if ns.transport == "mqtt":
        node_id = getattr(ns, "node", None)
        try:
            worker = _make_mqtt_worker(node=node_id)
        except RuntimeError as exc:
            print(exc, file=sys.stderr)
            return 2
    else:
        if ns.dry_run:
            print("'interactive' ignores --dry-run; requires a serial port.", file=sys.stderr)
            return 2
        if serial is None:
            print(
                "pyserial not installed. Install 'pyserial' to use interactive mode.",
                file=sys.stderr,
            )
            return 2
        if not ns.port:
            print("--port is required for interactive mode", file=sys.stderr)
            return 2
        worker = SerialWorker(
            port=ns.port,
            baud=ns.baud,
            timeout=ns.timeout,
            poll_rate_hz=ns.rate,
            serial_module=serial,
            parse_status=parse_status_lines,
            parse_kv=parse_kv_line,
            parse_thermal=parse_thermal_get_response,
        )
    worker.start()
    try:
        if ns.transport == "mqtt" and node_id:
            try:
                worker.queue_cmd("NET:STATUS")
            except Exception as exc:
                worker._log.append(f"error: failed to queue NET:STATUS ({exc})")  # type: ignore[attr-defined]
        ui = UIClass(worker, render_table)  # type: ignore[call-arg]
        rc = ui.run()
    finally:
        worker.stop()
        worker.join(timeout=1.0)
    return rc
