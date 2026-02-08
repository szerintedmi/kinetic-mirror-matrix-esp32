"""Comprehensive tests for command_builder.py - command parsing and building."""

import unittest

from tools.mirror_cli.command_builder import (
    CommandParseError,
    CommandRequest,
    UnsupportedCommandError,
    build_requests,
    parse_serial_command,
    split_batches,
)


# =============================================================================
# HELP Command Tests
# =============================================================================
class TestHelpCommand(unittest.TestCase):
    """Tests for HELP command parsing."""

    def test_help_basic(self):
        req = parse_serial_command("HELP")
        self.assertEqual(req.action, "HELP")
        self.assertEqual(req.params, {})

    def test_help_lowercase(self):
        req = parse_serial_command("help")
        self.assertEqual(req.action, "HELP")

    def test_help_with_args_raises(self):
        with self.assertRaises(CommandParseError) as ctx:
            parse_serial_command("HELP me")
        self.assertIn("does not take arguments", str(ctx.exception))


# =============================================================================
# STATUS Command Tests
# =============================================================================
class TestStatusCommand(unittest.TestCase):
    """Tests for STATUS command (unsupported over MQTT)."""

    def test_status_raises_unsupported(self):
        with self.assertRaises(UnsupportedCommandError) as ctx:
            parse_serial_command("STATUS")
        self.assertIn("not supported", str(ctx.exception))

    def test_st_alias_raises_unsupported(self):
        with self.assertRaises(UnsupportedCommandError):
            parse_serial_command("ST")


# =============================================================================
# MOVE Command Tests
# =============================================================================
class TestMoveCommand(unittest.TestCase):
    """Tests for MOVE command parsing."""

    def test_move_basic(self):
        req = parse_serial_command("MOVE:1,900")
        self.assertEqual(req.action, "MOVE")
        self.assertEqual(req.params["target_ids"], 1)
        self.assertEqual(req.params["position_steps"], 900)

    def test_move_with_speed(self):
        req = parse_serial_command("MOVE:0,500,4000")
        self.assertEqual(req.params["target_ids"], 0)
        self.assertEqual(req.params["position_steps"], 500)
        self.assertEqual(req.params["speed"], 4000)

    def test_move_with_speed_and_accel(self):
        req = parse_serial_command("MOVE:2,1000,5000,10000")
        self.assertEqual(req.params["speed"], 5000)
        self.assertEqual(req.params["accel"], 10000)

    def test_move_all_target(self):
        req = parse_serial_command("MOVE:ALL,800")
        self.assertEqual(req.params["target_ids"], "ALL")
        self.assertEqual(req.params["position_steps"], 800)

    def test_move_all_lowercase(self):
        req = parse_serial_command("MOVE:all,500")
        self.assertEqual(req.params["target_ids"], "ALL")

    def test_move_shorthand_m(self):
        req = parse_serial_command("M:3,600")
        self.assertEqual(req.action, "MOVE")
        self.assertEqual(req.params["target_ids"], 3)

    def test_move_negative_position(self):
        req = parse_serial_command("MOVE:0,-500")
        self.assertEqual(req.params["position_steps"], -500)

    def test_move_negative_target_id(self):
        # Negative IDs are parsed as integers (firmware may reject)
        req = parse_serial_command("MOVE:-1,100")
        self.assertEqual(req.params["target_ids"], -1)

    def test_move_skip_speed_set_accel(self):
        # Empty speed, explicit accel
        req = parse_serial_command("MOVE:0,100,,5000")
        self.assertNotIn("speed", req.params)
        self.assertEqual(req.params["accel"], 5000)

    def test_move_with_overshoot(self):
        req = parse_serial_command("MOVE:0,500,,,200")
        self.assertEqual(req.params["overshoot_steps"], 200)
        self.assertNotIn("speed", req.params)
        self.assertNotIn("accel", req.params)

    def test_move_with_all_settle_params(self):
        req = parse_serial_command("MOVE:0,500,,,300,50,4")
        self.assertEqual(req.params["overshoot_steps"], 300)
        self.assertEqual(req.params["dither_amplitude"], 50)
        self.assertEqual(req.params["dither_cycles"], 4)

    def test_move_with_speed_accel_and_settle(self):
        req = parse_serial_command("MOVE:0,500,4000,16000,200,50,3")
        self.assertEqual(req.params["speed"], 4000)
        self.assertEqual(req.params["accel"], 16000)
        self.assertEqual(req.params["overshoot_steps"], 200)
        self.assertEqual(req.params["dither_amplitude"], 50)
        self.assertEqual(req.params["dither_cycles"], 3)

    def test_move_skip_overshoot_set_dither(self):
        req = parse_serial_command("MOVE:0,500,,,,50,3")
        self.assertNotIn("overshoot_steps", req.params)
        self.assertEqual(req.params["dither_amplitude"], 50)
        self.assertEqual(req.params["dither_cycles"], 3)

    def test_move_overshoot_zero(self):
        req = parse_serial_command("MOVE:0,500,,,0")
        self.assertEqual(req.params["overshoot_steps"], 0)

    def test_move_missing_position_raises(self):
        with self.assertRaises(CommandParseError) as ctx:
            parse_serial_command("MOVE:0")
        self.assertIn("requires target and position", str(ctx.exception))

    def test_move_invalid_target_raises(self):
        with self.assertRaises(CommandParseError) as ctx:
            parse_serial_command("MOVE:abc,100")
        self.assertIn("target selector invalid", str(ctx.exception))

    def test_move_invalid_position_raises(self):
        with self.assertRaises(CommandParseError):
            parse_serial_command("MOVE:0,xyz")

    def test_move_space_syntax_raises(self):
        with self.assertRaises(CommandParseError) as ctx:
            parse_serial_command("MOVE 0 100")
        self.assertIn("colon syntax", str(ctx.exception))

    def test_move_preserves_raw(self):
        raw = "  MOVE:0,100  "
        req = parse_serial_command(raw)
        self.assertEqual(req.raw, raw.strip())


# =============================================================================
# HOME Command Tests
# =============================================================================
class TestHomeCommand(unittest.TestCase):
    """Tests for HOME command parsing."""

    def test_home_basic(self):
        req = parse_serial_command("HOME:0")
        self.assertEqual(req.action, "HOME")
        self.assertEqual(req.params["target_ids"], 0)

    def test_home_with_overshoot(self):
        req = parse_serial_command("HOME:1,800")
        self.assertEqual(req.params["overshoot_steps"], 800)

    def test_home_with_overshoot_and_backoff(self):
        req = parse_serial_command("HOME:1,800,150")
        self.assertEqual(req.params["overshoot_steps"], 800)
        self.assertEqual(req.params["backoff_steps"], 150)

    def test_home_all_optionals(self):
        req = parse_serial_command("HOME:ALL,800,150,3000,12000,2400")
        self.assertEqual(req.params["target_ids"], "ALL")
        self.assertEqual(req.params["overshoot_steps"], 800)
        self.assertEqual(req.params["backoff_steps"], 150)
        self.assertEqual(req.params["speed"], 3000)
        self.assertEqual(req.params["accel"], 12000)
        self.assertEqual(req.params["full_range_steps"], 2400)

    def test_home_skip_optional_fields(self):
        # Skip overshoot but set backoff
        req = parse_serial_command("HOME:0,,200")
        self.assertNotIn("overshoot_steps", req.params)
        self.assertEqual(req.params["backoff_steps"], 200)

    def test_home_shorthand_h(self):
        req = parse_serial_command("H:2")
        self.assertEqual(req.action, "HOME")
        self.assertEqual(req.params["target_ids"], 2)

    def test_home_missing_target_raises(self):
        with self.assertRaises(CommandParseError):
            parse_serial_command("HOME:")

    def test_home_space_syntax_raises(self):
        with self.assertRaises(CommandParseError) as ctx:
            parse_serial_command("HOME 0")
        self.assertIn("colon syntax", str(ctx.exception))


# =============================================================================
# WAKE/SLEEP Command Tests
# =============================================================================
class TestWakeSleepCommands(unittest.TestCase):
    """Tests for WAKE and SLEEP command parsing."""

    def test_wake_colon_syntax(self):
        req = parse_serial_command("WAKE:0")
        self.assertEqual(req.action, "WAKE")
        self.assertEqual(req.params["target_ids"], 0)

    def test_wake_space_syntax(self):
        req = parse_serial_command("WAKE 1")
        self.assertEqual(req.action, "WAKE")
        self.assertEqual(req.params["target_ids"], 1)

    def test_wake_all_target(self):
        req = parse_serial_command("WAKE:ALL")
        self.assertEqual(req.params["target_ids"], "ALL")

    def test_sleep_colon_syntax(self):
        req = parse_serial_command("SLEEP:2")
        self.assertEqual(req.action, "SLEEP")
        self.assertEqual(req.params["target_ids"], 2)

    def test_sleep_space_syntax(self):
        req = parse_serial_command("SLEEP 3")
        self.assertEqual(req.action, "SLEEP")
        self.assertEqual(req.params["target_ids"], 3)

    def test_wake_missing_target_colon_raises(self):
        with self.assertRaises(CommandParseError):
            parse_serial_command("WAKE:")

    def test_sleep_missing_target_space_raises(self):
        with self.assertRaises(CommandParseError):
            parse_serial_command("SLEEP")


# =============================================================================
# GET Command Tests
# =============================================================================
class TestGetCommand(unittest.TestCase):
    """Tests for GET command parsing."""

    def test_get_no_args(self):
        req = parse_serial_command("GET")
        self.assertEqual(req.action, "GET")
        self.assertEqual(req.params["resource"], "ALL")

    def test_get_specific_resource(self):
        req = parse_serial_command("GET SPEED")
        self.assertEqual(req.params["resource"], "SPEED")

    def test_get_last_op_timing_with_colon(self):
        req = parse_serial_command("GET LAST_OP_TIMING:0")
        self.assertEqual(req.params["resource"], "LAST_OP_TIMING")
        self.assertEqual(req.params["target_ids"], 0)

    def test_get_last_op_timing_with_space(self):
        req = parse_serial_command("GET LAST_OP_TIMING 1")
        self.assertEqual(req.params["resource"], "LAST_OP_TIMING")
        self.assertEqual(req.params["target_ids"], 1)

    def test_get_last_op_timing_all(self):
        req = parse_serial_command("GET LAST_OP_TIMING:ALL")
        self.assertEqual(req.params["target_ids"], "ALL")

    def test_get_lowercase(self):
        req = parse_serial_command("get speed")
        self.assertEqual(req.params["resource"], "SPEED")


# =============================================================================
# SET Command Tests
# =============================================================================
class TestSetCommand(unittest.TestCase):
    """Tests for SET command parsing - passthrough to firmware."""

    def test_set_speed(self):
        req = parse_serial_command("SET SPEED=4000")
        self.assertEqual(req.action, "SET")
        self.assertEqual(req.params["SPEED"], 4000)

    def test_set_accel(self):
        req = parse_serial_command("SET ACCEL=10000")
        self.assertEqual(req.params["ACCEL"], 10000)

    def test_set_decel(self):
        req = parse_serial_command("SET DECEL=8000")
        self.assertEqual(req.params["DECEL"], 8000)

    def test_set_decel_zero(self):
        req = parse_serial_command("SET DECEL=0")
        self.assertEqual(req.params["DECEL"], 0)

    def test_set_decel_negative_passthrough(self):
        # Negative values passthrough to firmware for validation
        req = parse_serial_command("SET DECEL=-100")
        self.assertEqual(req.params["DECEL"], -100)

    def test_set_thermal_limiting_on(self):
        req = parse_serial_command("SET THERMAL_LIMITING=ON")
        self.assertEqual(req.params["THERMAL_LIMITING"], "ON")

    def test_set_thermal_limiting_off(self):
        req = parse_serial_command("SET THERMAL_LIMITING=OFF")
        self.assertEqual(req.params["THERMAL_LIMITING"], "OFF")

    def test_set_key_case_normalized(self):
        # Key is uppercased, value is passed as-is
        req = parse_serial_command("SET thermal_limiting=on")
        self.assertEqual(req.params["THERMAL_LIMITING"], "on")

    def test_set_any_value_passthrough(self):
        # Invalid values passthrough to firmware for validation
        req = parse_serial_command("SET THERMAL_LIMITING=MAYBE")
        self.assertEqual(req.params["THERMAL_LIMITING"], "MAYBE")

    def test_set_unknown_field_passthrough(self):
        # Unknown fields passthrough to firmware for validation
        req = parse_serial_command("SET FOOBAR=123")
        self.assertEqual(req.params["FOOBAR"], "123")

    def test_set_microstep(self):
        # MICROSTEP passthrough to firmware
        req = parse_serial_command("SET MICROSTEP=1/32")
        self.assertEqual(req.params["MICROSTEP"], "1/32")

    def test_set_move_overshoot(self):
        req = parse_serial_command("SET MOVE_OVERSHOOT=300")
        self.assertEqual(req.params["MOVE_OVERSHOOT"], 300)
        self.assertIsInstance(req.params["MOVE_OVERSHOOT"], int)

    def test_set_dither_amplitude(self):
        req = parse_serial_command("SET DITHER_AMPLITUDE=50")
        self.assertEqual(req.params["DITHER_AMPLITUDE"], 50)

    def test_set_dither_cycles(self):
        req = parse_serial_command("SET DITHER_CYCLES=5")
        self.assertEqual(req.params["DITHER_CYCLES"], 5)

    def test_set_dither_min_amplitude(self):
        req = parse_serial_command("SET DITHER_MIN_AMPLITUDE=10")
        self.assertEqual(req.params["DITHER_MIN_AMPLITUDE"], 10)

    def test_set_settle_non_integer_raises(self):
        with self.assertRaises(CommandParseError):
            parse_serial_command("SET MOVE_OVERSHOOT=abc")

    def test_thermal_budget(self):
        # THERMAL_BUDGET:<id> is a debug command that takes integer
        req = parse_serial_command("SET THERMAL_BUDGET:0=-60")
        self.assertEqual(req.params["THERMAL_BUDGET:0"], -60)
        self.assertIsInstance(req.params["THERMAL_BUDGET:0"], int)

    def test_set_thermal_budget_positive(self):
        req = parse_serial_command("SET THERMAL_BUDGET:3=300")
        self.assertEqual(req.params["THERMAL_BUDGET:3"], 300)

    def test_set_missing_equals_raises(self):
        # "SET SPEED 4000" is parsed as two tokens, failing "single assignment" check
        with self.assertRaises(CommandParseError):
            parse_serial_command("SET SPEED 4000")

    def test_set_missing_equals_single_token_raises(self):
        # Single token without equals
        with self.assertRaises(CommandParseError) as ctx:
            parse_serial_command("SET SPEED")
        self.assertIn("missing '='", str(ctx.exception))

    def test_set_speed_non_integer_raises(self):
        # Non-integer values for SPEED/ACCEL/DECEL raise error
        with self.assertRaises(CommandParseError) as ctx:
            parse_serial_command("SET SPEED=fast")
        self.assertIn("must be integer", str(ctx.exception))

    def test_set_colon_syntax(self):
        # SET:KEY=VALUE works like SET KEY=VALUE
        req = parse_serial_command("SET:MICROSTEP=1/32")
        self.assertEqual(req.action, "SET")
        self.assertEqual(req.params["MICROSTEP"], "1/32")

    def test_set_colon_syntax_lowercase(self):
        req = parse_serial_command("set:speed=4000")
        self.assertEqual(req.action, "SET")
        self.assertEqual(req.params["SPEED"], 4000)


# =============================================================================
# NET Command Tests
# =============================================================================
class TestNetCommands(unittest.TestCase):
    """Tests for NET:* command parsing."""

    def test_net_status(self):
        req = parse_serial_command("NET:STATUS")
        self.assertEqual(req.action, "NET:STATUS")
        self.assertEqual(req.params, {})

    def test_net_reset(self):
        req = parse_serial_command("NET:RESET")
        self.assertEqual(req.action, "NET:RESET")

    def test_net_list(self):
        req = parse_serial_command("NET:LIST")
        self.assertEqual(req.action, "NET:LIST")

    def test_net_status_with_args_raises(self):
        with self.assertRaises(CommandParseError) as ctx:
            parse_serial_command("NET:STATUS,extra")
        self.assertIn("does not accept arguments", str(ctx.exception))

    def test_net_set_basic(self):
        req = parse_serial_command('NET:SET,"MySSID","MyPass"')
        self.assertEqual(req.action, "NET:SET")
        self.assertEqual(req.params["ssid"], "MySSID")
        self.assertEqual(req.params["pass"], "MyPass")

    def test_net_set_without_quotes(self):
        req = parse_serial_command("NET:SET,OpenNetwork,secret123")
        self.assertEqual(req.params["ssid"], "OpenNetwork")
        self.assertEqual(req.params["pass"], "secret123")

    def test_net_set_missing_pass_raises(self):
        with self.assertRaises(CommandParseError) as ctx:
            parse_serial_command("NET:SET,OnlySSID")
        self.assertIn("requires ssid and pass", str(ctx.exception))

    def test_net_unsupported_action_raises(self):
        with self.assertRaises(UnsupportedCommandError):
            parse_serial_command("NET:CONNECT")

    def test_net_lowercase(self):
        req = parse_serial_command("net:status")
        self.assertEqual(req.action, "NET:STATUS")


# =============================================================================
# MQTT Command Tests
# =============================================================================
class TestMqttCommands(unittest.TestCase):
    """Tests for MQTT:* command parsing."""

    def test_mqtt_get_config(self):
        req = parse_serial_command("MQTT:GET_CONFIG")
        self.assertEqual(req.action, "MQTT:GET_CONFIG")
        self.assertEqual(req.params, {})

    def test_mqtt_get_config_with_args_raises(self):
        with self.assertRaises(CommandParseError):
            parse_serial_command("MQTT:GET_CONFIG extra")

    def test_mqtt_set_config_host(self):
        req = parse_serial_command("MQTT:SET_CONFIG host=broker.example.com")
        self.assertEqual(req.action, "MQTT:SET_CONFIG")
        self.assertEqual(req.params["host"], "broker.example.com")

    def test_mqtt_set_config_port(self):
        req = parse_serial_command("MQTT:SET_CONFIG port=1883")
        self.assertEqual(req.params["port"], 1883)

    def test_mqtt_set_config_user_pass(self):
        req = parse_serial_command("MQTT:SET_CONFIG user=admin pass=secret")
        self.assertEqual(req.params["user"], "admin")
        self.assertEqual(req.params["pass"], "secret")

    def test_mqtt_set_config_password_alias(self):
        req = parse_serial_command("MQTT:SET_CONFIG password=secret123")
        self.assertEqual(req.params["pass"], "secret123")

    def test_mqtt_set_config_multiple(self):
        req = parse_serial_command("MQTT:SET_CONFIG host=mqtt.local port=8883 user=test")
        self.assertEqual(req.params["host"], "mqtt.local")
        self.assertEqual(req.params["port"], 8883)
        self.assertEqual(req.params["user"], "test")

    def test_mqtt_set_config_reset(self):
        req = parse_serial_command("MQTT:SET_CONFIG RESET")
        self.assertEqual(req.params.get("reset"), True)

    def test_mqtt_set_config_reset_lowercase(self):
        req = parse_serial_command("MQTT:SET_CONFIG reset")
        self.assertEqual(req.params.get("reset"), True)

    def test_mqtt_set_config_invalid_port_raises(self):
        with self.assertRaises(CommandParseError):
            parse_serial_command("MQTT:SET_CONFIG port=abc")

    def test_mqtt_set_config_port_out_of_range_raises(self):
        with self.assertRaises(CommandParseError) as ctx:
            parse_serial_command("MQTT:SET_CONFIG port=99999")
        self.assertIn("out of range", str(ctx.exception))

    def test_mqtt_set_config_port_zero_raises(self):
        with self.assertRaises(CommandParseError):
            parse_serial_command("MQTT:SET_CONFIG port=0")

    def test_mqtt_set_config_unknown_field_raises(self):
        with self.assertRaises(UnsupportedCommandError):
            parse_serial_command("MQTT:SET_CONFIG timeout=30")

    def test_mqtt_set_config_missing_equals_raises(self):
        with self.assertRaises(CommandParseError):
            parse_serial_command("MQTT:SET_CONFIG host broker.local")

    def test_mqtt_set_config_no_args_raises(self):
        with self.assertRaises(CommandParseError):
            parse_serial_command("MQTT:SET_CONFIG")

    def test_mqtt_unsupported_action_raises(self):
        with self.assertRaises(UnsupportedCommandError):
            parse_serial_command("MQTT:CONNECT")

    def test_mqtt_empty_action_raises(self):
        with self.assertRaises(CommandParseError):
            parse_serial_command("MQTT:")


# =============================================================================
# split_batches Tests
# =============================================================================
class TestSplitBatches(unittest.TestCase):
    """Tests for split_batches function."""

    def test_single_command(self):
        parts = split_batches("MOVE:0,100")
        self.assertEqual(parts, ["MOVE:0,100"])

    def test_multiple_commands(self):
        parts = split_batches("MOVE:0,100;MOVE:1,200;MOVE:2,300")
        self.assertEqual(parts, ["MOVE:0,100", "MOVE:1,200", "MOVE:2,300"])

    def test_quoted_semicolon_preserved(self):
        parts = split_batches('NET:SET,"test;net","pass;word"')
        self.assertEqual(len(parts), 1)
        self.assertEqual(parts[0], 'NET:SET,"test;net","pass;word"')

    def test_mixed_quoted_and_unquoted(self):
        parts = split_batches('NET:SET,"ssid","pass";MOVE:0,100')
        self.assertEqual(len(parts), 2)
        self.assertIn("NET:SET", parts[0])
        self.assertIn("MOVE:0,100", parts[1])

    def test_strips_whitespace(self):
        parts = split_batches("  MOVE:0,100  ;  MOVE:1,200  ")
        self.assertEqual(parts, ["MOVE:0,100", "MOVE:1,200"])

    def test_empty_parts_filtered(self):
        parts = split_batches("MOVE:0,100;;MOVE:1,200")
        self.assertEqual(parts, ["MOVE:0,100", "MOVE:1,200"])

    def test_empty_string(self):
        parts = split_batches("")
        self.assertEqual(parts, [])

    def test_whitespace_only(self):
        parts = split_batches("   ")
        self.assertEqual(parts, [])


# =============================================================================
# build_requests Tests
# =============================================================================
class TestBuildRequests(unittest.TestCase):
    """Tests for build_requests function."""

    def test_single_request(self):
        reqs = build_requests("MOVE:0,100")
        self.assertEqual(len(reqs), 1)
        self.assertEqual(reqs[0].action, "MOVE")

    def test_multiple_requests(self):
        reqs = build_requests("MOVE:0,100;MOVE:1,200")
        self.assertEqual(len(reqs), 2)

    def test_mixed_command_types(self):
        reqs = build_requests("WAKE:0;MOVE:0,500;SET SPEED=4000")
        self.assertEqual(len(reqs), 3)
        self.assertEqual(reqs[0].action, "WAKE")
        self.assertEqual(reqs[1].action, "MOVE")
        self.assertEqual(reqs[2].action, "SET")


# =============================================================================
# Edge Cases and Error Handling
# =============================================================================
class TestEdgeCases(unittest.TestCase):
    """Tests for edge cases and error handling."""

    def test_empty_command_raises(self):
        with self.assertRaises(CommandParseError) as ctx:
            parse_serial_command("")
        self.assertIn("empty command", str(ctx.exception))

    def test_whitespace_only_raises(self):
        with self.assertRaises(CommandParseError):
            parse_serial_command("   ")

    def test_unterminated_quote_raises(self):
        with self.assertRaises(CommandParseError) as ctx:
            parse_serial_command('NET:SET,"unterminated,pass')
        self.assertIn("unterminated", str(ctx.exception))

    def test_escaped_quote_in_string(self):
        # Escaped quotes should work
        req = parse_serial_command(r'NET:SET,"test\"net","pass"')
        self.assertIn('"', req.params["ssid"])

    def test_empty_target_selector_raises(self):
        with self.assertRaises(CommandParseError) as ctx:
            parse_serial_command("MOVE:,100")
        self.assertIn("target selector missing", str(ctx.exception))

    def test_unknown_colon_command_raises(self):
        with self.assertRaises(UnsupportedCommandError):
            parse_serial_command("UNKNOWN:0,100")

    def test_unknown_space_command_raises(self):
        with self.assertRaises(UnsupportedCommandError):
            parse_serial_command("FOOBAR 123")

    def test_large_integer_values(self):
        req = parse_serial_command("MOVE:0,999999999")
        self.assertEqual(req.params["position_steps"], 999999999)

    def test_command_request_cmd_id_default(self):
        req = parse_serial_command("MOVE:0,100")
        self.assertIsNone(req.cmd_id)


# =============================================================================
# CommandRequest Dataclass Tests
# =============================================================================
class TestCommandRequest(unittest.TestCase):
    """Tests for CommandRequest dataclass."""

    def test_default_cmd_id_is_none(self):
        req = CommandRequest(action="TEST", params={}, raw="TEST")
        self.assertIsNone(req.cmd_id)

    def test_cmd_id_can_be_set(self):
        req = CommandRequest(action="TEST", params={}, raw="TEST", cmd_id="abc-123")
        self.assertEqual(req.cmd_id, "abc-123")

    def test_params_is_dict(self):
        req = CommandRequest(action="TEST", params={"key": "value"}, raw="TEST")
        self.assertEqual(req.params["key"], "value")


if __name__ == "__main__":
    unittest.main()
