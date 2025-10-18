#!/usr/bin/env python3
import sys, os

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))
from serial_cli import parse_thermal_get_response, extract_est_ms_from_ctrl_ok


def test_parse_thermal_on():
    s = "CTRL:OK THERMAL_LIMITING=ON max_budget_s=90"
    parsed = parse_thermal_get_response(s)
    assert parsed is not None
    enabled, max_budget = parsed
    assert enabled is True
    assert isinstance(max_budget, int) and max_budget == 90


def test_parse_thermal_off_missing_budget():
    s = "CTRL:OK THERMAL_LIMITING=OFF"
    parsed = parse_thermal_get_response(s)
    assert parsed is not None
    enabled, max_budget = parsed
    assert enabled is False
    assert max_budget is None


def test_extract_est_ms_with_warn():
    s = "\n".join([
        "CTRL:WARN THERMAL_NO_BUDGET id=0 budget_s=0 ttfc_s=10",
        "CTRL:OK est_ms=1234",
    ])
    est = extract_est_ms_from_ctrl_ok(s)
    assert est == 1234


def main():
    try:
        test_parse_thermal_on()
        test_parse_thermal_off_missing_budget()
        test_extract_est_ms_with_warn()
    except AssertionError:
        print("Tests failed.")
        return 1
    print("All CLI thermal flag tests passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
