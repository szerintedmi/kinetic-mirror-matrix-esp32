#!/usr/bin/env python3
import os
import sys

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))
from serial_cli import make_parser, parse_status_lines, render_table


def test_parse_interactive_args_default():
    ns = make_parser().parse_args(["interactive", "--port", "COM3"])
    assert ns.command == "interactive"
    assert ns.port == "COM3"
    assert abs(ns.rate - 2.0) < 1e-6


def test_parse_interactive_args_custom():
    ns = make_parser().parse_args(
        [
            "interactive",
            "--port",
            "/dev/ttyUSB0",
            "--baud",
            "9600",
            "--timeout",
            "1.5",
            "--rate",
            "4.0",
        ]
    )
    assert ns.port == "/dev/ttyUSB0"
    assert ns.baud == 9600
    assert abs(ns.timeout - 1.5) < 1e-6
    assert abs(ns.rate - 4.0) < 1e-6


def test_parse_status_kv_and_render():
    text = """
id=0 pos=0 moving=0 awake=1 homed=1 steps_since_home=0 budget_s=90.0 ttfc_s=0.0 speed=4000 accel=16000
id=1 pos=10 moving=1 awake=1 homed=1 steps_since_home=10 budget_s=89.5 ttfc_s=0.5 speed=4000 accel=16000
""".strip()
    rows = parse_status_lines(text)
    assert len(rows) == 2
    assert rows[0]["id"] == "0"
    assert rows[1]["pos"] == "10"
    table = render_table(rows)
    assert "id" in table and "pos" in table
    assert " 0" in table and " 10" in table


def test_parse_status_csv_and_render():
    text = """
id,pos,moving,awake,homed,steps_since_home,budget_s,ttfc_s
0,0,0,1,1,0,90.0,0.0
1,10,1,1,1,10,89.5,0.5
""".strip()
    rows = parse_status_lines(text)
    assert len(rows) == 2
    assert rows[0]["id"] == "0" and rows[1]["moving"] == "1"
    table = render_table(rows)
    assert ("ttfc_s" in table) or ("ttfc" in table)


def main():
    try:
        test_parse_interactive_args_default()
        test_parse_interactive_args_custom()
        test_parse_status_kv_and_render()
        test_parse_status_csv_and_render()
    except AssertionError:
        print("Tests failed.")
        return 1
    print("All interactive CLI tests passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
