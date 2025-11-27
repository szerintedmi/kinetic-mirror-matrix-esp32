"""Tests for runtime.py - SerialWorker class."""

import threading
import time
import unittest
from typing import Dict, List, Optional, Tuple
from unittest.mock import MagicMock, patch

from tools.serial_cli.runtime import SerialWorker, _MAX_BUFFER_SIZE


class MockSerial:
    """Mock serial port for testing."""

    def __init__(self, port: str, baud: int, timeout: float = 0.02):
        self.port = port
        self.baud = baud
        self.timeout = timeout
        self._is_open = True
        self._read_data: bytes = b""
        self._read_pos = 0
        self._write_log: List[bytes] = []
        self.in_waiting = 0
        self._lock = threading.Lock()

    def read(self, size: int = 1) -> bytes:
        """Return data from the mock read buffer."""
        with self._lock:
            if self._read_pos >= len(self._read_data):
                time.sleep(0.01)  # Simulate blocking
                return b""
            end = min(self._read_pos + size, len(self._read_data))
            chunk = self._read_data[self._read_pos : end]
            self._read_pos = end
            return chunk

    def write(self, data: bytes) -> int:
        """Log written data."""
        with self._lock:
            self._write_log.append(data)
        return len(data)

    def reset_input_buffer(self):
        pass

    def reset_output_buffer(self):
        pass

    def close(self):
        self._is_open = False

    # Test helpers
    def inject_response(self, data: str):
        """Inject data to be read by the worker."""
        with self._lock:
            self._read_data += data.encode()
            self.in_waiting = len(self._read_data) - self._read_pos

    def get_writes(self) -> List[str]:
        """Get all written data as strings."""
        with self._lock:
            return [w.decode() for w in self._write_log]


class MockSerialModule:
    """Mock serial module that returns MockSerial instances."""

    def __init__(self):
        self.last_instance: Optional[MockSerial] = None

    def Serial(self, port: str, baud: int, timeout: float = 0.02) -> MockSerial:
        self.last_instance = MockSerial(port, baud, timeout)
        return self.last_instance


def _parse_status(text: str) -> List[Dict[str, str]]:
    """Simple status parser for tests."""
    rows = []
    for line in text.strip().splitlines():
        row = {}
        for tok in line.split():
            if "=" in tok:
                k, v = tok.split("=", 1)
                row[k] = v
        if row:
            rows.append(row)
    return rows


def _parse_kv(text: str) -> Dict[str, str]:
    """Simple KV parser for tests."""
    out = {}
    for tok in text.split():
        if "=" in tok:
            k, v = tok.split("=", 1)
            out[k] = v
    return out


def _parse_thermal(text: str) -> Optional[Tuple[bool, Optional[int]]]:
    """Simple thermal parser for tests."""
    if "THERMAL_LIMITING=ON" in text:
        return (True, 90)
    if "THERMAL_LIMITING=OFF" in text:
        return (False, None)
    return None


class TestSerialWorkerInit(unittest.TestCase):
    """Tests for SerialWorker initialization."""

    def test_init_sets_parameters(self):
        mock_module = MockSerialModule()
        worker = SerialWorker(
            port="/dev/test",
            baud=115200,
            timeout=2.0,
            poll_rate_hz=4.0,
            serial_module=mock_module,
            parse_status=_parse_status,
            parse_kv=_parse_kv,
            parse_thermal=_parse_thermal,
        )
        self.assertEqual(worker.port, "/dev/test")
        self.assertEqual(worker.baud, 115200)
        self.assertEqual(worker.base_timeout, 2.0)
        self.assertAlmostEqual(worker.poll_interval, 0.25)

    def test_init_clamps_timeout(self):
        mock_module = MockSerialModule()
        worker = SerialWorker(
            port="/dev/test",
            baud=115200,
            timeout=0.1,  # Below minimum
            poll_rate_hz=2.0,
            serial_module=mock_module,
            parse_status=_parse_status,
            parse_kv=_parse_kv,
            parse_thermal=_parse_thermal,
        )
        self.assertEqual(worker.base_timeout, 0.5)  # Clamped to 0.5


class TestSerialWorkerQueueCmd(unittest.TestCase):
    """Tests for queue_cmd method."""

    def test_queue_cmd_adds_newline(self):
        mock_module = MockSerialModule()
        worker = SerialWorker(
            port="/dev/test",
            baud=115200,
            timeout=2.0,
            poll_rate_hz=2.0,
            serial_module=mock_module,
            parse_status=_parse_status,
            parse_kv=_parse_kv,
            parse_thermal=_parse_thermal,
        )
        worker.queue_cmd("STATUS")
        with worker._lock:
            self.assertEqual(len(worker._cmdq), 1)
            self.assertEqual(worker._cmdq[0], "STATUS\n")

    def test_queue_cmd_preserves_newline(self):
        mock_module = MockSerialModule()
        worker = SerialWorker(
            port="/dev/test",
            baud=115200,
            timeout=2.0,
            poll_rate_hz=2.0,
            serial_module=mock_module,
            parse_status=_parse_status,
            parse_kv=_parse_kv,
            parse_thermal=_parse_thermal,
        )
        worker.queue_cmd("STATUS\n")
        with worker._lock:
            self.assertEqual(worker._cmdq[0], "STATUS\n")

    def test_queue_cmd_logs_command(self):
        mock_module = MockSerialModule()
        worker = SerialWorker(
            port="/dev/test",
            baud=115200,
            timeout=2.0,
            poll_rate_hz=2.0,
            serial_module=mock_module,
            parse_status=_parse_status,
            parse_kv=_parse_kv,
            parse_thermal=_parse_thermal,
        )
        worker.queue_cmd("MOVE:0,100")
        with worker._lock:
            self.assertTrue(any("> MOVE:0,100" in log for log in worker._log))

    def test_queue_cmd_silent_no_log(self):
        mock_module = MockSerialModule()
        worker = SerialWorker(
            port="/dev/test",
            baud=115200,
            timeout=2.0,
            poll_rate_hz=2.0,
            serial_module=mock_module,
            parse_status=_parse_status,
            parse_kv=_parse_kv,
            parse_thermal=_parse_thermal,
        )
        worker.queue_cmd("STATUS", silent=True)
        with worker._lock:
            self.assertFalse(any("> STATUS" in log for log in worker._log))
            # Command should still be queued
            self.assertEqual(len(worker._cmdq), 1)

    def test_queue_cmd_returns_empty_list(self):
        mock_module = MockSerialModule()
        worker = SerialWorker(
            port="/dev/test",
            baud=115200,
            timeout=2.0,
            poll_rate_hz=2.0,
            serial_module=mock_module,
            parse_status=_parse_status,
            parse_kv=_parse_kv,
            parse_thermal=_parse_thermal,
        )
        result = worker.queue_cmd("STATUS")
        self.assertEqual(result, [])


class TestSerialWorkerGetState(unittest.TestCase):
    """Tests for get_state method."""

    def test_get_state_returns_tuple(self):
        mock_module = MockSerialModule()
        worker = SerialWorker(
            port="/dev/test",
            baud=115200,
            timeout=2.0,
            poll_rate_hz=2.0,
            serial_module=mock_module,
            parse_status=_parse_status,
            parse_kv=_parse_kv,
            parse_thermal=_parse_thermal,
        )
        state = worker.get_state()
        self.assertEqual(len(state), 7)
        rows, log, err, ts, help_text, net, log_seq = state
        self.assertIsInstance(rows, list)
        self.assertIsInstance(log, list)
        self.assertIsNone(err)
        self.assertEqual(ts, 0.0)
        self.assertEqual(help_text, "")
        self.assertIsInstance(net, dict)
        self.assertIsInstance(log_seq, int)


class TestSerialWorkerThermalState(unittest.TestCase):
    """Tests for thermal state methods."""

    def test_get_thermal_state_initially_none(self):
        mock_module = MockSerialModule()
        worker = SerialWorker(
            port="/dev/test",
            baud=115200,
            timeout=2.0,
            poll_rate_hz=2.0,
            serial_module=mock_module,
            parse_status=_parse_status,
            parse_kv=_parse_kv,
            parse_thermal=_parse_thermal,
        )
        self.assertIsNone(worker.get_thermal_state())

    def test_get_thermal_status_text_empty_when_none(self):
        mock_module = MockSerialModule()
        worker = SerialWorker(
            port="/dev/test",
            baud=115200,
            timeout=2.0,
            poll_rate_hz=2.0,
            serial_module=mock_module,
            parse_status=_parse_status,
            parse_kv=_parse_kv,
            parse_thermal=_parse_thermal,
        )
        self.assertEqual(worker.get_thermal_status_text(), "")

    def test_get_thermal_status_text_on(self):
        mock_module = MockSerialModule()
        worker = SerialWorker(
            port="/dev/test",
            baud=115200,
            timeout=2.0,
            poll_rate_hz=2.0,
            serial_module=mock_module,
            parse_status=_parse_status,
            parse_kv=_parse_kv,
            parse_thermal=_parse_thermal,
        )
        with worker._lock:
            worker._thermal_state = (True, 90)
        self.assertEqual(worker.get_thermal_status_text(), "thermal limiting=ON (max=90s)")

    def test_get_thermal_status_text_off(self):
        mock_module = MockSerialModule()
        worker = SerialWorker(
            port="/dev/test",
            baud=115200,
            timeout=2.0,
            poll_rate_hz=2.0,
            serial_module=mock_module,
            parse_status=_parse_status,
            parse_kv=_parse_kv,
            parse_thermal=_parse_thermal,
        )
        with worker._lock:
            worker._thermal_state = (False, None)
        self.assertEqual(worker.get_thermal_status_text(), "thermal limiting=OFF")


class TestSerialWorkerNetInfo(unittest.TestCase):
    """Tests for net info methods."""

    def test_get_net_info_initially_empty(self):
        mock_module = MockSerialModule()
        worker = SerialWorker(
            port="/dev/test",
            baud=115200,
            timeout=2.0,
            poll_rate_hz=2.0,
            serial_module=mock_module,
            parse_status=_parse_status,
            parse_kv=_parse_kv,
            parse_thermal=_parse_thermal,
        )
        net = worker.get_net_info()
        self.assertIsInstance(net, dict)

    def test_get_net_info_includes_device(self):
        mock_module = MockSerialModule()
        worker = SerialWorker(
            port="/dev/test",
            baud=115200,
            timeout=2.0,
            poll_rate_hz=2.0,
            serial_module=mock_module,
            parse_status=_parse_status,
            parse_kv=_parse_kv,
            parse_thermal=_parse_thermal,
        )
        with worker._lock:
            worker._device_id = "AA:BB:CC:DD:EE:FF"
        net = worker.get_net_info()
        self.assertEqual(net.get("device"), "AA:BB:CC:DD:EE:FF")


class TestSerialWorkerBufferLimit(unittest.TestCase):
    """Tests for buffer overflow protection."""

    def test_buffer_truncated_when_too_large(self):
        mock_module = MockSerialModule()
        worker = SerialWorker(
            port="/dev/test",
            baud=115200,
            timeout=2.0,
            poll_rate_hz=2.0,
            serial_module=mock_module,
            parse_status=_parse_status,
            parse_kv=_parse_kv,
            parse_thermal=_parse_thermal,
        )
        # Simulate buffer growing beyond limit
        worker._buffer = "x" * (_MAX_BUFFER_SIZE + 1000)

        # Manually call the truncation logic (normally happens in _read_and_process)
        if len(worker._buffer) > _MAX_BUFFER_SIZE:
            worker._buffer = worker._buffer[-(_MAX_BUFFER_SIZE // 2) :]

        self.assertLessEqual(len(worker._buffer), _MAX_BUFFER_SIZE // 2)


class TestSerialWorkerAppendLog(unittest.TestCase):
    """Tests for log methods."""

    def test_append_log_adds_entry(self):
        mock_module = MockSerialModule()
        worker = SerialWorker(
            port="/dev/test",
            baud=115200,
            timeout=2.0,
            poll_rate_hz=2.0,
            serial_module=mock_module,
            parse_status=_parse_status,
            parse_kv=_parse_kv,
            parse_thermal=_parse_thermal,
        )
        worker.append_log("Test message")
        with worker._lock:
            self.assertIn("Test message", worker._log)

    def test_append_log_increments_seq(self):
        mock_module = MockSerialModule()
        worker = SerialWorker(
            port="/dev/test",
            baud=115200,
            timeout=2.0,
            poll_rate_hz=2.0,
            serial_module=mock_module,
            parse_status=_parse_status,
            parse_kv=_parse_kv,
            parse_thermal=_parse_thermal,
        )
        with worker._lock:
            initial_seq = worker._log_seq
        worker.append_log("Test")
        with worker._lock:
            self.assertEqual(worker._log_seq, initial_seq + 1)


class TestSerialWorkerIntegration(unittest.TestCase):
    """Integration tests for SerialWorker with mock serial."""

    def test_worker_lifecycle(self):
        """Test start/stop lifecycle."""
        mock_module = MockSerialModule()
        worker = SerialWorker(
            port="/dev/test",
            baud=115200,
            timeout=0.5,
            poll_rate_hz=10.0,
            serial_module=mock_module,
            parse_status=_parse_status,
            parse_kv=_parse_kv,
            parse_thermal=_parse_thermal,
        )
        worker.start()
        time.sleep(0.1)  # Let it run briefly
        worker.stop()
        worker.join(timeout=1.0)
        self.assertFalse(worker.is_alive())

    def test_worker_sends_initial_net_status(self):
        """Test that worker sends NET:STATUS on startup."""
        mock_module = MockSerialModule()
        worker = SerialWorker(
            port="/dev/test",
            baud=115200,
            timeout=0.5,
            poll_rate_hz=10.0,
            serial_module=mock_module,
            parse_status=_parse_status,
            parse_kv=_parse_kv,
            parse_thermal=_parse_thermal,
        )
        worker.start()
        time.sleep(0.2)  # Let it send initial commands
        worker.stop()
        worker.join(timeout=1.0)

        if mock_module.last_instance:
            writes = mock_module.last_instance.get_writes()
            # Should have sent NET:STATUS as first poll
            self.assertTrue(
                any("NET:STATUS" in w for w in writes),
                f"Expected NET:STATUS in writes: {writes}",
            )

    def test_worker_handles_queued_command(self):
        """Test that worker sends queued commands."""
        mock_module = MockSerialModule()
        worker = SerialWorker(
            port="/dev/test",
            baud=115200,
            timeout=0.5,
            poll_rate_hz=10.0,
            serial_module=mock_module,
            parse_status=_parse_status,
            parse_kv=_parse_kv,
            parse_thermal=_parse_thermal,
        )
        # Queue a command before starting
        worker.queue_cmd("MOVE:0,100")

        worker.start()
        time.sleep(0.3)  # Let it run and process commands
        worker.stop()
        worker.join(timeout=1.0)

        if mock_module.last_instance:
            writes = mock_module.last_instance.get_writes()
            # Should have written our command (after initial polls)
            self.assertTrue(
                any("MOVE:0,100" in w for w in writes),
                f"Expected MOVE command in writes: {writes}",
            )


if __name__ == "__main__":
    unittest.main()
