"""Abstract protocol defining the interface for transport workers (Serial/MQTT)."""

from __future__ import annotations

from abc import ABC, abstractmethod
from typing import Dict, List, Optional, Tuple


class WorkerProtocol(ABC):
    """
    Abstract base class defining the interface for CLI transport workers.

    Both SerialWorker and MqttWorker implement this protocol, allowing
    the TUI and CLI code to work with either transport interchangeably.
    """

    @abstractmethod
    def start(self) -> None:
        """Start the worker thread."""
        ...

    @abstractmethod
    def stop(self) -> None:
        """Signal the worker to stop."""
        ...

    @abstractmethod
    def join(self, timeout: Optional[float] = None) -> None:
        """Wait for the worker thread to finish."""
        ...

    @abstractmethod
    def is_alive(self) -> bool:
        """Check if the worker thread is running."""
        ...

    @abstractmethod
    def queue_cmd(self, cmd: str, *, silent: bool = False) -> List[int]:
        """
        Queue a command to be sent to the device.

        Args:
            cmd: The command string to send
            silent: If True, suppress logging for this command

        Returns:
            List of command handles (may be empty for serial transport)
        """
        ...

    @abstractmethod
    def get_state(
        self,
    ) -> Tuple[
        List[Dict[str, str]],  # motor status rows
        List[str],  # log lines
        Optional[str],  # error message
        float,  # last update timestamp
        str,  # help text
        Dict[str, str],  # net info
        int,  # log sequence number
    ]:
        """
        Get the current state snapshot for rendering.

        Returns:
            A tuple containing:
            - Motor status rows (list of dicts with motor info)
            - Log lines (list of recent log entries)
            - Error message (None if no error)
            - Last update timestamp (time.time() value)
            - Help text (device help output)
            - Net info (network status dict)
            - Log sequence number (increments on each log entry)
        """
        ...

    @abstractmethod
    def get_net_info(self) -> Dict[str, str]:
        """
        Get network information including device ID, IP, SSID, etc.

        Returns:
            Dictionary with network status fields
        """
        ...

    @abstractmethod
    def get_thermal_state(self) -> Optional[Tuple[bool, Optional[int]]]:
        """
        Get thermal limiting state.

        Returns:
            None if unknown, or (enabled: bool, max_budget: Optional[int])
        """
        ...

    @abstractmethod
    def get_thermal_status_text(self) -> str:
        """
        Get human-readable thermal status text.

        Returns:
            String like "thermal limiting=ON (max=90s)" or empty string
        """
        ...

    @abstractmethod
    def append_log(self, line: str) -> None:
        """
        Append a line to the log.

        Args:
            line: The log line to append
        """
        ...


__all__ = ["WorkerProtocol"]
