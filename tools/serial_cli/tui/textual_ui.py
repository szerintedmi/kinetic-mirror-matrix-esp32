from __future__ import annotations

from typing import Callable, Dict, List, Optional, Tuple

from .base import BaseUI


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
            from textual.screen import Screen, ModalScreen
            from textual.events import Key
            from textual.reactive import var
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
                _rows, _log, _err, _ts, help_text = worker.get_state()
                if not help_text:
                    worker.queue_cmd("HELP")
                self.set_interval(
                    0.25, self._refresh_help, pause=False, name="help-refresh"
                )

            def _render_left(self) -> str:
                left = [
                    "Keys:",
                    "  Ctrl+Q = quit",
                    "  ? = help",
                    "  d = toggle theme",
                    "  c = clear log",
                    "  PageUp/PageDown = scroll log",
                    "  Up/Down = command history",
                    "  Enter = send command (e.g., WAKE:ALL, MOVE:0,1200)",
                    "",
                    "Common commands:",
                    "  MOVE:<id|ALL>,<abs_steps>",
                    "  HOME:<id|ALL>[,<overshoot>][,<backoff>][,<full_range>]",
                    "  GET/SET SPEED, ACCEL, DECEL",
                    "  GET LAST_OP_TIMING[:<id|ALL>]",
                    "",
                    "Copy/paste:",
                    "  Tip: Hold Shift to select text in many terminals",
                    "",
                    "Status columns:",
                    "  id, pos, moving, awake, homed",
                    "  steps_since_home, budget_s, ttfc_s",
                    "  est_ms, started_ms, actual_ms",
                ]
                return "\n".join(left)

            def _refresh_help(self) -> None:
                _rows, _log, _err, _ts, help_text = worker.get_state()
                t = help_text or "(no data)"
                if t != self._last_help_text:
                    # Trim any leading HELP echo
                    lines = t.splitlines()
                    if lines and lines[0].strip().upper() == "HELP":
                        t = "\n".join(lines[1:])
                    self.query_one("#help-right", Static).update(t)
                    self._last_help_text = t

            BINDINGS = [
                ("escape", "dismiss", "Close"),
            ]

            def action_dismiss(self) -> None:
                self.dismiss(None)

        class SerialApp(App):
            TITLE = "Motor Control"
            CSS = """
            Screen { background: $surface; }
            Header { height: 1; }
            Footer { height: 1; }
            #main { layout: vertical; }
            #status_bar { height: 1; content-align: left middle; padding: 0 1; }
            #table_panel { height: 8; background: $panel; }
            /* Revert to accent header + normal intensity */
            DataTable { background: $panel; color: $text; }
            DataTable .datatable--header { background: $accent; color: $text; text-style: bold; }
            DataTable .datatable--cursor { background: $accent-darken-1; }
            #log_panel { height: 1fr; }
            #input_row { height: 1; layout: horizontal; background: $boost; }
            /* Ensure the "> " prompt is always visible: give it room and no side padding */
            #prompt { width: 2; content-align: right middle; padding: 0 0; color: $text-muted; }
            #cmd_input { width: 1fr; background: $panel; color: $text; border: none; }
            """

            BINDINGS = [
                Binding("Ctrl+x", "quit", "Quit", show=False),
                Binding("Ctrl+q", "quit", "Quit", show=True),
                Binding("Ctrl+i", "help", "Help"),
                Binding("pageup", "scroll_log_up", "Log Up", show=False),
                Binding("pagedown", "scroll_log_down", "Log Down", show=False),
            ]

            dark = var(True)

            def __init__(self) -> None:
                super().__init__()
                self._last_log_len = 0
                self._cols_ready = False
                self._hist: List[str] = []
                self._hist_idx: Optional[int] = None
                # Buffer to remember the user's in-progress input when
                # they first enter history navigation with Up/Down.
                # When they navigate back past the newest history item,
                # we restore this text instead of clearing the field.
                self._hist_buffer: Optional[str] = None
                self._columns: List[Tuple[str, str, int]] = list(TABLE_COLS)
                self._transport = transport.lower()

            def compose(self) -> ComposeResult:  # type: ignore[override]
                yield Header(show_clock=False)
                with Container(id="main"):
                    yield Static("", id="status_bar")
                    with Container(id="table_panel"):
                        yield DataTable(id="status_table")
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
                    for key, label, width in self._columns:
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

            def action_scroll_log_up(self) -> None:
                self.query_one("#log", RichLog).scroll_page_up()

            def action_scroll_log_down(self) -> None:
                self.query_one("#log", RichLog).scroll_page_down()

            def on_input_submitted(self, event: Input.Submitted) -> None:
                text = (event.value or "").strip()
                if not text:
                    return
                # Only device commands are sent; use ctrl+h / ctrl+q for app actions
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
                rows, log, err, last_ts, _help_text = worker.get_state()

                # Update status bar
                try:
                    import time as _t

                    age = max(0.0, _t.time() - last_ts) if last_ts else 0.0
                except Exception:
                    age = 0.0

                net: Dict[str, str] = {}
                if hasattr(worker, "get_net_info"):
                    try:
                        net = worker.get_net_info() or {}
                    except Exception:
                        net = {}

                transport = (net.get("transport") or self._transport or "").lower()
                status_text = ""
                if transport == "mqtt":
                    host = net.get("host") or "-"
                    port = net.get("port") or "-"
                    status_text = f"transport=mqtt broker={host}:{port} last update={age:.1f}s"
                else:
                    thermal_text = ""
                    if hasattr(worker, "get_thermal_state"):
                        try:
                            t_state = worker.get_thermal_state()
                        except Exception:
                            t_state = None
                        if t_state is not None:
                            enabled, max_budget = t_state
                            thermal_text = (
                                f"thermal limiting=[green]ON[/]"
                                if enabled
                                else f"thermal limiting=[red]OFF[/]"
                            )
                            if isinstance(max_budget, int):
                                thermal_text += f" (max={max_budget}s)"
                    wifi_banner = ""
                    state = (net.get("state") or "").upper()
                    ssid = net.get("ssid") or ""
                    ip = net.get("ip") or ""
                    if state == "CONNECTED":
                        wifi_banner = f"SSID: [green]{ssid}[/] connected [green]({ip})[/]"
                    elif state == "AP_ACTIVE":
                        wifi_banner = f"SSID: [yellow]{ssid}[/] AP mode [yellow]({ip})[/]"
                    elif state:
                        wifi_banner = f"SSID: [red]{ssid or 'N/A'}[/] disconnected"
                    port_val = getattr(worker, "port", "-")
                    baud_val = getattr(worker, "baud", "-")
                    status_text = f"port={port_val} baud={baud_val} last status={age:.1f}s"
                    if thermal_text:
                        status_text += f"  {thermal_text}"
                    if wifi_banner:
                        status_text += f"  {wifi_banner}"
                if err:
                    status_text += f"  error={err}"
                self.query_one("#status_bar", Static).update(status_text.strip())

                # Update table - simple replace for now
                table = self.query_one("#status_table", DataTable)
                try:
                    data: List[List[str]] = []
                    for r in rows or []:
                        row: List[str] = []
                        for key, _label, _w in self._columns:
                            val = r.get(key, "")
                            if key == "age_s":
                                try:
                                    val = f"{float(val):.1f}"
                                except Exception:
                                    val = ""
                            row.append(str(val))
                        data.append(row)
                    if table.row_count != len(data):
                        table.clear()
                        for rec in data:
                            table.add_row(*rec)
                    else:
                        for i, rec in enumerate(data):
                            table.update_row(i, rec)
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
                new_lines = log[self._last_log_len :]
                for ln in new_lines:
                    rich_log.write(ln)
                if new_lines:
                    self._last_log_len = len(log)
                # Optionally surface connection error
                if err:
                    self.status = f"[yellow]{err}[/]"

                # Dynamic sizing: fit table to motors, leave min 3 lines for logs
                try:
                    total_h = self.size.height or 24
                    fixed = 1 + 1 + 1 + 1  # header + footer + status bar + input
                    remain = max(0, total_h - fixed)
                    min_log = 3
                    desired_table = max(3, (len(rows) or 0) + 1)  # header + rows
                    if remain >= min_log + 3:
                        desired_table = min(desired_table, remain - min_log)
                    else:
                        desired_table = 3
                    log_h = max(min_log, remain - desired_table)
                    tp = self.query_one("#table_panel")
                    lp = self.query_one("#log_panel")
                    # Apply only if changed to reduce churn
                    if int(
                        getattr(tp.styles.height, "value", tp.styles.height or 0) or 0
                    ) != int(desired_table):
                        tp.styles.height = desired_table
                    if int(
                        getattr(lp.styles.height, "value", lp.styles.height or 0) or 0
                    ) != int(log_h):
                        lp.styles.height = log_h
                except Exception:
                    pass

        try:
            SerialApp().run()
            return 0
        except KeyboardInterrupt:
            return 130


# Development entrypoint with a simple mock worker
if __name__ == "__main__":  # pragma: no cover
    import threading, time, random

    class _MockWorker:
        def __init__(self) -> None:
            self.port = "MOCK"
            self.baud = 115200
            self.timeout = 1.0
            self.period = 0.5
            self._lock = threading.Lock()
            self._rows: List[Dict[str, str]] = []
            self._log: List[str] = []
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
                                "budget_s": f"{90.0 - steps_since_home[mid]*0.01:.1f}",
                                "ttfc_s": f"{(steps_since_home[mid]*0.01)%2:.1f}",
                            }
                        )
                    with self._lock:
                        self._rows = rows
                        self._ts = time.time()
                        if self._cmdq:
                            cmd = self._cmdq.pop(0).strip()
                            self._log.append(f"> {cmd}")
                            self._log.append("CTRL:OK est_ms=100")
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
