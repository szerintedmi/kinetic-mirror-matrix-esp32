"""
Manual bench routine runner for shared-step motion testing.

This script replays a deterministic series of MOVE/HOME commands so operators can
compare MCU-reported positions with physical marks without editing firmware.
"""

from __future__ import annotations

import argparse
import sys
import time
from typing import Optional, Sequence

try:
    import serial  # type: ignore
except Exception:  # pragma: no cover - pyserial missing on host
    serial = None

from tools.serial_cli import (  # type: ignore[import-not-found]
    extract_est_ms_from_ctrl_ok,
    parse_status_lines,
    read_response,
)

# One-time prep commands run before the repeated pattern.
SETUP_COMMANDS: Sequence[str] = ["SET SPEED=4000;SET ACCEL=16000;SET DECEL=0;HOME:ALL"]

# Hard-coded sequence that emphasizes rhythm changes without per-run tweaking.
PATTERN_COMMANDS: Sequence[str] = [
    # Small Steps forward then back to 0 to surface if small steps are accurate. running 10 repetitions of these moves
    ## sharedStep: huge drifting with 100 steps. Fas: minor drift
    # "MOVE:ALL,100",
    # "MOVE:ALL,200",
    # "MOVE:ALL,300",
    # "MOVE:ALL,400",
    # "MOVE:ALL,500",
    # sharedStep: big drift. Fas: small drift
    # "MOVE:ALL,50",
    # "MOVE:ALL,100",
    # "MOVE:ALL,150",
    # "MOVE:ALL,200",
    # "MOVE:ALL,250",
    # sharedStep: hardly moving with 10 steps, Fas: moving an ca. small amount of drift as with 50 steps
    "MOVE:ALL,30",
    "MOVE:ALL,40",
    "MOVE:ALL,50",
    "MOVE:ALL,60",
    "MOVE:ALL,70",
    "MOVE:ALL,80",
    "MOVE:ALL,90",
    "MOVE:ALL,100",
    "MOVE:ALL,0",
]

FINALIZE_COMMANDS: Sequence[str] = [
    "MOVE:ALL,1200"  # move  to limit - if small steps skipped then motor wont be at limit
]

COMMAND_FALLBACK_WAIT_S = 0.20  # sleep when device skips est_ms
ESTIMATE_GRACE_S = 0
STATUS_POLL_INTERVAL_S = (
    0.2  # once estimate_ms + estimate_grace is reached, poll STATUS
)
STATUS_POLL_TIMEOUT_S = 30.0


def _send_and_log(ser, cmd: str, timeout: float) -> str:
    """Send a single command (or multicommand) and print the device response."""
    payload = cmd if cmd.endswith("\n") else cmd + "\n"
    print(f"\n>>> {cmd}")
    ser.write(payload.encode())
    ser.flush()
    resp = read_response(ser, timeout)
    if resp:
        print(resp)
    else:
        print("(no response)")
    return resp


def _wait_for_idle(ser, timeout: float, est_ms: Optional[int]) -> None:
    """Wait for motion to finish by sleeping near the estimate, then STATUS polling."""
    wait_s = COMMAND_FALLBACK_WAIT_S
    if est_ms is not None:
        wait_s = max(0.0, est_ms / 1000.0) + ESTIMATE_GRACE_S
    time.sleep(wait_s)

    deadline = time.time() + STATUS_POLL_TIMEOUT_S
    while True:
        status_text = _send_and_log(ser, "STATUS", timeout)
        rows = parse_status_lines(status_text)
        any_moving = any(r.get("moving") == "1" for r in rows)
        if not any_moving:
            return
        if time.time() >= deadline:
            raise RuntimeError(
                "Timed out waiting for motors to stop (STATUS still reports moving=1)."
            )
        time.sleep(STATUS_POLL_INTERVAL_S)


def run_sequence(ns) -> int:
    if serial is None:
        print(
            "pyserial not installed. Install 'pyserial' to use this script.",
            file=sys.stderr,
        )
        return 2
    with serial.Serial(ns.port, ns.baud, timeout=ns.timeout) as ser:
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        total_passes = max(1, ns.repetitions + 1)
        print(f"Running setup commands ({len(SETUP_COMMANDS)}).")
        for cmd in SETUP_COMMANDS:
            resp = _send_and_log(ser, cmd, ns.timeout)
            est_ms = extract_est_ms_from_ctrl_ok(resp)
            _wait_for_idle(ser, ns.timeout, est_ms)

        print(f"\nStarting patterned sequence for {total_passes} pass(es).")
        pattern = tuple(PATTERN_COMMANDS)
        for rep in range(total_passes):
            print(f"\n=== Pass {rep + 1} / {total_passes} ===")
            for idx, cmd in enumerate(pattern, start=1):
                print(f"\nStep {idx}/{len(pattern)}")
                resp = _send_and_log(ser, cmd, ns.timeout)
                est_ms = extract_est_ms_from_ctrl_ok(resp)
                _wait_for_idle(ser, ns.timeout, est_ms)

        if FINALIZE_COMMANDS:
            print("\nRunning final commands.")
            for cmd in FINALIZE_COMMANDS:
                resp = _send_and_log(ser, cmd, ns.timeout)
                est_ms = extract_est_ms_from_ctrl_ok(resp)
                _wait_for_idle(ser, ns.timeout, est_ms)

        print("\nFinal STATUS:")
        _send_and_log(ser, "STATUS", ns.timeout)
    return 0


def _build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="python tools/bench/manual_sequence_runner.py",
        description="Replay a deterministic MOVE/HOME pattern for manual bench validation.",
    )
    p.add_argument(
        "--port", "-p", required=True, help="Serial port (e.g., /dev/ttyUSB0, COM3)"
    )
    p.add_argument(
        "--baud", "-b", type=int, default=115200, help="Baud rate (default: 115200)"
    )
    p.add_argument(
        "--timeout", "-t", type=float, default=2.0, help="Serial read timeout (seconds)"
    )
    p.add_argument(
        "--repetitions",
        "-r",
        type=int,
        default=0,
        help="Additional repetitions of the pattern (default 0 → run once).",
    )
    return p


def main(argv=None) -> int:
    ns = _build_parser().parse_args(argv)
    if ns.repetitions < 0:
        print("--repetitions must be >= 0", file=sys.stderr)
        return 2
    try:
        return run_sequence(ns)
    except KeyboardInterrupt:
        print("\nInterrupted by user", file=sys.stderr)
        return 130
    except Exception as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
