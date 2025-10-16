#!/usr/bin/env python3
"""
Example end-to-end script for Serial Command Protocol v1.

If --port is provided, connects to the device and runs a short sequence.
Otherwise, runs in --dry-run mode and prints the commands that would be sent.
"""
import argparse
import sys
import time
import os

sys.path.append(os.path.abspath(os.path.dirname(__file__)))
from serial_cli import make_parser, build_command, read_response  # type: ignore

try:
    import serial  # type: ignore
except Exception:
    serial = None


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="E2E demo for Serial Protocol v1")
    ap.add_argument("--port", "-p", help="Serial port (e.g., /dev/ttyUSB0, COM3)")
    ap.add_argument(
        "--baud", "-b", type=int, default=115200, help="Baud (default 115200)"
    )
    ap.add_argument("--timeout", "-t", type=float, default=2.0, help="Timeout (s)")
    ns = ap.parse_args(argv)

    cmds = [
        ("help", {}),
        ("status", {}),
        ("wake", {"id": "ALL"}),
        ("sleep", {"id": "ALL"}),
        ("move", {"id": "0", "abs_steps": 10}),
        ("home", {"id": "0"}),
    ]

    if ns.port:
        if serial is None:
            print("pyserial not installed. Install 'pyserial' or run without --port.")
            return 2
        with serial.Serial(ns.port, ns.baud, timeout=ns.timeout) as ser:
            # Drain any banner
            time.sleep(0.1)
            _ = ser.read(4096)
            for name, kw in cmds:
                # Build command string by reusing serial_cli parser
                p = make_parser()
                args = [name]
                for k, v in kw.items():
                    if k in ("id", "abs_steps"):
                        args.append(str(v))
                    else:
                        # map kw to flags if present (not used in this demo)
                        pass
                line = build_command(p.parse_args(args))
                print(f">>> {line.strip()}")
                ser.write(line.encode())
                resp = read_response(ser, ns.timeout)
                print(resp)
                # short wait between commands
                time.sleep(0.2)
    else:
        # Dry-run: just print commands
        for name, kw in cmds:
            p = make_parser()
            args = [name]
            for k, v in kw.items():
                if k in ("id", "abs_steps"):
                    args.append(str(v))
            line = build_command(p.parse_args(args))
            print(f">>> {line.strip()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
