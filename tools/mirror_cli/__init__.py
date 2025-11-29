"""Serial CLI for Mirror Array protocol v1."""

import sys
import time
from typing import Dict, List, Optional

try:
    import serial  # type: ignore
except Exception:
    serial = None

from .cli import build_command, make_parser
from .parsing import (
    extract_est_ms_from_ctrl_ok,
    parse_kv_line,
    parse_status_lines,
    parse_thermal_get_response,
    read_response,
)
from .rendering import render_table
from .response_events import EventType, format_event
from .runtime import SerialWorker


def _make_mqtt_worker(
    *,
    broker_overrides: Optional[Dict[str, str]] = None,
    node: Optional[str] = None,
    **kwargs,
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
        state = worker.get_state()
        rows, log, err, _last_update, _ = state[:5]
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


def _join_home_with_placeholders(id_token: str, overshoot, backoff, full_range) -> str:
    """Build HOME payload supporting optional fields without legacy speed/accel.

    Grammar: HOME:<id|ALL>[,<overshoot>][,<backoff>][,<full_range>]
    """
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
        print(
            "pyserial not installed. Use --dry-run or install 'pyserial'.",
            file=sys.stderr,
        )
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


def run_interactive(ns) -> int:
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
            print(
                "'interactive' ignores --dry-run; requires a serial port.",
                file=sys.stderr,
            )
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
                worker.append_log(f"error: failed to queue NET:STATUS ({exc})")  # type: ignore[attr-defined]
        ui = UIClass(worker, render_table)  # type: ignore[call-arg]
        rc = ui.run()
    finally:
        worker.stop()
        worker.join(timeout=1.0)
    return rc


# Re-export for backwards compatibility
__all__ = [
    "SerialWorker",
    "build_command",
    "extract_est_ms_from_ctrl_ok",
    "main",
    "make_parser",
    "parse_kv_line",
    "parse_status_lines",
    "parse_thermal_get_response",
    "read_response",
    "render_table",
    "run_command_mqtt",
    "run_interactive",
]
