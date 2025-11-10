#!/usr/bin/env python3
import os
import sys

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))
from serial_cli import build_command, make_parser


def run_case(argv, expected):
    ns = make_parser().parse_args(argv)
    got = build_command(ns)
    if got != expected:
        print(f"FAIL: argv={argv} expected={expected!r} got={got!r}")
        return 1
    return 0


def main():
    failures = 0
    failures += run_case(["help"], "HELP\n")
    failures += run_case(["status"], "STATUS\n")
    # Aliases
    failures += run_case(["st"], "STATUS\n")
    failures += run_case(["wake", "ALL"], "WAKE:ALL\n")
    failures += run_case(["sleep", "0"], "SLEEP:0\n")
    failures += run_case(["move", "3", "1200"], "MOVE:3,1200\n")
    # Legacy per-move speed/accel removed; ensure alias still works
    failures += run_case(["m", "2", "-5"], "MOVE:2,-5\n")
    failures += run_case(["home", "0"], "HOME:0\n")
    failures += run_case(
        ["home", "ALL", "--overshoot", "800", "--backoff", "150"], "HOME:ALL,800,150\n"
    )
    failures += run_case(
        ["home", "1", "--overshoot", "900", "--full-range", "2400"], "HOME:1,900,,2400\n"
    )
    failures += run_case(["h", "7", "--overshoot", "50", "--backoff", "25"], "HOME:7,50,25\n")
    failures += run_case(["last-op"], "GET LAST_OP_TIMING\n")
    failures += run_case(["last-op", "1"], "GET LAST_OP_TIMING:1\n")
    if failures:
        print(f"Tests failed: {failures}")
        return 1
    print("All CLI mapping tests passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
