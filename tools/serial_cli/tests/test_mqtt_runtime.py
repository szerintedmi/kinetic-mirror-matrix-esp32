import json
import unittest
from typing import List

from tools.serial_cli import render_table
from tools.serial_cli.mqtt_runtime import MqttWorker


class _NoOpClient:
    def __init__(self, *args, **kwargs) -> None:
        pass

    def loop_start(self) -> None:
        pass

    def loop_stop(self) -> None:
        pass

    def disconnect(self) -> None:
        pass

    def subscribe(self, *args, **kwargs) -> None:
        pass


class MqttRuntimeTests(unittest.TestCase):
    def setUp(self) -> None:
        self.worker = MqttWorker(client_factory=lambda: _NoOpClient())

    def tearDown(self) -> None:
        self.worker.stop()

    def _sample_payload(self) -> str:
        return json.dumps(
            {
                "node_state": "ready",
                "ip": "192.168.1.42",
                "motors": {
                    "0": {
                        "id": 0,
                        "position": 120,
                        "moving": False,
                        "awake": True,
                        "homed": True,
                        "steps_since_home": 360,
                        "budget_s": 1.8,
                        "ttfc_s": 0.4,
                        "speed": 4000,
                        "accel": 16000,
                        "est_ms": 240,
                        "started_ms": 812345,
                        "actual_ms": 230,
                    },
                    "1": {
                        "id": 1,
                        "position": -45,
                        "moving": True,
                        "awake": True,
                        "homed": False,
                        "steps_since_home": 12,
                        "budget_s": 0.0,
                        "ttfc_s": 0.0,
                        "speed": 3200,
                        "accel": 12000,
                        "est_ms": 180,
                        "started_ms": 712300,
                    },
                },
            }
        )

    def test_ingest_aggregate_status_rows(self) -> None:
        payload = self._sample_payload()
        self.worker.ingest_message("devices/02123456789a/status", payload, timestamp=100.0)

        rows, log, err, last_ts, _help = self.worker.get_state()

        self.assertIsNone(err)
        self.assertGreater(last_ts, 0.0)
        self.assertGreaterEqual(len(rows), 2)

        first = rows[0]
        self.assertIn("device", first)
        self.assertEqual("02123456789a", first["device"])
        self.assertEqual("ready", first["node_state"])
        self.assertEqual("0", first["id"])
        self.assertEqual("0", rows[0]["moving"])  # moving False -> "0"
        self.assertEqual("1", rows[1]["moving"])  # moving True -> "1"
        self.assertEqual("230", first["actual_ms"])
        self.assertEqual("", rows[1]["actual_ms"])
        self.assertTrue(first["age_s"] >= 0.0)

        table = render_table(rows)
        self.assertIn("id", table.splitlines()[0])
        self.assertIn("actual_ms", table.splitlines()[0])

    def test_multiple_devices_sorted(self) -> None:
        payload_a = self._sample_payload()
        payload_b = json.loads(self._sample_payload())
        payload_b["ip"] = "192.168.1.77"
        payload_b["motors"]["0"]["id"] = 0
        payload_b["motors"]["0"]["position"] = 999
        payload_b["motors"]["0"]["moving"] = False
        payload_b["motors"]["0"]["actual_ms"] = 100
        self.worker.ingest_message("devices/02123456789a/status", payload_a, timestamp=10.0)
        self.worker.ingest_message("devices/abcdefabcdef/status", json.dumps(payload_b), timestamp=20.0)

        rows, _, _, _, _ = self.worker.get_state()
        devices: List[str] = [row["device"] for row in rows]
        self.assertEqual(sorted(devices), devices)


if __name__ == "__main__":  # pragma: no cover
    unittest.main()

