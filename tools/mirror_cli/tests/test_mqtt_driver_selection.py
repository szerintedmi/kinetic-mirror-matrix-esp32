from __future__ import annotations

import json

from tools.mirror_cli.mqtt_runtime import MqttWorker


class DummyPublishInfo:
    def __init__(self, rc: int = 0):
        self.rc = rc


class DummyClient:
    def __init__(self):
        self.published = []

    def publish(self, topic, payload, qos=0):  # pragma: no cover - simple stub
        self.published.append((topic, payload, qos))
        return DummyPublishInfo(rc=0)

    def loop_start(self):  # pragma: no cover - not used in tests
        return None

    def connect(self, *args, **kwargs):  # pragma: no cover - not used in tests
        return None

    def subscribe(self, *args, **kwargs):  # pragma: no cover - not used in tests
        return None

    def username_pw_set(self, *args, **kwargs):  # pragma: no cover - not used in tests
        return None

    def disconnect(self):  # pragma: no cover - not used in tests
        return None


def _make_worker() -> MqttWorker:
    return MqttWorker(client_factory=lambda: DummyClient())


def test_set_selected_device_by_index_prefers_sorted_order():
    worker = _make_worker()
    with worker._lock:
        worker._devices = {
            "devB": {"last_seen": 2, "node_state": "ready", "motors": {}},
            "devA": {"last_seen": 1, "node_state": "ready", "motors": {}},
        }

    ok, dev, total = worker.set_selected_device_by_index(2)
    assert ok is True
    assert dev == "devB"
    assert total == 2

    # selection drives resolution
    assert worker._resolve_node_id() == "devB"


def test_queue_cmd_targets_selected_device(monkeypatch):
    worker = _make_worker()
    client = DummyClient()
    with worker._lock:
        worker._devices = {
            "mac01": {"last_seen": 1, "node_state": "ready", "motors": {}},
            "mac02": {"last_seen": 2, "node_state": "ready", "motors": {}},
        }
        worker._client = client
        worker._connected = True

    # pick second device (mac02)
    worker.set_selected_device_by_index(2)

    handles = worker.queue_cmd("HELP", silent=True)
    # silent mode returns [-1] to indicate success without full tracking
    assert handles == [-1]

    assert client.published, "Command should have been published"
    topic, payload, _qos = client.published[0]
    assert topic == "devices/mac02/cmd"

    decoded = json.loads(payload)
    assert decoded.get("action") == "HELP"


def test_get_state_reports_selection_metadata():
    worker = _make_worker()
    with worker._lock:
        worker._devices = {
            "devA": {"last_seen": 1, "node_state": "ready", "motors": {}},
            "devB": {"last_seen": 2, "node_state": "ready", "motors": {}},
        }
        worker._connected = True

    # select second device
    worker.set_selected_device_by_index(2)
    _rows, _log, _err, _ts, _help, net, _seq = worker.get_state()

    assert net["selected_device"] == "devB"
    assert net["selected_index"] == 2
    assert net["device_count"] == 2


def test_out_of_range_selection_is_rejected():
    worker = _make_worker()
    with worker._lock:
        worker._devices = {"only": {"last_seen": 1, "node_state": "ready", "motors": {}}}

    ok, dev, total = worker.set_selected_device_by_index(3)
    assert ok is False
    assert dev is None
    assert total == 1
