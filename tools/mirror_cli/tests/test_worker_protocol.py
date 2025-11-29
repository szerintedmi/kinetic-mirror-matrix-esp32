"""Tests to verify workers implement the WorkerProtocol interface."""

import unittest
from typing import Dict, List, Optional, Tuple


class MockSerialModule:
    """Mock serial module for testing."""

    def Serial(self, port: str, baud: int, timeout: float = 0.02):
        class MockSerial:
            def __init__(self):
                self._is_open = True
                self.in_waiting = 0

            def read(self, size: int = 1) -> bytes:
                return b""

            def write(self, data: bytes) -> int:
                return len(data)

            def reset_input_buffer(self):
                pass

            def reset_output_buffer(self):
                pass

            def close(self):
                self._is_open = False

        return MockSerial()


class MockMqttClient:
    """Mock MQTT client for testing."""

    def __init__(self):
        self.published = []

    def loop_start(self):
        pass

    def loop_stop(self):
        pass

    def disconnect(self):
        pass

    def subscribe(self, *args, **kwargs):
        pass

    def publish(self, topic, payload, qos=1):
        self.published.append((topic, payload, qos))

        class Info:
            rc = 0

        return Info()


def _parse_status(text: str) -> List[Dict[str, str]]:
    return []


def _parse_kv(text: str) -> Dict[str, str]:
    return {}


def _parse_thermal(text: str) -> Optional[Tuple[bool, Optional[int]]]:
    return None


class TestSerialWorkerImplementsProtocol(unittest.TestCase):
    """Test that SerialWorker implements WorkerProtocol."""

    def setUp(self):
        from tools.mirror_cli.runtime import SerialWorker

        self.worker = SerialWorker(
            port="/dev/test",
            baud=115200,
            timeout=1.0,
            poll_rate_hz=2.0,
            serial_module=MockSerialModule(),
            parse_status=_parse_status,
            parse_kv=_parse_kv,
            parse_thermal=_parse_thermal,
        )

    def test_has_start_method(self):
        self.assertTrue(callable(getattr(self.worker, "start", None)))

    def test_has_stop_method(self):
        self.assertTrue(callable(getattr(self.worker, "stop", None)))

    def test_has_join_method(self):
        self.assertTrue(callable(getattr(self.worker, "join", None)))

    def test_has_is_alive_method(self):
        self.assertTrue(callable(getattr(self.worker, "is_alive", None)))

    def test_has_queue_cmd_method(self):
        self.assertTrue(callable(getattr(self.worker, "queue_cmd", None)))

    def test_has_get_state_method(self):
        self.assertTrue(callable(getattr(self.worker, "get_state", None)))

    def test_has_get_net_info_method(self):
        self.assertTrue(callable(getattr(self.worker, "get_net_info", None)))

    def test_has_get_thermal_state_method(self):
        self.assertTrue(callable(getattr(self.worker, "get_thermal_state", None)))

    def test_has_get_thermal_status_text_method(self):
        self.assertTrue(callable(getattr(self.worker, "get_thermal_status_text", None)))

    def test_has_append_log_method(self):
        self.assertTrue(callable(getattr(self.worker, "append_log", None)))

    def test_queue_cmd_returns_list(self):
        result = self.worker.queue_cmd("TEST")
        self.assertIsInstance(result, list)

    def test_get_state_returns_tuple(self):
        state = self.worker.get_state()
        self.assertIsInstance(state, tuple)
        self.assertEqual(len(state), 7)

    def test_get_net_info_returns_dict(self):
        info = self.worker.get_net_info()
        self.assertIsInstance(info, dict)

    def test_get_thermal_status_text_returns_str(self):
        text = self.worker.get_thermal_status_text()
        self.assertIsInstance(text, str)


class TestMqttWorkerImplementsProtocol(unittest.TestCase):
    """Test that MqttWorker implements WorkerProtocol."""

    def setUp(self):
        from tools.mirror_cli.mqtt_runtime import MqttWorker

        self.worker = MqttWorker(client_factory=lambda: MockMqttClient())

    def tearDown(self):
        self.worker.stop()

    def test_has_start_method(self):
        self.assertTrue(callable(getattr(self.worker, "start", None)))

    def test_has_stop_method(self):
        self.assertTrue(callable(getattr(self.worker, "stop", None)))

    def test_has_join_method(self):
        self.assertTrue(callable(getattr(self.worker, "join", None)))

    def test_has_is_alive_method(self):
        self.assertTrue(callable(getattr(self.worker, "is_alive", None)))

    def test_has_queue_cmd_method(self):
        self.assertTrue(callable(getattr(self.worker, "queue_cmd", None)))

    def test_has_get_state_method(self):
        self.assertTrue(callable(getattr(self.worker, "get_state", None)))

    def test_has_get_net_info_method(self):
        self.assertTrue(callable(getattr(self.worker, "get_net_info", None)))

    def test_has_get_thermal_state_method(self):
        self.assertTrue(callable(getattr(self.worker, "get_thermal_state", None)))

    def test_has_get_thermal_status_text_method(self):
        self.assertTrue(callable(getattr(self.worker, "get_thermal_status_text", None)))

    def test_has_append_log_method(self):
        self.assertTrue(callable(getattr(self.worker, "append_log", None)))

    def test_queue_cmd_returns_list(self):
        result = self.worker.queue_cmd("TEST")
        self.assertIsInstance(result, list)

    def test_get_state_returns_tuple(self):
        state = self.worker.get_state()
        self.assertIsInstance(state, tuple)
        self.assertEqual(len(state), 7)

    def test_get_net_info_returns_dict(self):
        info = self.worker.get_net_info()
        self.assertIsInstance(info, dict)

    def test_get_thermal_status_text_returns_str(self):
        text = self.worker.get_thermal_status_text()
        self.assertIsInstance(text, str)


if __name__ == "__main__":
    unittest.main()
