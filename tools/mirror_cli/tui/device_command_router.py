"""Route commands to specific devices or all devices."""

from __future__ import annotations

from typing import TYPE_CHECKING, Callable, Dict, Optional, Tuple

from .device_target_parser import DeviceTarget

if TYPE_CHECKING:
    pass


class DeviceCommandRouter:
    """Routes device-targeted commands to appropriate devices.

    Handles:
    - Switching active device (/N)
    - Executing on specific device without switching (/N cmd)
    - Broadcasting to all devices (/all cmd)
    """

    def __init__(
        self,
        worker: object,
        notify_callback: Callable[[str, str, float], None],
    ) -> None:
        """Initialize router.

        Args:
            worker: Worker instance (SerialWorker or MqttWorker)
            notify_callback: Callback for notifications (message, severity, timeout)
        """
        self._worker = worker
        self._notify = notify_callback

    def execute(self, target: DeviceTarget) -> Tuple[bool, Optional[str]]:
        """Execute a device-targeted command.

        Args:
            target: Parsed device target

        Returns:
            Tuple of (handled: bool, command_sent: Optional[str])
            - handled=True means the target was processed (even if error)
            - command_sent is the actual command that was sent, or None if just switching
        """
        if target.kind == "switch":
            self._switch_device(target.device_index)
            return True, None

        if target.kind == "single":
            return self._execute_on_device(target.device_index, target.command)

        if target.kind == "all":
            return self._execute_on_all(target.command)

        return False, None

    def _switch_device(self, device_index: int) -> None:
        """Switch active device (existing /N behavior)."""
        if not hasattr(self._worker, "set_selected_device_by_index"):
            self._notify(
                "Driver selection not supported for this transport", "warning", 2.0
            )
            return

        ok, dev, total = self._worker.set_selected_device_by_index(device_index)
        if ok:
            self._notify(f"Selected driver {device_index}/{total}: {dev}", "info", 2.0)
        else:
            self._notify(f"No driver #{device_index} (available: {total})", "warning", 2.0)

    def _execute_on_device(
        self, device_index: int, command: str
    ) -> Tuple[bool, Optional[str]]:
        """Execute command on specific device without switching active device."""
        if not hasattr(self._worker, "set_selected_device_by_index"):
            self._notify(
                "Device targeting not supported for this transport", "error", 3.0
            )
            return True, None

        # Get current selection to restore later
        original_index = self._get_selected_index()

        # Temporarily switch to target device (silently)
        ok, _dev, total = self._worker.set_selected_device_by_index(
            device_index, silent=True
        )
        if not ok:
            self._notify(
                f"No driver #{device_index} (available: {total})", "error", 3.0
            )
            return True, None

        # Echo the targeted command
        if hasattr(self._worker, "append_log"):
            self._worker.append_log(f"> /{device_index} {command}")

        # Execute command (silently - we already echoed)
        self._worker.queue_cmd(command, silent=True)

        # Restore original selection (silently)
        if original_index is not None and original_index != device_index:
            self._worker.set_selected_device_by_index(original_index, silent=True)

        return True, command

    def _execute_on_all(self, command: str) -> Tuple[bool, Optional[str]]:
        """Execute command on ALL devices in parallel."""
        # Validate transport is MQTT
        transport = getattr(self._worker, "transport", "serial")
        if transport != "mqtt":
            self._notify("/all is only supported in MQTT mode", "error", 3.0)
            return True, None

        # Get device list
        summaries = self._get_device_summaries()
        if not summaries:
            self._notify("No devices available", "error", 3.0)
            return True, None

        devices = sorted(summaries.keys())
        device_count = len(devices)

        # Echo the broadcast command
        if hasattr(self._worker, "append_log"):
            self._worker.append_log(f"> /all {command} ({device_count} devices)")

        # Save current selection
        original_index = self._get_selected_index()

        # Execute on each device (parallel dispatch via MQTT, suppress per-device logs)
        for i, _device_name in enumerate(devices, start=1):
            ok, _dev, _total = self._worker.set_selected_device_by_index(i, silent=True)
            if ok:
                self._worker.queue_cmd(command, silent=True)

        # Restore original selection (silently)
        if original_index is not None:
            self._worker.set_selected_device_by_index(original_index, silent=True)

        self._notify(f"Sent to {device_count} device(s): {command}", "info", 1.5)
        return True, command

    def _get_selected_device(self) -> Optional[str]:
        """Get currently selected device name."""
        if hasattr(self._worker, "get_net_info"):
            net = self._worker.get_net_info()
            return net.get("selected_device")
        return None

    def _get_selected_index(self) -> Optional[int]:
        """Get currently selected device index (1-based)."""
        if hasattr(self._worker, "get_net_info"):
            net = self._worker.get_net_info()
            idx = net.get("selected_index")
            if idx:
                try:
                    return int(idx)
                except (ValueError, TypeError):
                    pass
        return None

    def _get_device_summaries(self) -> Dict[str, Dict]:
        """Get device summaries from worker."""
        if hasattr(self._worker, "get_device_summaries"):
            return self._worker.get_device_summaries()
        return {}


__all__ = ["DeviceCommandRouter"]
