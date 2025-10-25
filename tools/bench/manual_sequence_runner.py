"""
Manual bench routine runner for shared-step motion testing.

This script replays a deterministic series of MOVE/HOME commands so operators can
compare MCU-reported positions with physical marks without editing firmware.
"""

from __future__ import annotations

import argparse
import sys
import time
from typing import Optional, Sequence, Tuple, Dict

try:
    import serial  # type: ignore
except Exception:  # pragma: no cover - pyserial missing on host
    serial = None

# Ensure low-latency printing regardless of host buffering (IDE pipes, etc.)
try:  # Python 3.7+
    sys.stdout.reconfigure(line_buffering=True, write_through=True)  # type: ignore[attr-defined]
    sys.stderr.reconfigure(line_buffering=True, write_through=True)  # type: ignore[attr-defined]
except Exception:
    pass

from tools.serial_cli import (  # type: ignore[import-not-found]
    parse_status_lines,
)

# One-time prep commands run before the repeated pattern.
SETUP_COMMANDS: Sequence[str] = ["SET SPEED=4000;SET ACCEL=16000;SET DECEL=0;HOME:ALL"]

# Hard-coded sequence that emphasizes rhythm changes without per-run tweaking.
PATTERN_COMMANDS: Sequence[str] = [
    # Small Steps forward then back to 0 to surface if small steps are accurate. running 10 repetitions of these moves
    # sharedstep: total chaos drift, all motors have different drift. Fas: no noticable drift
    # "MOVE:ALL,-300",
    # "MOVE:ALL,0",
    # "MOVE:ALL,300",
    # "MOVE:ALL,600",
    # "MOVE:ALL,900",
    # "MOVE:ALL,0",
    # sharedStep: huge drifting, all motors have different drift. Fas: no noticable drift
    "MOVE:ALL,100",
    "MOVE:ALL,200",
    "MOVE:ALL,300",
    "MOVE:ALL,400",
    "MOVE:ALL,500",
    "MOVE:ALL,0",
    # sharedStep: huge  drift. Fas: no noticable drift
    # "MOVE:ALL,50",
    # "MOVE:ALL,100",
    # "MOVE:ALL,150",
    # "MOVE:ALL,200",
    # "MOVE:ALL,250",
    # "MOVE:ALL,0",
    # "MOVE:4,50",
    # "MOVE:4,100",
    # "MOVE:4,150",
    # "MOVE:4,200",
    # "MOVE:4,250",
    # "MOVE:4,0",
    # sharedStep: hardly moving with 10 steps, Fas: small drift
    # "MOVE:ALL,30",
    # "MOVE:ALL,40",
    # "MOVE:ALL,50",
    # "MOVE:ALL,60",
    # "MOVE:ALL,70",
    # "MOVE:ALL,80",
    # "MOVE:ALL,90",
    # "MOVE:ALL,100",
    # "MOVE:ALL,110",
    # "MOVE:ALL,120",
    # "MOVE:ALL,130",
    # "MOVE:ALL,140",
    # "MOVE:ALL,150",
    # "MOVE:ALL,160",
    # "MOVE:ALL,170",
    # "MOVE:ALL,180",
    # "MOVE:ALL,190",
    # "MOVE:ALL,200",
    # "MOVE:ALL,0",
]

FINALIZE_COMMANDS: Sequence[str] = [
    "MOVE:ALL,1200"  # move  to limit - if small steps skipped then motor wont be at limit
]

# Small initial grace after sending a command before we start polling
WAIT_INITIAL_GRACE_S = 0.01
# Shorter polling interval to reduce perceived lag between steps
STATUS_POLL_INTERVAL_S = 0.05
# Faster completion detection for short STATUS responses
STATUS_IDLE_GRACE_S = 0.02
STATUS_POLL_TIMEOUT_S = 30.0
# Use a tighter idle grace for command acks (CTRL:OK/ERR)
CMD_IDLE_GRACE_S = 0.01

# Tracing removed to keep runner minimal


def _read_response_fast(
    ser,
    timeout: float,
    idle_grace: float,
    *,
    early_ctrl: bool = False,
    trace: bool = False,
) -> Tuple[str, Dict[str, float]]:
    """Non-blocking style reader using in_waiting to reduce latency.

    - idle_grace: quiet period to consider response complete
    - early_ctrl: return as soon as a full line starting with 'CTRL:' appears
    - trace: capture timing markers
    Returns (text, meta) where meta includes timing keys when trace is True.
    """
    try:
        ser.timeout = 0  # non-blocking reads
    except Exception:
        pass
    buf: list[str] = []
    t0 = time.time()
    first_rx = 0.0
    last_rx = 0.0
    ctrl_seen_at = 0.0
    deadline = t0 + max(0.05, timeout)
    idle_grace = max(0.001, idle_grace)
    while True:
        now = time.time()
        if now >= deadline:
            break
        available = 0
        try:
            available = int(getattr(ser, "in_waiting", 0))
        except Exception:
            available = 0
        if available > 0:
            data = ser.read(available)
            if data:
                if not first_rx:
                    first_rx = time.time() - t0
                last_rx = time.time() - t0
                s = data.decode(errors="replace")
                buf.append(s)
                if early_ctrl:
                    combined = "".join(buf)
                    # Return as soon as we detect a CTRL: line token, even if no newline yet
                    if "CTRL:" in combined:
                        ctrl_seen_at = time.time() - t0
                        meta = {
                            "dt_first_rx": first_rx or 0.0,
                            "dt_ctrl": ctrl_seen_at,
                            "dt_total": time.time() - t0,
                        }
                        return combined.strip(), meta
            continue
        # No data present; if we've received something and been idle long enough, stop
        if last_rx and (now - (t0 + last_rx)) >= idle_grace:
            break
        time.sleep(0.002)
    text = "".join(buf).strip()
    meta = (
        {
            "dt_first_rx": first_rx or 0.0,
            "dt_ctrl": ctrl_seen_at or 0.0,
            "dt_total": time.time() - t0,
        }
        if trace
        else {}
    )
    return text, meta


def _send_and_log(ser, cmd: str, timeout: float) -> str:
    """Send a single command (or multicommand) and print the device response.

    For STATUS polls, callers may pass a shorter idle_grace via idle_grace_override
    to avoid lingering waits after the last byte arrives.
    """
    payload = cmd if cmd.endswith("\n") else cmd + "\n"
    print(f"\n>>> {cmd}", flush=True)
    ser.write(payload.encode())
    ser.flush()
    # Choose reader behavior based on command type
    ucmd = cmd.strip().upper()
    is_status = ucmd.startswith("STATUS") or ucmd.startswith("ST")
    idle = STATUS_IDLE_GRACE_S if is_status else CMD_IDLE_GRACE_S
    early_ctrl = not is_status  # return as soon as CTRL appears for non-STATUS
    resp, _meta = _read_response_fast(
        ser, timeout, idle, early_ctrl=early_ctrl, trace=False
    )
    if resp:
        print(resp, flush=True)
    else:
        print("(no response)", flush=True)
    return resp


def _wait_for_idle(ser, timeout: float) -> None:
    """Wait for motion to finish by polling STATUS with low latency.

    Previously, this routine slept for est_ms before polling, which caused
    noticeable lag when the device completed the move quickly and only then
    printed CTRL:OK. We now skip the long sleep and immediately poll STATUS
    with a short interval and a fast idle grace.
    """
    # Tiny initial grace to avoid racing immediately after write/OK burst.
    time.sleep(WAIT_INITIAL_GRACE_S)

    deadline = time.time() + STATUS_POLL_TIMEOUT_S
    while True:
        # Poll STATUS quietly with a tighter idle_grace so we return as soon
        # as the line goes quiet. Avoid spamming the console while waiting.
        ser.write(b"STATUS\n")
        status_text, _meta = _read_response_fast(
            ser, min(timeout, 0.25), STATUS_IDLE_GRACE_S
        )
        rows = parse_status_lines(status_text)
        any_moving = any(r.get("moving") == "1" for r in rows)
        if not any_moving:
            # Print the final STATUS we just observed for operator visibility
            print("\n>>> STATUS", flush=True)
            print(status_text if status_text else "(no response)", flush=True)
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
        # Best-effort low-latency tuning; ignore if not supported by platform.
        try:
            ser.inter_byte_timeout = 0.02
            ser.write_timeout = 0
        except Exception:
            pass
        total_passes = max(1, ns.repetitions + 1)
        print(f"Running setup commands ({len(SETUP_COMMANDS)}).", flush=True)
        for cmd in SETUP_COMMANDS:
            _send_and_log(ser, cmd, ns.timeout)
            _wait_for_idle(ser, ns.timeout)

        print(f"\nStarting patterned sequence for {total_passes} pass(es).", flush=True)
        pattern = tuple(PATTERN_COMMANDS)
        for rep in range(total_passes):
            print(f"\n=== Pass {rep + 1} / {total_passes} ===", flush=True)
            for idx, cmd in enumerate(pattern, start=1):
                print(f"\nStep {idx}/{len(pattern)}", flush=True)
                _send_and_log(ser, cmd, ns.timeout)
                _wait_for_idle(ser, ns.timeout)

        if FINALIZE_COMMANDS:
            print("\nRunning final commands.", flush=True)
            for cmd in FINALIZE_COMMANDS:
                _send_and_log(ser, cmd, ns.timeout)
                _wait_for_idle(ser, ns.timeout)

        print("\nFinal STATUS:", flush=True)
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
