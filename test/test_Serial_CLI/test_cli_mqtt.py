import json
import os
import sys

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))
import io
import time
from contextlib import redirect_stdout
from unittest import mock

import tools.serial_cli
import serial_cli
from serial_cli import render_table, main as cli_main
from tools.serial_cli.mqtt_runtime import MqttWorker


def test_render_status_table():
    rows = [
        {
            "device": "02123456789a",
            "id": "0",
            "pos": "120",
            "moving": "1",
            "awake": "1",
            "homed": "1",
            "steps_since_home": "360",
            "budget_s": "1.8",
            "ttfc_s": "0.4",
            "est_ms": "240",
            "started_ms": "812345",
            "actual_ms": "230",
        },
        {
            "device": "02123456789a",
            "id": "1",
            "pos": "-45",
            "moving": "0",
            "awake": "1",
            "homed": "0",
            "steps_since_home": "12",
            "budget_s": "0.0",
            "ttfc_s": "0.0",
            "est_ms": "180",
            "started_ms": "712300",
            "actual_ms": "",
        },
    ]
    output = render_table(rows)
    assert "id" in output
    assert "pos" in output
    assert "moving" in output
    assert "awake" in output
    assert "actual_ms" in output


class StubMqttClient:
    def __init__(self):
        self.on_connect = None
        self.on_disconnect = None
        self.on_message = None
        self._connected = False
        self.subscriptions = []

    def username_pw_set(self, user, password):
        self._user = user
        self._password = password

    def connect(self, host, port, keepalive=30):
        self._host = host
        self._port = port
        self._keepalive = keepalive
        self._connected = True
        if self.on_connect:
            self.on_connect(self, None, None, 0)

    def disconnect(self):
        was_connected = self._connected
        self._connected = False
        if was_connected and self.on_disconnect:
            self.on_disconnect(self, None, 0)

    def loop_start(self):
        pass

    def loop_stop(self):
        pass

    def subscribe(self, topic, qos=0):
        self.subscriptions.append((topic, qos))
        return 1

    def emit_message(self, topic, payload):
        if self.on_message:
            class _Msg:
                pass
            msg = _Msg()
            msg.topic = topic
            msg.payload = payload.encode()
            self.on_message(self, None, msg)


def _sample_payload(actual_ms=True):
    motors = {
        "0": {
            "id": 0,
            "position": 120,
            "moving": True,
            "awake": True,
            "homed": True,
            "steps_since_home": 360,
            "budget_s": 1.8,
            "ttfc_s": 0.4,
            "speed": 4000,
            "accel": 16000,
            "est_ms": 240,
            "started_ms": 812345,
        },
        "1": {
            "id": 1,
            "position": -45,
            "moving": False,
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
    }
    if actual_ms:
        motors["0"]["actual_ms"] = 230
    return json.dumps({"node_state": "ready", "ip": "10.0.0.2", "motors": motors})


def test_mqtt_worker_ingest_snapshot():
    worker = MqttWorker(broker={}, client_factory=lambda: None)
    payload = _sample_payload()
    worker.ingest_message("devices/aa/status", payload, timestamp=100.0)
    rows, log, err, ts, _ = worker.get_state()
    assert err is None
    assert len(rows) == 2
    first = rows[0]
    assert first["device"] == "aa"
    assert first["node_state"] == "ready"
    assert first["ip"] == "10.0.0.2"
    assert first["id"] == "0"
    assert first["moving"] == "1"
    assert first["actual_ms"] == "230"
    assert isinstance(first["age_s"], float)

    second = rows[1]
    assert second["id"] == "1"
    assert second["moving"] == "0"
    assert second["actual_ms"] == ""

    worker.ingest_message("devices/aa/status", _sample_payload(actual_ms=False), timestamp=105.0)
    rows_updated, _, _, _, _ = worker.get_state()
    assert rows_updated[0]["actual_ms"] == ""
    assert rows_updated[1]["actual_ms"] == ""


def test_cli_mqtt_command_error():
    buf = io.StringIO()
    with redirect_stdout(buf):
        rc = cli_main(["move", "0", "100", "--transport", "mqtt"])
    output = buf.getvalue()
    assert rc == 1
    assert "error: MQTT transport not implemented yet" in output


def test_mqtt_worker_queue_cmd_logs_error():
    worker = MqttWorker(broker={}, client_factory=lambda: None)
    worker.queue_cmd("STATUS")
    _, log, _, _, _ = worker.get_state()
    assert log[-2] == "> STATUS"
    assert log[-1] == "error: MQTT transport not implemented yet"


def test_status_mqtt_uses_worker():
    class StubWorker:
        def __init__(self):
            self.started = False
            self.stopped = False

        def start(self):
            self.started = True

        def stop(self):
            self.stopped = True

        def join(self, timeout=None):
            pass

        def get_state(self):
            now = time.time()
            rows = [
                {
                    "device": "02123456789a",
                    "node_state": "ready",
                    "ip": "10.0.0.2",
                    "id": "0",
                    "pos": "120",
                    "moving": "1",
                    "awake": "1",
                    "homed": "1",
                    "steps_since_home": "360",
                    "budget_s": "1.8",
                    "ttfc_s": "0.4",
                    "est_ms": "240",
                    "started_ms": "812345",
                    "actual_ms": "230",
                    "last_seen": now,
                    "age_s": 0.05,
                }
            ]
            log = ["[mqtt] connected; subscribed to devices/+/status"]
            return rows, log, None, now, ""

    stub = StubWorker()
    with mock.patch("tools.serial_cli._make_mqtt_worker", return_value=stub), \
         mock.patch("tools.serial_cli.time.sleep", lambda *_: None):
        buf = io.StringIO()
        with redirect_stdout(buf):
            rc = cli_main(["status", "--transport", "mqtt", "--timeout", "0.5"])
    output = buf.getvalue()
    assert rc == 0
    assert "id" in output
    assert "120" in output
    assert stub.started and stub.stopped


def test_mqtt_worker_integration_latency_and_debounce():
    stub = StubMqttClient()
    worker = MqttWorker(broker={"host": "localhost", "port": 1883}, client_factory=lambda: stub)
    worker.start()
    try:
        deadline = time.time() + 2.0
        connected = False
        while time.time() < deadline:
            _, logs, _, _, _ = worker.get_state()
            if logs and "connected; subscribed" in logs[-1]:
                connected = True
                break
            time.sleep(0.05)
        assert connected, "worker failed to connect within timeout"

        mac = "02123456789a"
        topic = f"devices/{mac}/status"
        payload1 = _sample_payload()
        stub.emit_message(topic, payload1)
        time.sleep(0.05)
        rows, log, err, last_ts, _ = worker.get_state()
        assert len(rows) == 2
        first = rows[0]
        assert first["device"] == mac
        assert first["node_state"] == "ready"
        assert first["id"] == "0"
        assert first["moving"] == "1"
        assert first["ip"] == "10.0.0.2"
        latency = time.time() - first["last_seen"]
        assert latency < 0.25

        stub.emit_message(topic, payload1)
        time.sleep(0.05)
        rows_dup, log_dup, _, _, _ = worker.get_state()
        assert log_dup == log

        payload2 = _sample_payload(actual_ms=False)
        stub.emit_message(topic, payload2)
        time.sleep(0.05)
        rows_new, log_new, _, _, _ = worker.get_state()
        assert rows_new[0]["actual_ms"] == ""
    finally:
        worker.stop()
        worker.join(timeout=1)


def main():
    try:
        test_render_status_table()
        test_mqtt_worker_ingest_snapshot()
        test_cli_mqtt_command_error()
        test_mqtt_worker_queue_cmd_logs_error()
        test_status_mqtt_uses_worker()
        test_mqtt_worker_integration_latency_and_debounce()
    except AssertionError:
        print("Tests failed.")
        return 1
    print("All MQTT CLI tests passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
