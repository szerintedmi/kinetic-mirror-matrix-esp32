"""Device view modal for managing device visibility and displaying device info."""

from __future__ import annotations

from typing import TYPE_CHECKING, Callable, ClassVar, Dict, List, Optional, Set, Tuple

if TYPE_CHECKING:
    pass


def _format_age(age_s: float) -> str:
    """Format age in seconds to human-readable string."""
    if age_s is None or age_s == float("inf"):
        return "?"
    if age_s < 60:
        return f"{age_s:.1f}s"
    elif age_s < 3600:
        return f"{age_s / 60:.1f}m"
    else:
        return f"{age_s / 3600:.1f}h"


def _format_uptime(uptime_s: int) -> str:
    """Format uptime in seconds to human-readable string."""
    if not uptime_s:
        return "-"
    if uptime_s < 60:
        return f"{uptime_s}s"
    elif uptime_s < 3600:
        return f"{uptime_s // 60}m{uptime_s % 60}s"
    elif uptime_s < 86400:
        hours = uptime_s // 3600
        mins = (uptime_s % 3600) // 60
        return f"{hours}h{mins}m"
    else:
        days = uptime_s // 86400
        hours = (uptime_s % 86400) // 3600
        return f"{days}d{hours}h"


def _format_thermal(thermal_state: Optional[Tuple[bool, Optional[int]]]) -> str:
    """Format thermal state tuple to string."""
    if not thermal_state or not isinstance(thermal_state, tuple):
        return "-"
    enabled, max_budget = thermal_state
    if enabled:
        return "ON" if max_budget is None else f"ON({max_budget}s)"
    return "OFF"


def _color(value: str, swatch: Optional[str]) -> str:
    """Wrap value in Rich color markup."""
    if not value or not swatch:
        return value or "-"
    return f"[{swatch}]{value}[/]"


def create_device_view_screen(worker: object, notify_callback: Callable[[str, str, float], None]):
    """Factory to create DeviceViewScreen with lazy Textual imports.

    Returns the screen class and an instance, allowing the TUI to push it.
    """
    # Lazy import Textual to avoid import errors if not installed
    try:
        from textual.app import ComposeResult
        from textual.binding import Binding
        from textual.containers import Vertical
        from textual.screen import ModalScreen
        from textual.widgets import DataTable, Static
    except ImportError as e:
        raise RuntimeError(f"Textual is required for device view: {e}") from e

    # Device table columns: (key, label, width)
    DEVICE_COLS: List[Tuple[str, str, int]] = [
        ("sel", "✓", 3),
        ("idx", "#", 3),
        ("mac", "MAC", 14),
        ("ip", "IP", 15),
        ("state", "State", 8),
        ("age", "Age", 6),
        ("motors", "Mot", 4),
        ("thermal", "Therm", 6),
        ("fw", "FW", 8),
        ("uptime", "Uptime", 8),
    ]

    class DeviceViewScreen(ModalScreen[Optional[Set[str]]]):
        """Modal screen for viewing and filtering devices.

        Returns:
            Set[str] of selected device MACs on Apply, or None on Cancel.
        """

        CSS = """
        DeviceViewScreen > Vertical {
            width: 95%;
            height: 90%;
            border: round $accent;
            background: $panel;
            margin: 1 2;
        }
        #device-header {
            padding: 0 1;
            height: 1;
            content-align: center middle;
            text-style: bold;
        }
        #device-table-container {
            height: 1fr;
            padding: 0 1;
        }
        DataTable {
            height: 100%;
        }
        #device-help {
            height: 1;
            padding: 0 1;
            color: $text-muted;
        }
        """

        BINDINGS: ClassVar = [
            Binding("escape", "cancel", "Cancel"),
            Binding("enter", "apply", "Apply"),
            Binding("space", "toggle_row", "Toggle", show=False),
            Binding("a", "select_all", "Select All", show=False),
            Binding("n", "select_none", "Select None", show=False),
            Binding("r", "refresh", "Refresh", show=True),
        ]

        def __init__(self) -> None:
            super().__init__()
            self._worker = worker
            self._notify = notify_callback
            # Track which devices are selected (checked)
            self._selected: Set[str] = set()
            # Ordered list of device MACs for index lookup
            self._devices_ordered: List[str] = []
            # Cache device details
            self._device_details: Dict[str, Dict[str, object]] = {}
            # Track if initial auto-select has been done
            self._initial_select_done: bool = False

        def compose(self) -> ComposeResult:
            yield Vertical(
                Static("Device Manager", id="device-header"),
                Vertical(
                    DataTable(id="device-table", cursor_type="row"),
                    id="device-table-container",
                ),
                Static(
                    "Space=Toggle  a=All  n=None  r=Refresh  Enter=Apply  Esc=Cancel",
                    id="device-help",
                ),
            )

        def on_mount(self) -> None:
            # Initialize table columns
            table = self.query_one("#device-table", DataTable)
            table.clear(columns=True)
            for key, label, width in DEVICE_COLS:
                table.add_column(label, width=width, key=key)
            table.zebra_stripes = True
            table.cursor_type = "row"
            try:
                table.show_row_labels = False
            except Exception:
                pass

            # Load initial selection from worker
            # Empty worker filter means "all visible", so we need to get the actual device list
            if hasattr(self._worker, "get_device_details"):
                details = self._worker.get_device_details()
                all_devices = set(details.keys())

                if hasattr(self._worker, "get_visible_devices"):
                    current = self._worker.get_visible_devices()
                    if current:
                        # Explicit filter - use it (filter out sentinel)
                        self._selected = set(current) - {"__none__"}
                        self._initial_select_done = True
                    else:
                        # Empty means all visible - select all current devices
                        self._selected = set(all_devices)
                        self._initial_select_done = True
                else:
                    self._selected = set(all_devices)
                    self._initial_select_done = True

            # Trigger GET ALL for all devices on open
            self._trigger_refresh_all()

            # Start refresh timer
            self.set_interval(0.5, self._refresh_table)
            self._refresh_table()

        def _trigger_refresh_all(self) -> None:
            """Send GET ALL to all devices to refresh firmware/uptime info."""
            if not hasattr(self._worker, "get_device_summaries"):
                return

            summaries = self._worker.get_device_summaries()
            if not summaries:
                return

            devices_ordered = sorted(summaries.keys())

            # Save original device by MAC (not index) to handle device list changes
            original_device_mac: Optional[str] = None
            if hasattr(self._worker, "get_net_info"):
                net = self._worker.get_net_info()
                original_device_mac = net.get("selected_device")

            # Send GET ALL to each device
            for i, _mac in enumerate(devices_ordered, start=1):
                if hasattr(self._worker, "set_selected_device_by_index"):
                    ok, _dev, _total = self._worker.set_selected_device_by_index(i, silent=True)
                    if ok and hasattr(self._worker, "queue_cmd"):
                        self._worker.queue_cmd("GET ALL", silent=True)

            # Restore original selection by MAC (handles device list changes)
            if original_device_mac and hasattr(self._worker, "set_selected_device_by_index"):
                # Get current device list and find the original device's new index
                current_summaries = self._worker.get_device_summaries()
                current_devices = sorted(current_summaries.keys())
                if original_device_mac in current_devices:
                    restore_index = current_devices.index(original_device_mac) + 1
                    self._worker.set_selected_device_by_index(restore_index, silent=True)

            self._notify(f"Refreshing {len(devices_ordered)} device(s)...", "info", 2.0)

        def _refresh_table(self) -> None:
            """Refresh device table with current data."""
            if not hasattr(self._worker, "get_device_details"):
                return

            self._device_details = self._worker.get_device_details()
            self._devices_ordered = sorted(self._device_details.keys())

            # Auto-select all devices only on initial load (not after user clears selection)
            if not self._initial_select_done and self._devices_ordered:
                self._selected = set(self._devices_ordered)
                self._initial_select_done = True

            table = self.query_one("#device-table", DataTable)

            # Build row data
            rows: List[Tuple[str, List[str]]] = []
            for idx, mac in enumerate(self._devices_ordered, start=1):
                details = self._device_details.get(mac, {})
                is_selected = mac in self._selected

                # Format checkbox (backslash escapes brackets in Rich)
                checkbox = r"\[x]" if is_selected else r"\[ ]"

                # Format state with color
                state = str(details.get("node_state", "") or "")
                state_lower = state.lower()
                if state_lower == "ready":
                    state_fmt = _color(state, "green")
                elif state_lower in ("offline", "error", "disconnected"):
                    state_fmt = _color(state, "red")
                else:
                    state_fmt = _color(state, "yellow") if state else "-"

                # Format age with color
                age_s = float(details.get("age_s", 0) or 0)
                age_str = _format_age(age_s)
                if age_s > 5.0:
                    age_fmt = _color(age_str, "red")
                elif age_s > 2.0:
                    age_fmt = _color(age_str, "yellow")
                else:
                    age_fmt = age_str

                # Format thermal
                thermal = details.get("thermal_state")
                thermal_fmt = _format_thermal(thermal)

                # Format uptime
                uptime_s = int(details.get("uptime_s", 0) or 0)
                uptime_fmt = _format_uptime(uptime_s)

                # MAC short form (last 6 chars)
                mac_short = mac[-12:] if len(mac) > 12 else mac

                row_data = [
                    checkbox,
                    str(idx),
                    mac_short,
                    str(details.get("ip", "") or "-"),
                    state_fmt,
                    age_fmt,
                    str(details.get("motor_count", 0)),
                    thermal_fmt,
                    str(details.get("firmware_version", "") or "-"),
                    uptime_fmt,
                ]
                rows.append((mac, row_data))

            # Update table - always rebuild to ensure checkbox state is correct
            try:
                # Save cursor position
                cursor_row = table.cursor_row if table.row_count > 0 else 0
                table.clear()
                for mac, row_data in rows:
                    table.add_row(*row_data, key=mac)
                # Restore cursor position
                if rows and cursor_row < len(rows):
                    table.move_cursor(row=cursor_row)
            except Exception:
                pass

        def _get_cursor_device(self) -> Optional[str]:
            """Get the device MAC at the current cursor position."""
            table = self.query_one("#device-table", DataTable)
            try:
                row_idx = table.cursor_row
                if 0 <= row_idx < len(self._devices_ordered):
                    return self._devices_ordered[row_idx]
            except Exception:
                pass
            return None

        def action_toggle_row(self) -> None:
            """Toggle selection for the current row."""
            mac = self._get_cursor_device()
            if mac:
                if mac in self._selected:
                    self._selected.discard(mac)
                else:
                    self._selected.add(mac)
                self._refresh_table()

        def action_select_all(self) -> None:
            """Select all devices."""
            self._selected = set(self._devices_ordered)
            self._refresh_table()

        def action_select_none(self) -> None:
            """Deselect all devices."""
            self._selected.clear()
            self._refresh_table()

        def action_refresh(self) -> None:
            """Refresh device data via GET ALL."""
            self._trigger_refresh_all()

        def action_apply(self) -> None:
            """Apply selection and close."""
            # Worker semantics: empty set = all visible
            # If all current devices selected, return empty set
            if self._selected == set(self._devices_ordered):
                self.dismiss(set())  # All visible
            elif not self._selected:
                # None selected - return a sentinel that won't match any device
                # This effectively hides all motors (empty list)
                self.dismiss({"__none__"})
            else:
                self.dismiss(set(self._selected))

        def action_cancel(self) -> None:
            """Cancel and close without changes."""
            self.dismiss(None)

        def on_key(self, event) -> None:
            """Handle keys that DataTable would otherwise capture."""
            key = event.key
            if key == "space":
                self.action_toggle_row()
                event.stop()
                event.prevent_default()
            elif key == "a":
                self.action_select_all()
                event.stop()
                event.prevent_default()
            elif key == "n":
                self.action_select_none()
                event.stop()
                event.prevent_default()
            elif key == "enter":
                self.action_apply()
                event.stop()
                event.prevent_default()

    return DeviceViewScreen


__all__ = ["create_device_view_screen"]
