"""CLI argument parsing and command building for mirror_cli."""

from __future__ import annotations

import argparse
from typing import List, Optional


def _join_home_with_placeholders(
    id_token: str,
    overshoot: Optional[int],
    backoff: Optional[int],
    full_range: Optional[int],
) -> str:
    """Build HOME payload supporting optional fields without legacy speed/accel.

    Grammar: HOME:<id|ALL>[,<overshoot>][,<backoff>][,<full_range>]
    """
    fields: List[Optional[str]] = [None, None, None]
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


def _join_move_with_placeholders(
    id_token: str,
    abs_steps: int,
    overshoot: Optional[int],
    dither_amplitude: Optional[int],
    dither_cycles: Optional[int],
) -> str:
    """Build MOVE payload supporting optional settle fields.

    Grammar: MOVE:<id|ALL>,<abs_steps>[,<speed>][,<accel>][,<overshoot>][,<dither_amp>][,<dither_cycles>]
    Speed/accel slots (indices 2-3) are always empty (use global SET).
    """
    # fields[0]=speed, [1]=accel, [2]=overshoot, [3]=dither_amp, [4]=dither_cycles
    fields: List[Optional[str]] = [None, None, None, None, None]
    if overshoot is not None:
        fields[2] = str(overshoot)
    if dither_amplitude is not None:
        fields[3] = str(dither_amplitude)
    if dither_cycles is not None:
        fields[4] = str(dither_cycles)
    hi = -1
    for i in range(len(fields) - 1, -1, -1):
        if fields[i] is not None:
            hi = i
            break
    parts = [id_token, str(abs_steps)]
    if hi >= 0:
        parts.extend([fields[i] if fields[i] is not None else "" for i in range(0, hi + 1)])
    return f"MOVE:{','.join(parts)}\n"


def build_command(ns: argparse.Namespace) -> str:
    """Build a command string from parsed arguments."""
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
        return _join_move_with_placeholders(
            str(ns.id),
            ns.abs_steps,
            getattr(ns, "overshoot", None),
            getattr(ns, "dither_amplitude", None),
            getattr(ns, "dither_cycles", None),
        )
    if cmd == "home" or cmd == "h":
        # Simplified grammar: no per-home speed/accel
        return _join_home_with_placeholders(str(ns.id), ns.overshoot, ns.backoff, ns.full_range)
    raise SystemExit(f"Unknown command: {cmd}")


def _add_common(sub: argparse.ArgumentParser) -> argparse.ArgumentParser:
    """Add common arguments to a subparser."""
    sub.add_argument("--port", "-p", help="Serial port (e.g., /dev/ttyUSB0, COM3)")
    sub.add_argument("--baud", "-b", type=int, default=115200, help="Baud rate (default 115200)")
    sub.add_argument(
        "--timeout",
        "-t",
        type=float,
        default=2.0,
        help="Read timeout seconds (default 2.0)",
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
    """Create the main argument parser for mirror_cli."""
    p = argparse.ArgumentParser(prog="mirror_cli", description="CLI for Mirror Array protocol v1")
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
    s.add_argument("--overshoot", type=int, help="Override settle overshoot steps (0=disable)")
    s.add_argument(
        "--dither-amplitude", type=int, dest="dither_amplitude", help="Override dither amplitude"
    )
    s.add_argument(
        "--dither-cycles", type=int, dest="dither_cycles", help="Override dither cycle count"
    )
    s_alias = _add_common(sp.add_parser("m", help="Alias for move"))
    s_alias.add_argument("id", help="Motor id 0-7 or ALL")
    s_alias.add_argument("abs_steps", type=int, help="Absolute target steps (-1200..1200)")
    s_alias.add_argument(
        "--overshoot", type=int, help="Override settle overshoot steps (0=disable)"
    )
    s_alias.add_argument(
        "--dither-amplitude", type=int, dest="dither_amplitude", help="Override dither amplitude"
    )
    s_alias.add_argument(
        "--dither-cycles", type=int, dest="dither_cycles", help="Override dither cycle count"
    )

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
        "--full-range",
        type=int,
        default=2400,
        dest="full_range",
        help="Full range steps",
    )
    s.add_argument("--tolerance", type=float, default=0.25, help="Allowed relative deviation")

    # interactive TUI
    s = _add_common(
        sp.add_parser("interactive", help="Interactive TUI (poll STATUS and accept commands)")
    )
    s.add_argument("--rate", "-r", type=float, default=2.0, help="Refresh rate in Hz (default 2.0)")

    return p


__all__ = [
    "build_command",
    "make_parser",
]
