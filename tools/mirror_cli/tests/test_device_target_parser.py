"""Tests for device_target_parser module."""

from tools.mirror_cli.tui.device_target_parser import DeviceTarget, parse_device_target


class TestParseDeviceTarget:
    """Tests for parse_device_target function."""

    # --- Switch device syntax: /N ---

    def test_switch_single_digit(self):
        result = parse_device_target("/1")
        assert result == DeviceTarget(kind="switch", device_index=1, command=None)

    def test_switch_multi_digit(self):
        result = parse_device_target("/12")
        assert result == DeviceTarget(kind="switch", device_index=12, command=None)

    def test_switch_with_whitespace(self):
        result = parse_device_target("  /3  ")
        assert result == DeviceTarget(kind="switch", device_index=3, command=None)

    # --- Single device command syntax: /N cmd ---

    def test_single_device_command(self):
        result = parse_device_target("/2 MOVE:0,800")
        assert result == DeviceTarget(kind="single", device_index=2, command="MOVE:0,800")

    def test_single_device_command_with_spaces(self):
        result = parse_device_target("/1 GET SPEED")
        assert result == DeviceTarget(kind="single", device_index=1, command="GET SPEED")

    def test_single_device_command_multi_digit_index(self):
        result = parse_device_target("/15 STATUS")
        assert result == DeviceTarget(kind="single", device_index=15, command="STATUS")

    def test_single_device_command_preserves_command_case(self):
        result = parse_device_target("/1 move:0,800")
        assert result == DeviceTarget(kind="single", device_index=1, command="move:0,800")

    # --- Broadcast syntax: /all cmd ---

    def test_all_lowercase(self):
        result = parse_device_target("/all STATUS")
        assert result == DeviceTarget(kind="all", device_index=None, command="STATUS")

    def test_all_uppercase(self):
        result = parse_device_target("/ALL WAKE:ALL")
        assert result == DeviceTarget(kind="all", device_index=None, command="WAKE:ALL")

    def test_all_mixed_case(self):
        result = parse_device_target("/All HOME:0")
        assert result == DeviceTarget(kind="all", device_index=None, command="HOME:0")

    def test_all_with_complex_command(self):
        result = parse_device_target("/all MOVE:ALL,1200")
        assert result == DeviceTarget(kind="all", device_index=None, command="MOVE:ALL,1200")

    # --- Invalid/non-matching inputs ---

    def test_all_without_command_returns_none(self):
        """'/all' alone is invalid - needs a command."""
        result = parse_device_target("/all")
        assert result is None

    def test_regular_command_returns_none(self):
        result = parse_device_target("MOVE:0,800")
        assert result is None

    def test_empty_string_returns_none(self):
        result = parse_device_target("")
        assert result is None

    def test_whitespace_only_returns_none(self):
        result = parse_device_target("   ")
        assert result is None

    def test_slash_only_returns_none(self):
        result = parse_device_target("/")
        assert result is None

    def test_slash_with_letters_returns_none(self):
        result = parse_device_target("/abc")
        assert result is None

    def test_slash_with_mixed_returns_none(self):
        result = parse_device_target("/1a")
        assert result is None

    def test_zero_index_returns_none(self):
        """/0 is invalid - indices are 1-based."""
        result = parse_device_target("/0")
        assert result is None

    def test_negative_not_parsed(self):
        """Negative numbers don't match digit pattern."""
        result = parse_device_target("/-1")
        assert result is None


class TestDeviceTargetDataclass:
    """Tests for DeviceTarget dataclass."""

    def test_equality(self):
        a = DeviceTarget(kind="switch", device_index=1, command=None)
        b = DeviceTarget(kind="switch", device_index=1, command=None)
        assert a == b

    def test_inequality_kind(self):
        a = DeviceTarget(kind="switch", device_index=1, command=None)
        b = DeviceTarget(kind="single", device_index=1, command="CMD")
        assert a != b
