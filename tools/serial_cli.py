#!/usr/bin/env python3
import argparse
import sys
import time

try:
    import serial  # type: ignore
except Exception:
    serial = None  # allow --dry-run without pyserial


def build_command(ns: argparse.Namespace) -> str:
    cmd = ns.command
    if cmd == "help":
        return "HELP\n"
    if cmd == "status":
        return "STATUS\n"
    if cmd == "wake":
        return f"WAKE:{ns.id}\n"
    if cmd == "sleep":
        return f"SLEEP:{ns.id}\n"
    if cmd == "move":
        parts = [str(ns.id), str(ns.abs_steps)]
        if ns.speed is not None:
            parts.append(str(ns.speed))
        if ns.accel is not None:
            parts.append(str(ns.accel))
        return f"MOVE:{','.join(parts)}\n"
    if cmd == "home":
        parts = [str(ns.id)]
        # All optional
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


def read_response(ser, timeout: float) -> str:
    ser.timeout = timeout
    # Read initial line or block
    chunks = []
    start = time.time()
    while time.time() - start < timeout:
        data = ser.read(1024)
        if data:
            chunks.append(data.decode(errors="replace"))
            # heuristic: if we got OK/ERR and nothing more after a short wait, stop
            if "CTRL:OK" in chunks[-1] or "CTRL:ERR" in chunks[-1]:
                # small grace period for multi-line (HELP/STATUS)
                time.sleep(0.05)
                more = ser.read(4096)
                if more:
                    chunks.append(more.decode(errors="replace"))
                break
        else:
            # brief sleep to avoid busy loop
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

    for name in ("wake", "sleep"):
        s = _add_common(sp.add_parser(name, help=f"{name.upper()} a motor or ALL"))
        s.add_argument("id", help="Motor id 0-7 or ALL")

    s = _add_common(sp.add_parser("move", help="MOVE absolute position"))
    s.add_argument("id", help="Motor id 0-7 or ALL")
    s.add_argument("abs_steps", type=int, help="Absolute target steps (-1200..1200)")
    s.add_argument("--speed", type=int, help="Optional speed (steps/s)")
    s.add_argument("--accel", type=int, help="Optional accel (steps/s^2)")

    s = _add_common(sp.add_parser("home", help="HOME with optional params"))
    s.add_argument("id", help="Motor id 0-7 or ALL")
    s.add_argument("--overshoot", type=int, help="Optional overshoot steps")
    s.add_argument("--backoff", type=int, help="Optional backoff steps")
    s.add_argument("--speed", type=int, help="Optional speed (steps/s)")
    s.add_argument("--accel", type=int, help="Optional accel (steps/s^2)")
    s.add_argument("--full-range", type=int, dest="full_range", help="Optional full_range steps")

    return p


def main(argv=None) -> int:
    ns = make_parser().parse_args(argv)
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


if __name__ == "__main__":
    raise SystemExit(main())

