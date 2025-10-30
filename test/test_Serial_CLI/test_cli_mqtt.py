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


def test_render_presence_table():
    rows = [
        {"device": "02123456789a", "state": "ready", "ip": "10.0.0.2", "msg_id": "abc", "last_seen": time.time()},
        {"device": "02123456789b", "state": "offline", "ip": "", "msg_id": "def", "last_seen": time.time()},
    ]
    output = render_table(rows)
    assert "device" in output
    assert "02123456789a" in output
    assert "ready" in output


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


def test_mqtt_worker_ingest_duplicate():
    worker = MqttWorker(broker={}, client_factory=lambda: None)
    worker.ingest_message("devices/aa/state", '{"state":"ready","ip":"1.2.3.4","msg_id":"abc"}', timestamp=100.0)
    rows, log, err, ts, _ = worker.get_state()
    assert len(rows) == 1
    assert log == []
    assert isinstance(rows[0]["age_s"], float)

    # Duplicate msg_id should not append another log entry
    worker.ingest_message("devices/aa/state", '{"state":"ready","ip":"1.2.3.4","msg_id":"abc"}', timestamp=101.0)
    rows2, log2, _, _, _ = worker.get_state()
    assert log2 == log
    assert rows2[0]["state"] == "ready"

    # New msg_id records a new log entry
    worker.ingest_message("devices/aa/state", '{"state":"ready","ip":"1.2.3.4","msg_id":"abd"}', timestamp=102.0)
    _, log3, _, _, _ = worker.get_state()
    assert log3 == log


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
            rows = [{
                "device": "02123456789a",
                "state": "ready",
                "ip": "10.0.0.2",
                "msg_id": "abc",
                "last_seen": now,
            }]
            log = ["[ready] 02123456789a ip=10.0.0.2 msg_id=abc"]
            return rows, log, None, now, ""

    stub = StubWorker()
    with mock.patch("tools.serial_cli._make_mqtt_worker", return_value=stub), \
         mock.patch("tools.serial_cli.time.sleep", lambda *_: None):
        buf = io.StringIO()
        with redirect_stdout(buf):
            rc = cli_main(["status", "--transport", "mqtt", "--timeout", "0.5"])
    output = buf.getvalue()
    assert rc == 0
    assert "02123456789a" in output
    assert stub.started and stub.stopped


def test_mqtt_worker_integration_latency_and_debounce():
    stub = StubMqttClient()
    worker = MqttWorker(broker={"host": "localhost", "port": 1883}, client_factory=lambda: stub)
    worker.start()
    try:
        deadline = time.time() + 2.0
        connected = False
        while time.time() < deadline:
            rows, logs, _, _, _ = worker.get_state()
            if logs and "[mqtt] connected" in logs[-1]:
                connected = True
                break
            time.sleep(0.05)
        assert connected, "worker failed to connect within timeout"

        mac = "02123456789a"
        topic = f"devices/{mac}/state"
        payload1 = json.dumps({"state": "ready", "ip": "10.0.0.2", "msg_id": "abc"})
        send_time = time.time()
        stub.emit_message(topic, payload1)
        time.sleep(0.05)
        rows, log, err, last_ts, _ = worker.get_state()
        assert len(rows) == 1
        row = rows[0]
        assert row["device"] == mac
        assert row["state"] == "ready"
        assert row["ip"] == "10.0.0.2"
        assert row["msg_id"] == "abc"
        latency = time.time() - row["last_seen"]
        assert latency < 0.25

        stub.emit_message(topic, payload1)
        time.sleep(0.05)
        rows_dup, log_dup, _, _, _ = worker.get_state()
        assert rows_dup[0]["msg_id"] == "abc"
        assert log_dup == log

        payload2 = json.dumps({"state": "ready", "ip": "10.0.0.2", "msg_id": "abd"})
        stub.emit_message(topic, payload2)
        time.sleep(0.05)
        rows_new, log_new, _, _, _ = worker.get_state()
        assert rows_new[0]["msg_id"] == "abd"
    finally:
        worker.stop()
        worker.join(timeout=1)


def main():
    try:
        test_render_presence_table()
        test_mqtt_worker_ingest_duplicate()
        test_cli_mqtt_command_error()
        test_status_mqtt_uses_worker()
        test_mqtt_worker_queue_cmd_logs_error()
    except AssertionError:
        print("Tests failed.")
        return 1
    print("All MQTT CLI tests passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
