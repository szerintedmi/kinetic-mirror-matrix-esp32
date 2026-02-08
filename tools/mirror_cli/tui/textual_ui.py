from __future__ import annotations

from typing import ClassVar, Dict, List, Optional, Tuple

from .base import BaseUI
from .device_command_router import DeviceCommandRouter
from .device_target_parser import parse_device_target
from .device_view import create_device_view_screen


class TextualUI(BaseUI):
    def run(self) -> int:  # pragma: no cover (interactive)
        try:
            # Textual is built on Rich
            from textual.app import App, ComposeResult, SystemCommand
            from textual.binding import Binding
            from textual.containers import (
                Container,
                Horizontal,
                Vertical,
                VerticalScroll,
            )
            from textual.events import Key
            from textual.reactive import var
            from textual.screen import ModalScreen
            from textual.widgets import (
                DataTable,
                Footer,
                Header,
                Input,
                Label,
                RichLog,
                Static,
            )

        except Exception as e:
            print("textual is required. Install with: pip install textual rich")
            print(f"Import error: {e}")
            return 2

        worker = self.worker

        # Columns mapping mirroring render_table() layout
        STATUS_COLS: List[Tuple[str, str, int]] = [
            ("dev", "dev", 4),  # Device sequence number (MQTT multi-device)
            ("id", "id", 4),
            ("pos", "pos", 8),
            ("moving", "moving", 8),
            ("awake", "awake", 8),
            ("homed", "homed", 8),
            ("steps_since_home", "steps_since_home", 20),
            ("budget_s", "budget_s", 12),
            ("ttfc_s", "ttfc_s", 10),
            # Timing summary from STATUS (mirrors GET LAST_OP_TIMING)
            ("est_ms", "est_ms", 10),
            ("started_ms", "started_ms", 12),
            ("actual_ms", "actual_ms", 12),
        ]

        transport = getattr(worker, "transport", "serial")
        columns_from_worker: Optional[List[Tuple[str, str, int]]] = None
        if hasattr(worker, "get_table_columns"):
            try:
                raw_cols = worker.get_table_columns()
                columns_from_worker = [
                    (
                        str(col[0]),
                        str(col[1] if len(col) > 1 else col[0]),
                        int(col[2] if len(col) > 2 else 12),
                    )
                    for col in raw_cols
                ]
            except Exception:
                columns_from_worker = None
        TABLE_COLS: List[Tuple[str, str, int]] = (
            columns_from_worker if columns_from_worker else STATUS_COLS
        )

        def _color(value: str, swatch: Optional[str]) -> str:
            if not value:
                return "-"
            if not swatch:
                return value
            return f"[{swatch}]{value}[/]"

        def _fmt_age(age_val: float) -> str:
            if age_val is None or age_val == float("inf"):
                return "age=unknown"
            color = None
            if age_val > 10.0:
                color = "red"
            elif age_val > 5.0:
                color = "yellow"
            age_text = f"{age_val:.1f}s"
            return f"age={_color(age_text, color)}"

        base_transport = transport.lower()

        def _render_status_line(rows, net, last_ts) -> str:
            import time as _time

            parts: List[str] = []
            transport_mode = (net.get("transport") or base_transport or "serial").lower()
            if transport_mode == "mqtt":
                host = net.get("host") or "-"
                port = net.get("port") or "-"
                parts.append(f"mqtt://{host}:{port}")
            else:
                port_val = getattr(worker, "port", "-")
                parts.append(f"serial:{port_val}")

            summaries: Dict[str, Dict[str, object]] = {}
            if hasattr(worker, "get_device_summaries"):
                try:
                    summaries = worker.get_device_summaries()  # type: ignore[attr-defined]
                except Exception:
                    summaries = {}

            if not summaries:
                for r in rows or []:
                    dev = str(r.get("device", "") or "")
                    if not dev:
                        continue
                    summaries.setdefault(
                        dev,
                        {
                            "node_state": r.get("node_state", ""),
                            "ip": r.get("ip", ""),
                            "age_s": float(r.get("age_s", 0.0) or 0.0),
                        },
                    )

            device_count = 0
            try:
                device_count = int(net.get("device_count") or 0)
            except Exception:
                device_count = 0

            selected_device = net.get("selected_device") or net.get("device") or ""
            if (not selected_device) and summaries:
                selected_device = sorted(summaries.keys())[0]

            selected_index = 0
            try:
                selected_index = int(net.get("selected_index") or 0)
            except Exception:
                selected_index = 0
            if selected_index == 0 and selected_device:
                order = sorted(summaries.keys())
                if selected_device in order:
                    selected_index = order.index(selected_device) + 1
                    device_count = device_count or len(order)

            device_label = selected_device or "-"
            if device_count > 0 and selected_index > 0:
                parts.append(f"device {selected_index}/{device_count}={device_label}")
            else:
                parts.append(f"device={device_label}")

            # Determine primary state/ip/age from summaries or net info
            primary = {}
            if selected_device and selected_device in summaries:
                primary = summaries[selected_device]
            elif summaries:
                primary = summaries[sorted(summaries.keys())[0]]

            state_val = str(primary.get("node_state", "") or net.get("node_state") or "")
            ip_val = str(primary.get("ip", "") or net.get("ip", "") or "-")
            parts.append(f"ip={ip_val}")

            state_color = None
            if state_val:
                lower = state_val.lower()
                if lower == "ready":
                    state_color = "green"
                elif lower in {"offline", "error", "disconnected"}:
                    state_color = "red"
                else:
                    state_color = "yellow"
            parts.append(f"state={_color(state_val or '-', state_color)}")

            wifi_state = (net.get("state") or "").upper()
            ssid = net.get("ssid") or ""
            ssid_color = None
            if wifi_state == "CONNECTED":
                ssid_color = "green"
            elif wifi_state == "AP_ACTIVE":
                ssid_color = "yellow"
            elif wifi_state:
                ssid_color = "red"
            parts.append(f"SSID: {_color(ssid or '-', ssid_color)}")

            thermal = None
            if hasattr(worker, "get_thermal_state"):
                try:
                    thermal = worker.get_thermal_state()
                except Exception:
                    thermal = None
            if isinstance(thermal, tuple) and len(thermal) == 2:
                enabled, _ = thermal
                color = "green" if enabled else "red"
                text = "ON" if enabled else "OFF"
                parts.append(f"thermal={_color(text, color)}")
            else:
                parts.append("thermal=-")

            # Microstep state
            microstep = None
            if hasattr(worker, "get_microstep_state"):
                try:
                    microstep = worker.get_microstep_state()
                except Exception:
                    microstep = None
            if microstep:
                parts.append(f"step={microstep}")
            else:
                parts.append("step=-")

            if transport_mode == "mqtt" and summaries:
                ages = [
                    float(info.get("age_s", float("inf")) or float("inf"))
                    for info in summaries.values()
                ]
                min_age = min(ages) if ages else float("inf")
                parts.append(_fmt_age(min_age))
            else:
                age = None
                if last_ts:
                    try:
                        age = max(0.0, _time.time() - last_ts)
                    except Exception:
                        age = None
                parts.append(_fmt_age(age if age is not None else float("inf")))

            return "  ".join(parts)

        class HelpOverlay(ModalScreen[None]):
            """Modal help overlay with keys and device HELP text."""

            CSS = """
            HelpOverlay > Vertical {
                width: 90%;
                height: 90%;
                border: round $accent;
                background: $panel;
                margin: 2 4;
            }
            #help-header {
                padding: 1 2;
                content-align: left middle;
            }
            #help-body {
                height: 1fr;
            }
            #help-footer {
                padding: 1 2;
                color: $text-muted;
            }
            #help-left, #help-right {
                height: 1fr;
            }
            #help-left {
                padding: 0 1 0 2;
            }
            #help-right {
                padding: 0 2 0 1;
            }
            """

            def __init__(self) -> None:
                super().__init__()
                self._last_help_text: str = ""

            def compose(self) -> ComposeResult:  # type: ignore[override]
                yield Vertical(
                    Static("Interactive Terminal Help", id="help-header"),
                    Horizontal(
                        VerticalScroll(
                            Static(self._render_left(), id="help-left"),
                            id="help-left-scroll",
                        ),
                        VerticalScroll(
                            Static("Loading device help...", id="help-right"),
                            id="help-right-scroll",
                        ),
                        id="help-body",
                    ),
                    Static("Press ESC to close help", id="help-footer"),
                )

            def on_show(self) -> None:
                # Request HELP from device if not present
                state = worker.get_state()
                help_text = state[4] if isinstance(state, (list, tuple)) and len(state) >= 5 else ""
                if not help_text:
                    worker.queue_cmd("HELP")
                self.set_interval(0.25, self._refresh_help, pause=False, name="help-refresh")

            def _render_left(self) -> str:
                left = [
                    "Keys:",
                    "  Ctrl+Q = quit",
                    "  ? = help",
                    "  Ctrl+L = select controller (MQTT only)",
                    "  d = toggle theme",
                    "  c = clear log",
                    "  PageUp/PageDown = scroll log",
                    "  Up/Down = command history",
                    "  Enter = send command (e.g., WAKE:ALL, MOVE:0,1200)",
                    "",
                    "Common commands:",
                    "  MOVE:<id|ALL>,<abs_steps>[,spd][,acc][,overshoot][,dith_amp][,dith_cyc]",
                    "  HOME:<id|ALL>[,<overshoot>][,<backoff>][,<spd>][,<acc>][,<full_range>]",
                    "  GET/SET SPEED, ACCEL, DECEL, MOVE_OVERSHOOT,",
                    "        DITHER_AMPLITUDE, DITHER_CYCLES, DITHER_MIN_AMPLITUDE",
                    "  GET LAST_OP_TIMING[:<id|ALL>]",
                    "",
                    "Device targeting (MQTT only):",
                    "  /<n>        Switch to device #n (e.g., /1)",
                    "  /<n> <cmd>  Send cmd to device #n without switching",
                    "  /all <cmd>  Send cmd to ALL devices in parallel",
                    "",
                    "Controller selector (Ctrl+L, MQTT only):",
                    "  View all controllers with status and metadata",
                    "  Select active controller for commands and display",
                    "  Enter=Select  Esc=Cancel",
                    "",
                    "Copy/paste:",
                    "  Tip: Hold Shift to select text in many terminals",
                    "",
                    "Status columns:",
                    "  dev=device #, id, pos, moving, awake, homed",
                    "  steps_since_home, budget_s, ttfc_s",
                    "  est_ms, started_ms, actual_ms",
                ]
                return "\n".join(left)

            def _refresh_help(self) -> None:
                state = worker.get_state()
                help_text = state[4] if isinstance(state, (list, tuple)) and len(state) >= 5 else ""
                t = help_text or "(no data)"
                if t != self._last_help_text:
                    # Trim any leading HELP echo
                    lines = t.splitlines()
                    if lines and lines[0].strip().upper() == "HELP":
                        t = "\n".join(lines[1:])
                    self.query_one("#help-right", Static).update(t)
                    self._last_help_text = t

            BINDINGS: ClassVar = [
                ("escape", "dismiss", "Close"),
            ]

            def action_dismiss(self) -> None:
                self.dismiss(None)

        class SerialApp(App):
            TITLE = "Motor Control"
            # Maximum motors to display in main table (rest hidden, use Ctrl+L to filter)
            MAX_MOTOR_ROWS = 16

            CSS = """
            Screen { background: $surface; }
            Header { height: 1; }
            Footer { height: 1; }
            #main { layout: vertical; }
            #status_bar { height: 1; content-align: left middle; padding: 0 1; }
            #table_panel { height: auto; max-height: 18; background: $panel; }
            /* Revert to accent header + normal intensity */
            DataTable { background: $panel; color: $text; height: auto; }
            DataTable .datatable--header { background: $accent; color: $text; text-style: bold; }
            DataTable .datatable--cursor { background: $accent-darken-1; }
            #table_hint { height: 1; padding: 0 1; color: $text-muted; }
            #log_panel { height: 1fr; }
            #input_row { height: 1; layout: horizontal; background: $boost; }
            /* Ensure the "> " prompt is always visible: give it room and no side padding */
            #prompt { width: auto; min-width: 2; content-align: right middle; padding: 0 1 0 0; color: $text-muted; }
            #cmd_input { width: 1fr; background: $panel; color: $text; border: none; }
            """

            BINDINGS: ClassVar = [
                Binding("Ctrl+x", "quit", "Quit", show=False),
                Binding("Ctrl+q", "quit", "Quit", show=True),
                Binding("Ctrl+i", "help", "Help"),
                Binding("ctrl+l", "toggle_device_view", "Devices", show=True),
                Binding("pageup", "scroll_log_up", "Log Up", show=False),
                Binding("pagedown", "scroll_log_down", "Log Down", show=False),
            ]

            dark = var(True)

            def __init__(self) -> None:
                super().__init__()
                self._last_log_len = 0
                self._last_log_seq: Optional[int] = None
                self._cols_ready = False
                self._hist: List[str] = []
                self._hist_idx: Optional[int] = None
                # Buffer to remember the user's in-progress input when
                # they first enter history navigation with Up/Down.
                # When they navigate back past the newest history item,
                # we restore this text instead of clearing the field.
                self._hist_buffer: Optional[str] = None
                self._columns: List[Tuple[str, str, int]] = list(TABLE_COLS)
                self._transport = base_transport
                # Cache for table data, hint, and prompt to avoid unnecessary updates
                self._last_table_data: List[List[str]] = []
                self._last_hint: str = ""
                self._last_prompt: str = ""

                # Device command router for /N cmd and /all cmd syntax
                def _notify_wrapper(msg: str, severity: str, timeout: float) -> None:
                    # Map severity to Textual's expected values
                    sev = "information" if severity == "info" else severity
                    self.notify(msg, severity=sev, timeout=timeout)

                self._device_router = DeviceCommandRouter(worker, _notify_wrapper)

            def compose(self) -> ComposeResult:  # type: ignore[override]
                yield Header(show_clock=False)
                with Container(id="main"):
                    yield Static("", id="status_bar")
                    with Container(id="table_panel"):
                        yield DataTable(id="status_table")
                        yield Static("", id="table_hint")
                    with Container(id="log_panel"):
                        yield RichLog(id="log", highlight=True, wrap=True)
                    with Container(id="input_row"):
                        yield Label("> ", id="prompt")
                        yield Input(placeholder="Enter device command", id="cmd_input")
                yield Footer()

            def on_mount(self) -> None:
                self.theme = "textual-dark"
                # Prepare table columns once
                table = self.query_one("#status_table", DataTable)
                if not self._cols_ready:
                    table.clear(columns=True)
                    for _key, label, width in self._columns:
                        table.add_column(label, width=width)
                    table.cursor_type = "row"
                    table.zebra_stripes = True
                    # Compact header and hide row labels for vertical space
                    try:
                        table.header_height = 1
                        table.show_row_labels = False
                        table.show_cursor = False
                    except Exception:
                        pass
                    self._cols_ready = True
                # Keep input focused; make panels non-focusable to ease copy/paste
                try:
                    table.can_focus = False
                except Exception:
                    pass
                try:
                    logw = self.query_one("#log", RichLog)
                    logw.can_focus = False
                except Exception:
                    pass
                # Focus input and set refresh interval based on worker polling
                try:
                    period = getattr(worker, "period", 0.5) or 0.5
                    hz = max(0.5, 1.0 / max(0.05, float(period)))
                except Exception:
                    hz = 2.0
                self.set_interval(1.0 / hz, self._refresh)
                cmd_input = self.query_one("#cmd_input", Input)
                if self._transport == "mqtt":
                    cmd_input.placeholder = "MQTT presence mode (commands disabled)"
                cmd_input.focus()

            # Add Help in the Command Palette (Ctrl+P)
            def get_system_commands(self, screen):  # type: ignore[override]
                # Put Help at the top of the command palette
                yield SystemCommand("Help", "Open help overlay", self.action_help)
                # Include default commands (Quit, Theme, etc.)
                yield from super().get_system_commands(screen)

            def action_toggle_dark(self) -> None:
                self.dark = not self.dark
                self.theme = "textual-dark" if self.dark else "textual-light"

            def action_help(self) -> None:
                self.push_screen(HelpOverlay())

            def action_clear_log(self) -> None:
                self.query_one("#log", RichLog).clear()
                self._last_log_len = 0
                self._last_log_seq = None

            def action_scroll_log_up(self) -> None:
                self.query_one("#log", RichLog).scroll_page_up()

            def action_scroll_log_down(self) -> None:
                self.query_one("#log", RichLog).scroll_page_down()

            def action_toggle_device_view(self) -> None:
                """Open device manager modal (Tab key). MQTT only."""
                if self._transport != "mqtt":
                    self.notify(
                        "Device view only available in MQTT mode", severity="warning", timeout=2.0
                    )
                    return

                if not hasattr(worker, "get_device_summaries"):
                    self.notify(
                        "Device view not supported for this worker", severity="warning", timeout=2.0
                    )
                    return

                summaries = worker.get_device_summaries()
                if not summaries:
                    self.notify("No devices detected", severity="warning", timeout=2.0)
                    return

                def _notify_wrapper(msg: str, severity: str, timeout: float) -> None:
                    sev = "information" if severity == "info" else severity
                    self.notify(msg, severity=sev, timeout=timeout)

                def _on_dismiss(selected_mac: Optional[str]) -> None:
                    if selected_mac is not None:
                        # Find index of selected MAC and switch to it
                        devices = sorted(summaries.keys())
                        if selected_mac in devices:
                            index = devices.index(selected_mac) + 1
                            worker.set_selected_device_by_index(index)
                            # Short MAC for display
                            mac_short = selected_mac[-6:] if len(selected_mac) > 6 else selected_mac
                            self.notify(f"Selected controller {index}: {mac_short}", timeout=2.0)

                try:
                    DeviceViewScreen = create_device_view_screen(worker, _notify_wrapper)
                    self.push_screen(DeviceViewScreen(), _on_dismiss)
                except Exception as e:
                    self.notify(f"Error opening device view: {e}", severity="error", timeout=3.0)

            def on_input_submitted(self, event: Input.Submitted) -> None:
                text = (event.value or "").strip()
                if not text:
                    return

                # Check for device targeting syntax: /N, /N cmd, /all cmd
                target = parse_device_target(text)
                if target is not None:
                    handled, cmd_sent = self._device_router.execute(target)
                    if handled:
                        # Add to history if a command was actually sent
                        if cmd_sent and ((not self._hist) or self._hist[-1] != text):
                            self._hist.append(text)
                        self._hist_idx = None
                        self._hist_buffer = None
                        event.input.value = ""
                        return

                # Regular device command (no device targeting prefix)
                worker.queue_cmd(text)
                if (not self._hist) or self._hist[-1] != text:
                    self._hist.append(text)
                self._hist_idx = None
                self._hist_buffer = None
                self.notify(f"Sent: {text}", timeout=1.5)
                event.input.value = ""

            def on_key(self, event: Key) -> None:
                # Only intercept keys for history when the command input has focus
                try:
                    cmd_input = self.query_one("#cmd_input", Input)
                except Exception:
                    return
                if not getattr(cmd_input, "has_focus", False):
                    return
                if event.key == "up":
                    if self._hist:
                        if self._hist_idx is None:
                            # Remember current in-progress edit before entering history
                            self._hist_buffer = cmd_input.value
                            self._hist_idx = len(self._hist) - 1
                        elif self._hist_idx > 0:
                            self._hist_idx -= 1
                        cmd_input.value = self._hist[self._hist_idx]
                        try:
                            cmd_input.cursor_position = len(cmd_input.value or "")
                        except Exception:
                            pass
                        event.stop()
                elif event.key == "down":
                    if self._hist and self._hist_idx is not None:
                        if self._hist_idx < len(self._hist) - 1:
                            self._hist_idx += 1
                            cmd_input.value = self._hist[self._hist_idx]
                            try:
                                cmd_input.cursor_position = len(cmd_input.value or "")
                            except Exception:
                                pass
                        else:
                            self._hist_idx = None
                            # Restore original in-progress text if any
                            cmd_input.value = self._hist_buffer or ""
                            self._hist_buffer = None
                        try:
                            cmd_input.cursor_position = len(cmd_input.value or "")
                        except Exception:
                            pass
                        event.stop()
                elif event.key == "?" or getattr(event, "character", None) == "?":
                    # Single universal binding for help
                    self.action_help()
                    event.stop()

            def _refresh(self) -> None:
                state = worker.get_state()
                rows, log, err, last_ts, _help_text = state[:5]
                net: Dict[str, str] = {}
                log_seq: Optional[int] = None
                if len(state) >= 6 and isinstance(state[5], dict):
                    net = state[5] or {}
                elif hasattr(worker, "get_net_info"):
                    try:
                        net = worker.get_net_info() or {}
                    except Exception:
                        net = {}
                if len(state) >= 7:
                    try:
                        log_seq = int(state[6])
                    except (TypeError, ValueError):
                        log_seq = None
                if not net and hasattr(worker, "get_net_info"):
                    try:
                        net = worker.get_net_info() or {}
                    except Exception:
                        net = {}

                # Update prompt with controller number
                selected_index = 0
                try:
                    selected_index = int(net.get("selected_index") or 0)
                except Exception:
                    selected_index = 0

                if selected_index > 0 and self._transport == "mqtt":
                    new_prompt = f"{selected_index} > "
                else:
                    new_prompt = "> "

                if new_prompt != self._last_prompt:
                    try:
                        self.query_one("#prompt", Label).update(new_prompt)
                        self._last_prompt = new_prompt
                    except Exception:
                        pass  # Widget not ready yet

                status_text = _render_status_line(rows, net, last_ts)
                if err:
                    status_text += f"  error={err}"
                self.query_one("#status_bar", Static).update(status_text.strip())

                # Update table - worker already filters to selected device
                table = self.query_one("#status_table", DataTable)
                try:
                    # Build device index lookup for "dev" column
                    devices_ordered: List[str] = []
                    if hasattr(worker, "get_device_summaries"):
                        try:
                            summaries = worker.get_device_summaries()
                            devices_ordered = sorted(summaries.keys())
                        except Exception:
                            pass
                    device_idx_map: Dict[str, str] = {
                        mac: str(i + 1) for i, mac in enumerate(devices_ordered)
                    }
                    total_devices = len(devices_ordered)

                    # Build table data - worker already filters to selected device
                    data: List[List[str]] = []
                    for r in rows or []:
                        device_mac = str(r.get("device", "") or "")
                        row: List[str] = []
                        for key, _label, _w in self._columns:
                            if key == "dev":
                                # Device sequence number
                                val = device_idx_map.get(device_mac, "-")
                            elif key in ("moving", "awake", "homed"):
                                raw_val = r.get(key, "")
                                val = "1" if str(raw_val) in ("1", "True", "true") else "0"
                            else:
                                raw_val = r.get(key, "")
                                val = str(raw_val)
                            row.append(val)
                        data.append(row)

                    # Limit displayed rows to MAX_MOTOR_ROWS
                    filtered_motors = len(data)
                    truncated = filtered_motors > self.MAX_MOTOR_ROWS
                    display_data = data[: self.MAX_MOTOR_ROWS] if truncated else data

                    # Only update table if data changed
                    if display_data != self._last_table_data:
                        # Always clear and rebuild - simpler and avoids API issues
                        table.clear()
                        for rec in display_data:
                            table.add_row(*rec)
                        self._last_table_data = [list(row) for row in display_data]

                    # Update hint - only show truncation message if needed
                    hint_widget = self.query_one("#table_hint", Static)
                    if truncated:
                        hidden = filtered_motors - self.MAX_MOTOR_ROWS
                        new_hint = f"[dim]... +{hidden} more motors[/dim]"
                    elif total_devices > 1 and self._transport == "mqtt":
                        # Show hint about switching controllers
                        new_hint = f"[dim]^L to switch controller ({total_devices} available)[/dim]"
                    else:
                        new_hint = ""
                    if self._last_hint != new_hint:
                        hint_widget.update(new_hint)
                        self._last_hint = new_hint
                except Exception:
                    # Be robust to transient shape changes
                    try:
                        table.clear()
                        for rec in data:
                            table.add_row(*rec)
                    except Exception:
                        pass

                # Update log with only new lines
                rich_log = self.query_one("#log", RichLog)
                new_lines: List[str] = []
                if log_seq is not None:
                    if self._last_log_seq is None or log_seq < self._last_log_seq:
                        new_lines = list(log)
                    else:
                        delta = log_seq - self._last_log_seq
                        if delta > 0:
                            take = min(delta, len(log))
                            new_lines = list(log[-take:])
                    if new_lines:
                        for ln in new_lines:
                            rich_log.write(ln)
                    self._last_log_seq = log_seq
                    self._last_log_len = len(log)
                else:
                    new_lines = log[self._last_log_len :]
                    for ln in new_lines:
                        rich_log.write(ln)
                    if new_lines:
                        self._last_log_len = len(log)
                    self._last_log_seq = None
                # Optionally surface connection error
                if err:
                    self.status = f"[yellow]{err}[/]"

                # Table panel uses auto height with max-height in CSS, no dynamic sizing needed

        try:
            SerialApp().run()
            return 0
        except KeyboardInterrupt:
            return 130


# Development entrypoint with a simple mock worker
if __name__ == "__main__":  # pragma: no cover
    import random
    import threading
    import time

    class _MockWorker:
        def __init__(self) -> None:
            self.port = "MOCK"
            self.baud = 115200
            self.timeout = 1.0
            self.period = 0.5
            self._lock = threading.Lock()
            self._rows: List[Dict[str, str]] = []
            self._log: List[str] = []
            self._log_seq: int = 0
            self._err: Optional[str] = None
            self._ts: float = 0.0
            self._help: str = ""
            self._thermal: Optional[Tuple[bool, Optional[int]]] = (True, 90)
            self._cmdq: List[str] = []
            self._stop = False

            def _bg():
                pos = [0] * 8
                awake = [1] * 8
                moving = [0] * 8
                homed = [1] * 8
                steps_since_home = [0] * 8
                while not self._stop:
                    i = random.randint(0, 7)
                    delta = random.choice([-10, -5, 0, 5, 10])
                    pos[i] = max(-1200, min(1200, pos[i] + delta))
                    moving[i] = 1 if delta != 0 else 0
                    steps_since_home[i] = max(0, steps_since_home[i] + abs(delta))
                    rows = []
                    for mid in range(8):
                        rows.append(
                            {
                                "id": str(mid),
                                "pos": str(pos[mid]),
                                "moving": str(moving[mid]),
                                "awake": str(awake[mid]),
                                "homed": str(homed[mid]),
                                "steps_since_home": str(steps_since_home[mid]),
                                "budget_s": f"{90.0 - steps_since_home[mid] * 0.01:.1f}",
                                "ttfc_s": f"{(steps_since_home[mid] * 0.01) % 2:.1f}",
                            }
                        )
                    with self._lock:
                        self._rows = rows
                        self._ts = time.time()
                        if self._cmdq:
                            cmd = self._cmdq.pop(0).strip()
                            self._log.append(f"> {cmd}")
                            self._log_seq += 1
                            self._log.append("CTRL:OK est_ms=100")
                            self._log_seq += 1
                    time.sleep(self.period)

            self._thread = threading.Thread(target=_bg, daemon=True)
            self._thread.start()

        def queue_cmd(self, cmd: str) -> None:
            with self._lock:
                self._cmdq.append(cmd if cmd.endswith("\n") else cmd + "\n")

        def get_state(self):
            with self._lock:
                return (
                    list(self._rows),
                    list(self._log),
                    self._err,
                    self._ts,
                    self._help,
                    {"transport": "mock", "device": "MOCK"},
                    self._log_seq,
                )

        def get_thermal_state(self):
            with self._lock:
                return self._thermal

        def stop(self):
            self._stop = True
            self._thread.join(timeout=1.0)

    # Minimal renderer using the mock worker
    ui = TextualUI(_MockWorker(), lambda rows: "")
    raise SystemExit(ui.run())
