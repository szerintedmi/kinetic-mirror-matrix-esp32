"""Tests for response_events.py - event parsing and formatting."""

import unittest

from tools.mirror_cli.response_events import (
    EventType,
    ResponseEvent,
    format_event,
    parse_mqtt_payload,
    parse_serial_line,
    strip_quotes,
)


class TestParseSerialLine(unittest.TestCase):
    """Tests for parse_serial_line() function."""

    # -------------------------------------------------------------------------
    # CTRL: ACK events
    # -------------------------------------------------------------------------
    def test_ctrl_ack_simple(self):
        event = parse_serial_line("CTRL:ACK")
        self.assertIsNotNone(event)
        self.assertEqual(event.event_type, EventType.ACK)
        self.assertEqual(event.source, "serial")
        self.assertIsNone(event.cmd_id)

    def test_ctrl_ack_with_cmd_id(self):
        event = parse_serial_line("CTRL:ACK cmd_id=abc-123 est_ms=500")
        self.assertIsNotNone(event)
        self.assertEqual(event.event_type, EventType.ACK)
        self.assertEqual(event.cmd_id, "abc-123")
        self.assertEqual(event.attributes.get("est_ms"), "500")

    def test_ctrl_ack_with_msg_id(self):
        event = parse_serial_line("CTRL:ACK msg_id=xyz-789")
        self.assertIsNotNone(event)
        self.assertEqual(event.msg_id, "xyz-789")
        # cmd_id should be populated from msg_id if not present
        self.assertEqual(event.cmd_id, "xyz-789")

    def test_ctrl_ack_with_action(self):
        event = parse_serial_line("CTRL:ACK action=MOVE cmd_id=123")
        self.assertIsNotNone(event)
        self.assertEqual(event.action, "MOVE")

    # -------------------------------------------------------------------------
    # CTRL: DONE events
    # -------------------------------------------------------------------------
    def test_ctrl_done_simple(self):
        event = parse_serial_line("CTRL:DONE")
        self.assertIsNotNone(event)
        self.assertEqual(event.event_type, EventType.DONE)

    def test_ctrl_done_with_attributes(self):
        event = parse_serial_line("CTRL:DONE cmd_id=abc actual_ms=1500")
        self.assertIsNotNone(event)
        self.assertEqual(event.event_type, EventType.DONE)
        self.assertEqual(event.cmd_id, "abc")
        self.assertEqual(event.attributes.get("actual_ms"), "1500")

    # -------------------------------------------------------------------------
    # CTRL: WARN events
    # -------------------------------------------------------------------------
    def test_ctrl_warn_with_code(self):
        event = parse_serial_line("CTRL:WARN W001 Motor stalled")
        self.assertIsNotNone(event)
        self.assertEqual(event.event_type, EventType.WARN)
        self.assertEqual(event.code, "W001")
        self.assertEqual(event.reason, "Motor stalled")

    def test_ctrl_warn_with_code_attr(self):
        event = parse_serial_line("CTRL:WARN code=W002 reason=Overheat")
        self.assertIsNotNone(event)
        self.assertEqual(event.event_type, EventType.WARN)
        self.assertEqual(event.code, "W002")
        self.assertEqual(event.reason, "Overheat")

    # -------------------------------------------------------------------------
    # CTRL: ERR events
    # -------------------------------------------------------------------------
    def test_ctrl_err_simple(self):
        event = parse_serial_line("CTRL:ERR E001 Invalid command")
        self.assertIsNotNone(event)
        self.assertEqual(event.event_type, EventType.ERROR)
        self.assertEqual(event.code, "E001")
        self.assertEqual(event.reason, "Invalid command")

    def test_ctrl_err_with_cmd_id(self):
        event = parse_serial_line("CTRL:ERR cmd_id=xyz E002 Motor busy")
        self.assertIsNotNone(event)
        self.assertEqual(event.event_type, EventType.ERROR)
        self.assertEqual(event.cmd_id, "xyz")
        self.assertEqual(event.code, "E002")
        self.assertEqual(event.reason, "Motor busy")

    # -------------------------------------------------------------------------
    # CTRL: INFO events
    # -------------------------------------------------------------------------
    def test_ctrl_info_basic(self):
        event = parse_serial_line("CTRL:INFO status=ready")
        self.assertIsNotNone(event)
        self.assertEqual(event.event_type, EventType.INFO)
        self.assertEqual(event.status, "ready")

    def test_ctrl_unknown_kind(self):
        # Unknown CTRL: kind should default to INFO
        event = parse_serial_line("CTRL:CUSTOM foo=bar")
        self.assertIsNotNone(event)
        self.assertEqual(event.event_type, EventType.INFO)
        self.assertEqual(event.attributes.get("foo"), "bar")

    # -------------------------------------------------------------------------
    # NET: events
    # -------------------------------------------------------------------------
    def test_net_status_line(self):
        event = parse_serial_line("NET:STATUS device=AA:BB:CC state=ready ip=192.168.1.100")
        self.assertIsNotNone(event)
        self.assertEqual(event.event_type, EventType.DATA)
        self.assertEqual(event.attributes.get("device"), "AA:BB:CC")
        self.assertEqual(event.attributes.get("state"), "ready")
        self.assertEqual(event.attributes.get("ip"), "192.168.1.100")

    def test_ssid_line(self):
        event = parse_serial_line('SSID="MyNetwork" signal=-50')
        self.assertIsNotNone(event)
        self.assertEqual(event.event_type, EventType.DATA)
        self.assertEqual(event.attributes.get("SSID"), "MyNetwork")
        self.assertEqual(event.attributes.get("signal"), "-50")

    def test_net_list_with_commas(self):
        event = parse_serial_line("NET:LIST device=mac1,device=mac2")
        self.assertIsNotNone(event)
        self.assertEqual(event.event_type, EventType.DATA)

    # -------------------------------------------------------------------------
    # Edge cases
    # -------------------------------------------------------------------------
    def test_empty_line(self):
        event = parse_serial_line("")
        self.assertIsNone(event)

    def test_whitespace_only(self):
        event = parse_serial_line("   \t  ")
        self.assertIsNone(event)

    def test_non_ctrl_non_net_line(self):
        # Regular lines that don't match patterns return None
        event = parse_serial_line("id=0, pos=100, moving=0")
        self.assertIsNone(event)

    def test_trailing_comma_in_value(self):
        # Values with trailing commas should have comma stripped
        event = parse_serial_line("CTRL:ACK est_ms=500,")
        self.assertIsNotNone(event)
        self.assertEqual(event.attributes.get("est_ms"), "500")

    def test_raw_preserved(self):
        line = "CTRL:ACK cmd_id=test123"
        event = parse_serial_line(line)
        self.assertIsNotNone(event)
        self.assertEqual(event.raw, line)


class TestParseMqttPayload(unittest.TestCase):
    """Tests for parse_mqtt_payload() function."""

    def test_ack_status(self):
        payload = {"status": "ack", "action": "MOVE", "cmd_id": "abc-123"}
        event = parse_mqtt_payload(payload)
        self.assertIsNotNone(event)
        self.assertEqual(event.event_type, EventType.ACK)
        self.assertEqual(event.action, "MOVE")
        self.assertEqual(event.cmd_id, "abc-123")
        self.assertEqual(event.source, "mqtt")

    def test_done_status(self):
        payload = {"status": "done", "cmd_id": "xyz"}
        event = parse_mqtt_payload(payload)
        self.assertIsNotNone(event)
        self.assertEqual(event.event_type, EventType.DONE)

    def test_error_status(self):
        payload = {"status": "error", "cmd_id": "err1"}
        event = parse_mqtt_payload(payload)
        self.assertIsNotNone(event)
        self.assertEqual(event.event_type, EventType.ERROR)

    def test_warn_status(self):
        payload = {"status": "warn"}
        event = parse_mqtt_payload(payload)
        self.assertIsNotNone(event)
        self.assertEqual(event.event_type, EventType.WARN)

    def test_unknown_status_defaults_to_info(self):
        payload = {"status": "custom"}
        event = parse_mqtt_payload(payload)
        self.assertIsNotNone(event)
        self.assertEqual(event.event_type, EventType.INFO)

    def test_result_attributes(self):
        payload = {
            "status": "done",
            "result": {"est_ms": 500, "actual_ms": 480, "foo": None},
        }
        event = parse_mqtt_payload(payload)
        self.assertIsNotNone(event)
        self.assertEqual(event.attributes.get("est_ms"), "500")
        self.assertEqual(event.attributes.get("actual_ms"), "480")
        self.assertEqual(event.attributes.get("foo"), "")  # None becomes empty string

    def test_warnings_array(self):
        payload = {
            "status": "done",
            "warnings": [
                {"code": "W001", "reason": "Low voltage"},
                {"code": "W002", "message": "High temp"},
            ],
        }
        event = parse_mqtt_payload(payload)
        self.assertIsNotNone(event)
        self.assertEqual(len(event.warnings), 2)
        self.assertEqual(event.warnings[0]["code"], "W001")
        self.assertEqual(event.warnings[1]["code"], "W002")

    def test_errors_array(self):
        payload = {
            "status": "error",
            "errors": [{"code": "E001", "reason": "Motor busy"}],
        }
        event = parse_mqtt_payload(payload)
        self.assertIsNotNone(event)
        self.assertEqual(len(event.errors), 1)
        # First error should populate code/reason
        self.assertEqual(event.code, "E001")
        self.assertEqual(event.reason, "Motor busy")

    def test_errors_with_message_field(self):
        payload = {
            "status": "error",
            "errors": [{"code": "E002", "message": "Timeout"}],
        }
        event = parse_mqtt_payload(payload)
        self.assertIsNotNone(event)
        self.assertEqual(event.reason, "Timeout")

    def test_msg_id_fallback(self):
        payload = {"status": "ack", "msg_id": "legacy-id"}
        event = parse_mqtt_payload(payload)
        self.assertIsNotNone(event)
        self.assertEqual(event.cmd_id, "legacy-id")

    def test_empty_result_dict(self):
        payload = {"status": "done", "result": {}}
        event = parse_mqtt_payload(payload)
        self.assertIsNotNone(event)
        self.assertEqual(event.attributes, {})

    def test_result_not_dict(self):
        payload = {"status": "done", "result": "not_a_dict"}
        event = parse_mqtt_payload(payload)
        self.assertIsNotNone(event)
        self.assertEqual(event.attributes, {})

    def test_non_dict_payload(self):
        event = parse_mqtt_payload("not a dict")
        self.assertIsNone(event)

    def test_none_payload(self):
        event = parse_mqtt_payload(None)
        self.assertIsNone(event)


class TestFormatEvent(unittest.TestCase):
    """Tests for format_event() function."""

    def test_basic_ack(self):
        event = ResponseEvent(
            source="serial",
            raw="CTRL:ACK",
            event_type=EventType.ACK,
        )
        formatted = format_event(event)
        self.assertIn("[ACK]", formatted)

    def test_with_action_and_cmd_id(self):
        event = ResponseEvent(
            source="mqtt",
            raw="",
            event_type=EventType.DONE,
            action="MOVE",
            cmd_id="abc-123",
        )
        formatted = format_event(event)
        self.assertIn("[DONE]", formatted)
        self.assertIn("action=MOVE", formatted)
        self.assertIn("cmd_id=abc-123", formatted)

    def test_msg_id_shown_if_no_cmd_id(self):
        event = ResponseEvent(
            source="serial",
            raw="",
            event_type=EventType.ACK,
            msg_id="msg-456",
        )
        formatted = format_event(event)
        self.assertIn("msg_id=msg-456", formatted)

    def test_error_with_code_and_reason(self):
        event = ResponseEvent(
            source="serial",
            raw="",
            event_type=EventType.ERROR,
            code="E001",
            reason="Invalid motor ID",
        )
        formatted = format_event(event)
        self.assertIn("[ERROR]", formatted)
        self.assertIn("code=E001", formatted)
        self.assertIn("Invalid motor ID", formatted)

    def test_status_not_shown_if_matches_event_type(self):
        event = ResponseEvent(
            source="mqtt",
            raw="",
            event_type=EventType.ACK,
            status="ack",
        )
        formatted = format_event(event)
        # status=ack should NOT appear since it matches event_type
        self.assertNotIn("status=ack", formatted)

    def test_status_shown_if_different(self):
        event = ResponseEvent(
            source="mqtt",
            raw="",
            event_type=EventType.INFO,
            status="ready",
        )
        formatted = format_event(event)
        self.assertIn("status=ready", formatted)

    def test_attributes_sorted(self):
        event = ResponseEvent(
            source="serial",
            raw="",
            event_type=EventType.ACK,
            attributes={"z_val": "1", "a_val": "2", "m_val": "3"},
        )
        formatted = format_event(event)
        # Attributes should be sorted alphabetically
        idx_a = formatted.find("a_val=")
        idx_m = formatted.find("m_val=")
        idx_z = formatted.find("z_val=")
        self.assertLess(idx_a, idx_m)
        self.assertLess(idx_m, idx_z)

    def test_text_attribute_as_multiline_block(self):
        event = ResponseEvent(
            source="serial",
            raw="",
            event_type=EventType.INFO,
            attributes={"text": "Line 1\nLine 2\nLine 3"},
        )
        formatted = format_event(event)
        # text should be rendered as indented block
        self.assertIn("  Line 1", formatted)
        self.assertIn("  Line 2", formatted)

    def test_warnings_list_formatted(self):
        event = ResponseEvent(
            source="mqtt",
            raw="",
            event_type=EventType.DONE,
            warnings=[{"code": "W001", "reason": "Low battery"}],
        )
        formatted = format_event(event)
        self.assertIn("warnings=[W001:Low battery]", formatted)

    def test_errors_list_formatted(self):
        event = ResponseEvent(
            source="mqtt",
            raw="",
            event_type=EventType.ERROR,
            errors=[{"code": "E001", "message": "Timeout"}],
        )
        formatted = format_event(event)
        self.assertIn("errors=[E001:Timeout]", formatted)

    def test_latency_ms_appended(self):
        event = ResponseEvent(
            source="serial",
            raw="",
            event_type=EventType.ACK,
        )
        formatted = format_event(event, latency_ms=123.456)
        self.assertIn("latency_ms=123", formatted)

    def test_device_label_prefix(self):
        """Device label should be prepended to output."""
        event = ResponseEvent(
            source="mqtt",
            raw="",
            event_type=EventType.ACK,
            action="MOVE",
        )
        formatted = format_event(event, device_label="1")
        self.assertTrue(formatted.startswith("[1] [ACK]"))
        self.assertIn("action=MOVE", formatted)

    def test_device_label_none_no_prefix(self):
        """No prefix when device_label is None."""
        event = ResponseEvent(
            source="mqtt",
            raw="",
            event_type=EventType.ACK,
        )
        formatted = format_event(event, device_label=None)
        self.assertTrue(formatted.startswith("[ACK]"))

    def test_device_label_with_latency(self):
        """Device label and latency should both appear."""
        event = ResponseEvent(
            source="mqtt",
            raw="",
            event_type=EventType.DONE,
            action="HOME",
        )
        formatted = format_event(event, latency_ms=50.0, device_label="2")
        self.assertTrue(formatted.startswith("[2] [DONE]"))
        self.assertIn("latency_ms=50", formatted)


class TestResponseEventDataclass(unittest.TestCase):
    """Tests for ResponseEvent dataclass."""

    def test_attribute_method(self):
        event = ResponseEvent(
            source="serial",
            raw="",
            event_type=EventType.ACK,
            attributes={"key1": "value1", "key2": "value2"},
        )
        self.assertEqual(event.attribute("key1"), "value1")
        self.assertEqual(event.attribute("missing"), None)
        self.assertEqual(event.attribute("missing", "default"), "default")


class TestStripQuotes(unittest.TestCase):
    """Tests for strip_quotes utility function."""

    def test_removes_double_quotes(self):
        self.assertEqual(strip_quotes('"hello"'), "hello")

    def test_preserves_unquoted(self):
        self.assertEqual(strip_quotes("hello"), "hello")

    def test_preserves_single_quotes(self):
        self.assertEqual(strip_quotes("'hello'"), "'hello'")

    def test_preserves_mismatched_quotes(self):
        self.assertEqual(strip_quotes('"hello'), '"hello')

    def test_empty_string(self):
        self.assertEqual(strip_quotes(""), "")

    def test_only_quotes(self):
        self.assertEqual(strip_quotes('""'), "")

    def test_single_char(self):
        self.assertEqual(strip_quotes('"'), '"')


if __name__ == "__main__":
    unittest.main()
