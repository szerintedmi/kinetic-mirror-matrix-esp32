"""Tests for device_command_router module."""

from typing import Dict, List, Optional, Tuple

from tools.mirror_cli.tui.device_command_router import DeviceCommandRouter
from tools.mirror_cli.tui.device_target_parser import DeviceTarget


class MockWorker:
    """Mock worker for testing device command routing."""

    def __init__(
        self,
        *,
        transport: str = "mqtt",
        devices: Optional[Dict[str, Dict]] = None,
        selected_index: int = 1,
    ):
        self.transport = transport
        self._devices = {"dev_a": {}, "dev_b": {}, "dev_c": {}} if devices is None else devices
        self._selected_index = selected_index
        self._commands_sent: List[str] = []
        self._selection_history: List[int] = []
        self._log: List[str] = []

    def set_selected_device_by_index(
        self, index: int, *, silent: bool = False
    ) -> Tuple[bool, Optional[str], int]:
        total = len(self._devices)
        if index < 1 or index > total:
            return False, None, total
        device_names = sorted(self._devices.keys())
        self._selected_index = index
        self._selection_history.append(index)
        return True, device_names[index - 1], total

    def queue_cmd(self, cmd: str, *, silent: bool = False) -> List[int]:
        self._commands_sent.append(cmd)
        return [1]

    def append_log(self, line: str) -> None:
        self._log.append(line)

    def get_net_info(self) -> Dict[str, object]:
        return {
            "selected_index": self._selected_index,
            "selected_device": sorted(self._devices.keys())[self._selected_index - 1]
            if self._devices
            else None,
            "device_count": len(self._devices),
        }

    def get_device_summaries(self) -> Dict[str, Dict]:
        return self._devices


class TestDeviceCommandRouter:
    """Tests for DeviceCommandRouter class."""

    def setup_method(self):
        self.notifications: List[Tuple[str, str, float]] = []

        def notify(msg: str, severity: str, timeout: float) -> None:
            self.notifications.append((msg, severity, timeout))

        self.notify = notify

    # --- Switch device tests ---

    def test_switch_device_success(self):
        worker = MockWorker(selected_index=1)
        router = DeviceCommandRouter(worker, self.notify)

        target = DeviceTarget(kind="switch", device_index=2, command=None)
        handled, cmd_sent = router.execute(target)

        assert handled is True
        assert cmd_sent is None
        assert worker._selected_index == 2
        assert len(self.notifications) == 1
        assert "Selected driver 2/3" in self.notifications[0][0]

    def test_switch_device_invalid_index(self):
        worker = MockWorker(selected_index=1)
        router = DeviceCommandRouter(worker, self.notify)

        target = DeviceTarget(kind="switch", device_index=99, command=None)
        handled, cmd_sent = router.execute(target)

        assert handled is True
        assert cmd_sent is None
        assert worker._selected_index == 1  # Unchanged
        assert "No driver #99" in self.notifications[0][0]

    def test_switch_device_serial_not_supported(self):
        """Serial worker doesn't have set_selected_device_by_index."""

        class SerialLikeWorker:
            transport = "serial"

        worker = SerialLikeWorker()
        router = DeviceCommandRouter(worker, self.notify)

        target = DeviceTarget(kind="switch", device_index=1, command=None)
        handled, _cmd_sent = router.execute(target)

        assert handled is True
        assert "not supported" in self.notifications[0][0]

    # --- Single device command tests ---

    def test_single_device_command_success(self):
        worker = MockWorker(selected_index=1)
        router = DeviceCommandRouter(worker, self.notify)

        target = DeviceTarget(kind="single", device_index=2, command="STATUS")
        handled, cmd_sent = router.execute(target)

        assert handled is True
        assert cmd_sent == "STATUS"
        assert "STATUS" in worker._commands_sent
        # Should restore to original device
        assert worker._selected_index == 1
        # Should echo the targeted command
        assert "> /2 STATUS" in worker._log

    def test_single_device_command_restores_selection(self):
        worker = MockWorker(selected_index=3)
        router = DeviceCommandRouter(worker, self.notify)

        target = DeviceTarget(kind="single", device_index=1, command="MOVE:0,800")
        router.execute(target)

        # Selection history: switch to 1, then restore to 3
        assert worker._selection_history == [1, 3]
        assert worker._selected_index == 3

    def test_single_device_command_invalid_index(self):
        worker = MockWorker(selected_index=1)
        router = DeviceCommandRouter(worker, self.notify)

        target = DeviceTarget(kind="single", device_index=99, command="STATUS")
        handled, cmd_sent = router.execute(target)

        assert handled is True
        assert cmd_sent is None
        assert len(worker._commands_sent) == 0
        assert "No driver #99" in self.notifications[0][0]

    # --- Broadcast (all devices) tests ---

    def test_all_devices_success(self):
        worker = MockWorker(selected_index=1)
        router = DeviceCommandRouter(worker, self.notify)

        target = DeviceTarget(kind="all", device_index=None, command="WAKE:ALL")
        handled, cmd_sent = router.execute(target)

        assert handled is True
        assert cmd_sent == "WAKE:ALL"
        # Command sent to all 3 devices
        assert worker._commands_sent == ["WAKE:ALL", "WAKE:ALL", "WAKE:ALL"]
        # Should restore to original device
        assert worker._selected_index == 1

    def test_all_devices_restores_selection(self):
        worker = MockWorker(selected_index=2)
        router = DeviceCommandRouter(worker, self.notify)

        target = DeviceTarget(kind="all", device_index=None, command="STATUS")
        router.execute(target)

        # Should cycle through 1, 2, 3 then restore to 2
        assert worker._selection_history[-1] == 2
        assert worker._selected_index == 2

    def test_all_devices_serial_error(self):
        worker = MockWorker(transport="serial")
        router = DeviceCommandRouter(worker, self.notify)

        target = DeviceTarget(kind="all", device_index=None, command="STATUS")
        handled, cmd_sent = router.execute(target)

        assert handled is True
        assert cmd_sent is None
        assert len(worker._commands_sent) == 0
        assert "/all is only supported in MQTT mode" in self.notifications[0][0]
        assert self.notifications[0][1] == "error"

    def test_all_devices_no_devices_available(self):
        worker = MockWorker(devices={})
        router = DeviceCommandRouter(worker, self.notify)

        target = DeviceTarget(kind="all", device_index=None, command="STATUS")
        handled, cmd_sent = router.execute(target)

        assert handled is True
        assert cmd_sent is None
        assert "No devices available" in self.notifications[0][0]

    def test_all_devices_skips_failed_selection(self):
        """If a device disappears mid-broadcast, skip it."""
        worker = MockWorker()

        # Make device 2 fail selection
        original_set = worker.set_selected_device_by_index

        def flaky_set(index: int, *, silent: bool = False):
            if index == 2:
                return False, None, 3
            return original_set(index, silent=silent)

        worker.set_selected_device_by_index = flaky_set
        router = DeviceCommandRouter(worker, self.notify)

        target = DeviceTarget(kind="all", device_index=None, command="STATUS")
        router.execute(target)

        # Only 2 commands sent (device 1 and 3, not 2)
        assert len(worker._commands_sent) == 2


class TestDeviceCommandRouterEdgeCases:
    """Edge case tests for DeviceCommandRouter."""

    def setup_method(self):
        self.notifications: List[Tuple[str, str, float]] = []

        def notify(msg: str, severity: str, timeout: float) -> None:
            self.notifications.append((msg, severity, timeout))

        self.notify = notify

    def test_single_device_same_as_current(self):
        """Executing on current device should still work."""
        worker = MockWorker(selected_index=2)
        router = DeviceCommandRouter(worker, self.notify)

        target = DeviceTarget(kind="single", device_index=2, command="STATUS")
        handled, cmd_sent = router.execute(target)

        assert handled is True
        assert cmd_sent == "STATUS"
        assert "STATUS" in worker._commands_sent

    def test_worker_without_get_device_summaries(self):
        """Worker without get_device_summaries returns empty dict."""

        class MinimalWorker:
            transport = "mqtt"

            def set_selected_device_by_index(self, idx, *, silent=False):
                return True, "dev", 1

            def queue_cmd(self, cmd, *, silent=False):
                return [1]

            def get_net_info(self):
                return {"selected_index": 1}

        worker = MinimalWorker()
        router = DeviceCommandRouter(worker, self.notify)

        target = DeviceTarget(kind="all", device_index=None, command="STATUS")
        handled, cmd_sent = router.execute(target)

        assert handled is True
        assert cmd_sent is None
        assert "No devices available" in self.notifications[0][0]

    def test_worker_without_get_net_info(self):
        """Worker without get_net_info should still work for single device."""

        class MinimalWorker:
            transport = "mqtt"

            def __init__(self):
                self._commands: List[str] = []

            def set_selected_device_by_index(self, idx, *, silent=False):
                return True, "dev", 3

            def queue_cmd(self, cmd, *, silent=False):
                self._commands.append(cmd)
                return [1]

            def get_device_summaries(self):
                return {"a": {}, "b": {}, "c": {}}

        worker = MinimalWorker()
        router = DeviceCommandRouter(worker, self.notify)

        target = DeviceTarget(kind="single", device_index=2, command="STATUS")
        handled, cmd_sent = router.execute(target)

        assert handled is True
        assert cmd_sent == "STATUS"
