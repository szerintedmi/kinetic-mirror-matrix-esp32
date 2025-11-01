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

    def test_ingest_response_updates_pending(self) -> None:
        class StubClient:
            def __init__(self) -> None:
                self.published = []

            def loop_start(self) -> None:
                pass

            def loop_stop(self) -> None:
                pass

            def disconnect(self) -> None:
                pass

            def subscribe(self, *args, **kwargs) -> None:
                pass

            def publish(self, topic, payload, qos=1):
                self.published.append((topic, payload, qos))

                class _Info:
                    rc = 0

                return _Info()

        ids = iter(["cli-0001", "cli-0002"])

        worker = MqttWorker(
            client_factory=lambda: StubClient(),
            node_id="aa",
            cmd_id_factory=lambda: next(ids),
        )
        client = worker._client_factory()  # type: ignore[attr-defined]
        worker._client = client
        with worker._lock:
            worker._connected = True

        handles = worker.queue_cmd("MOVE:0,100")
        self.assertTrue(handles)
        self.assertEqual(len(client.published), 1)  # type: ignore[attr-defined]
        topic, payload, qos = client.published[0]  # type: ignore[index]
        self.assertEqual(topic, "devices/aa/cmd")
        self.assertEqual(qos, 1)
        payload_obj = json.loads(payload)
        self.assertIn("cmd_id", payload_obj)
        host_cmd_id = payload_obj["cmd_id"]
        self.assertIsInstance(host_cmd_id, str)

        ack_payload = json.dumps(
            {
                "cmd_id": host_cmd_id,
                "action": "MOVE",
                "status": "ack",
                "result": {"est_ms": 150},
            }
        )
        done_payload = json.dumps(
            {
                "cmd_id": host_cmd_id,
                "action": "MOVE",
                "status": "done",
                "result": {"actual_ms": 140},
            }
        )
        worker.ingest_response("devices/aa/cmd/resp", ack_payload)
        worker.ingest_response("devices/aa/cmd/resp", done_payload)

        pending_map = worker.wait_for_completion(handles, timeout=0.1)
        pending = pending_map.get(handles[0])
        self.assertIsNotNone(pending)
        self.assertTrue(pending.completed)  # type: ignore[union-attr]
        self.assertEqual(pending.cmd_id, host_cmd_id)  # type: ignore[union-attr]
        self.assertGreaterEqual(len(pending.events), 2)  # type: ignore[union-attr]

        net_payload = json.dumps(
            {
                "cmd_id": "net123",
                "action": "NET:STATUS",
                "status": "done",
                "result": {
                    "state": "AP_ACTIVE",
                    "ssid": "DeviceAP",
                    "ip": "192.168.4.1",
                },
            }
        )
        worker.ingest_response("devices/aa/cmd/resp", net_payload)

        net_info = worker.get_net_info()
        self.assertEqual(net_info.get("state"), "AP_ACTIVE")
        self.assertEqual(net_info.get("ssid"), "DeviceAP")
        self.assertEqual(net_info.get("ip"), "192.168.4.1")

        _, log, _, _, _ = worker.get_state()
        joined = "\n".join(log)
        self.assertIn("[ACK]", joined)
        self.assertIn("[DONE]", joined)


if __name__ == "__main__":  # pragma: no cover
    unittest.main()
