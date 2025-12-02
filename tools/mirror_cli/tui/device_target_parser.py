"""Parse device targeting syntax (/N cmd, /all cmd, /N)."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Literal, Optional


@dataclass
class DeviceTarget:
    """Parsed device targeting command."""

    kind: Literal["switch", "single", "all"]
    """
    - "switch": /N alone - switch active device
    - "single": /N cmd - execute on device N without switching
    - "all": /all cmd - execute on all devices
    """
    device_index: Optional[int]  # 1-based index for "switch" and "single"
    command: Optional[str]  # Command text for "single" and "all"


def parse_device_target(input_text: str) -> Optional[DeviceTarget]:
    """Parse device targeting syntax from input text.

    Supported syntax:
    - /N       -> Switch to device N (kind="switch")
    - /N cmd   -> Execute cmd on device N without switching (kind="single")
    - /all cmd -> Execute cmd on all devices (kind="all")

    Returns None if input is not a device targeting command.
    """
    stripped = input_text.strip()
    if not stripped.startswith("/"):
        return None

    # Remove leading slash
    rest = stripped[1:]
    if not rest:
        return None

    # Split on first space to separate selector from command
    parts = rest.split(None, 1)
    if not parts:
        return None

    selector = parts[0]
    command = parts[1].strip() if len(parts) > 1 else None

    # /all cmd - broadcast to all devices
    if selector.lower() == "all":
        if not command:
            # /all alone is invalid - need a command
            return None
        return DeviceTarget(kind="all", device_index=None, command=command)

    # /N or /N cmd - device index targeting
    if not selector.isdigit():
        return None

    index = int(selector)
    if index < 1:
        return None

    if command:
        # /N cmd - execute on device N without switching
        return DeviceTarget(kind="single", device_index=index, command=command)
    else:
        # /N alone - switch to device N
        return DeviceTarget(kind="switch", device_index=index, command=None)


__all__ = ["DeviceTarget", "parse_device_target"]
