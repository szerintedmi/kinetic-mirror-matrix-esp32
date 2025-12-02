"""Tests for device_view module helper functions."""

from tools.mirror_cli.tui.device_view import (
    _color,
    _format_age,
    _format_thermal,
    _format_uptime,
)


class TestFormatAge:
    """Tests for _format_age function."""

    def test_seconds(self):
        assert _format_age(0) == "0.0s"
        assert _format_age(5) == "5.0s"
        assert _format_age(59.9) == "59.9s"

    def test_seconds_decimal(self):
        assert _format_age(2.5) == "2.5s"
        assert _format_age(0.1) == "0.1s"

    def test_minutes(self):
        assert _format_age(60) == "1.0m"
        assert _format_age(90) == "1.5m"
        assert _format_age(3599) == "60.0m"

    def test_hours(self):
        assert _format_age(3600) == "1.0h"
        assert _format_age(5400) == "1.5h"
        assert _format_age(7200) == "2.0h"

    def test_none_or_inf(self):
        assert _format_age(None) == "?"
        assert _format_age(float("inf")) == "?"


class TestFormatUptime:
    """Tests for _format_uptime function."""

    def test_zero_or_none(self):
        assert _format_uptime(0) == "-"
        assert _format_uptime(None) == "-"

    def test_seconds(self):
        assert _format_uptime(1) == "1s"
        assert _format_uptime(59) == "59s"

    def test_minutes(self):
        assert _format_uptime(60) == "1m0s"
        assert _format_uptime(90) == "1m30s"
        assert _format_uptime(3599) == "59m59s"

    def test_hours(self):
        assert _format_uptime(3600) == "1h0m"
        assert _format_uptime(3660) == "1h1m"
        assert _format_uptime(7200) == "2h0m"
        assert _format_uptime(86399) == "23h59m"

    def test_days(self):
        assert _format_uptime(86400) == "1d0h"
        assert _format_uptime(90000) == "1d1h"
        assert _format_uptime(172800) == "2d0h"


class TestFormatThermal:
    """Tests for _format_thermal function."""

    def test_none_or_invalid(self):
        assert _format_thermal(None) == "-"
        assert _format_thermal("not a tuple") == "-"
        assert _format_thermal([True, 100]) == "-"  # list, not tuple

    def test_disabled(self):
        assert _format_thermal((False, None)) == "OFF"
        assert _format_thermal((False, 100)) == "OFF"

    def test_enabled_no_budget(self):
        assert _format_thermal((True, None)) == "ON"

    def test_enabled_with_budget(self):
        assert _format_thermal((True, 60)) == "ON(60s)"
        assert _format_thermal((True, 120)) == "ON(120s)"


class TestColor:
    """Tests for _color function."""

    def test_value_with_swatch(self):
        assert _color("ready", "green") == "[green]ready[/]"
        assert _color("error", "red") == "[red]error[/]"

    def test_no_value(self):
        assert _color("", "green") == "-"
        assert _color(None, "green") == "-"

    def test_no_swatch(self):
        assert _color("ready", None) == "ready"
        assert _color("ready", "") == "ready"

    def test_both_empty(self):
        assert _color("", None) == "-"
        assert _color(None, None) == "-"
