from .base import BaseUI


class CursesUI(BaseUI):
    def run(self) -> int:  # pragma: no cover (interactive)
        try:
            import curses  # type: ignore
        except Exception as e:
            print("curses is required. On Windows: pip install windows-curses")
            print(f"Import error: {e}")
            return 2

        def _loop(stdscr):
            curses.curs_set(1)
            stdscr.nodelay(True)
            curses.use_default_colors()

            def draw_scrollbar(top: int, bottom: int, total_rows: int, start_row: int):
                try:
                    sh, sw = stdscr.getmaxyx()
                    if sw <= 1 or bottom < top:
                        return
                    height = max(1, bottom - top + 1)
                    if total_rows <= height:
                        return
                    col = sw - 1
                    for y in range(top, bottom + 1):
                        try:
                            stdscr.addch(y, col, ord('|'))
                        except Exception:
                            pass
                    thumb = max(1, int(height * height / max(1, total_rows)))
                    max_offset = max(1, total_rows - height)
                    rel = int((start_row / max_offset) * max(0, height - thumb))
                    for i in range(thumb):
                        y = top + min(height - 1, rel + i)
                        try:
                            stdscr.addch(y, col, ord('o'))
                        except Exception:
                            pass
                except Exception:
                    pass

            input_line = ""
            history = []
            hist_idx = None
            scroll_offset = 0
            show_help = False
            help_requested = False
            help_scroll = 0
            hint = "/q=quit  /h=help  PgUp/PgDn=scroll output  Up/Down=cmd history"

            log_pad = None
            log_pad_w = 0
            log_pad_h = 0
            help_pad = None
            help_pad_w = 0
            help_pad_h = 0

            while True:
                stdscr.erase()
                sh, sw = stdscr.getmaxyx()
                rows, log, err, last_ts, help_text = self.worker.get_state()
                pending_help = None
                pending_log = None

                if show_help:
                    if not help_requested and not help_text:
                        self.worker.queue_cmd("HELP")
                        help_requested = True

                    left_lines = [
                        "Interactive Terminal Help",
                        "",
                        "Keys:",
                        "  /q = quit",
                        "  /h = open help",
                        "  ESC = close help / clear input",
                        "  PgUp/PgDn = scroll output log",
                        "  Up/Down = command history",
                        "  Enter = send command (e.g., WAKE:ALL, MOVE:0,1200)",
                        "",
                        "Status columns:",
                        "  id = motor id",
                        "  pos = position (steps)",
                        "  moving, awake, homed = state flags",
                        "  steps_since_homed = steps since last successful HOME",
                        "  move_budget_s = remaining move time in s before reaching thermal limits",
                        "  ttfc_s = time-to-full-cooldown",
                    ]
                    right_header = ["Device commands", ""]
                    if help_text:
                        ht_lines = help_text.splitlines()
                        if ht_lines and ht_lines[0].strip().upper() == "HELP":
                            ht_lines = ht_lines[1:]
                        right_lines = right_header + (ht_lines if ht_lines else ["(no data)"])
                    else:
                        right_lines = right_header + ["(fetching...)"]

                    max_h = max(1, sh - 2)
                    col_gap = 2
                    left_w = max(28, sw // 2 - col_gap)
                    right_w = max(1, sw - left_w - 2)

                    import textwrap as _tw

                    def _wrap(lines, w):
                        out = []
                        w = max(10, w)
                        for ln in lines:
                            if not ln:
                                out.append("")
                            else:
                                out.extend(
                                    _tw.wrap(
                                        ln,
                                        width=w,
                                        replace_whitespace=False,
                                        drop_whitespace=False,
                                        break_long_words=True,
                                        break_on_hyphens=False,
                                    )
                                    or [""]
                                )
                        return out

                    L = _wrap(left_lines, left_w)
                    R = _wrap(right_lines, right_w)
                    combined = []
                    for i in range(max(len(L), len(R))):
                        ltxt = L[i] if i < len(L) else ""
                        rtxt = R[i] if i < len(R) else ""
                        combined.append((ltxt.ljust(left_w) + " " + rtxt)[: max(1, sw - 1)].ljust(max(1, sw - 1)))

                    if help_pad is None or help_pad_w != (sw - 2) or help_pad_h < len(combined):
                        help_pad_w = max(1, sw - 2)
                        help_pad_h = max(len(combined), 1)
                        help_pad = curses.newpad(help_pad_h + 1, help_pad_w)
                    help_pad.erase()
                    for i, ln in enumerate(combined):
                        if i >= help_pad_h:
                            break
                        help_pad.addnstr(i, 0, ln[: help_pad_w], help_pad_w)
                    top, bottom = 0, sh - 3
                    pending_help = (max(0, min(help_scroll, len(combined) - max(1, bottom - top + 1))), top, bottom, sw - 2, len(combined))

                    try:
                        stdscr.hline(sh - 2, 0, curses.ACS_HLINE, max(0, sw - 1))
                    except Exception:
                        pass
                    try:
                        stdscr.addnstr(sh - 1, 0, "Press ESC to close help".ljust(max(0, sw - 1)), max(0, sw - 1))
                    except Exception:
                        pass
                else:
                    # table
                    table = self.render_table(rows) if rows else "(no data)"
                    tls = table.splitlines()
                    max_table_h = max(3, min(len(tls), max(1, sh - 6)))
                    for i in range(min(max_table_h, len(tls))):
                        stdscr.addnstr(i, 0, tls[i].ljust(max(0, sw - 1)), max(0, sw - 1))
                    if len(tls) > max_table_h:
                        try:
                            stdscr.addnstr(max_table_h - 1, max(0, sw - 2), "▼", 1)
                        except Exception:
                            pass
                    try:
                        stdscr.hline(max_table_h, 0, curses.ACS_HLINE, max(0, sw - 1))
                    except Exception:
                        pass
                    # log pad
                    log_h = max(1, sh - max_table_h - 5)
                    if log_pad is None or log_pad_w != (sw - 2) or log_pad_h < len(log):
                        log_pad_w = max(1, sw - 2)
                        log_pad_h = max(len(log), 1)
                        log_pad = curses.newpad(log_pad_h + 1, log_pad_w)
                    log_pad.erase()
                    for i, ln in enumerate(log):
                        if i >= log_pad_h:
                            break
                        log_pad.addnstr(i, 0, ln[: log_pad_w].ljust(log_pad_w), log_pad_w)
                    top = max_table_h + 1
                    bottom = top + log_h - 1
                    start = max(0, len(log) - log_h - scroll_offset)
                    pending_log = (start, top, bottom, sw - 2, len(log))

                    # status and input
                    age = 0.0
                    if last_ts:
                        import time as _t
                        age = max(0.0, _t.time() - last_ts)
                    status = f"port={self.worker.port} baud={self.worker.baud} last status={age:.1f}s"
                    try:
                        stdscr.hline(sh - 4, 0, curses.ACS_HLINE, max(0, sw - 1))
                    except Exception:
                        pass
                    try:
                        stdscr.addnstr(sh - 3, 0, status.ljust(max(0, sw - 1)), max(0, sw - 1))
                        stdscr.addnstr(sh - 2, 0, hint.ljust(max(0, sw - 1)), max(0, sw - 1))
                        stdscr.addnstr(sh - 1, 0, ("cmd> " + input_line).ljust(max(0, sw - 1)), max(0, sw - 1))
                        stdscr.move(max(0, sh - 1), min(len("cmd> ") + len(input_line), max(0, sw - 2)))
                    except Exception:
                        pass

                # flush stdscr
                try:
                    stdscr.refresh()
                except Exception:
                    pass

                # draw pads last + scrollbar
                try:
                    if show_help and pending_help and help_pad is not None:
                        s, t, b, r, total = pending_help
                        help_pad.refresh(s, 0, t, 0, b, r)
                        draw_scrollbar(t, b, total, s)
                    if (not show_help) and pending_log and log_pad is not None:
                        s, t, b, r, total = pending_log
                        log_pad.refresh(s, 0, t, 0, b, r)
                        draw_scrollbar(t, b, total, s)
                except Exception:
                    pass

                # input handling
                try:
                    ch = stdscr.get_wch()
                except Exception:
                    ch = None
                if ch is None:
                    curses.napms(25)
                    continue
                if isinstance(ch, str):
                    if ch == "\n":
                        cmd = input_line.strip()
                        if cmd:
                            if cmd.startswith("/"):
                                meta = cmd[1:].strip().lower()
                                if meta in ("q", "quit"):
                                    return
                                if meta in ("h", "help"):
                                    show_help = True
                                    help_requested = False
                                    help_scroll = 0
                            else:
                                self.worker.queue_cmd(cmd)
                                if (not history) or history[-1] != cmd:
                                    history.append(cmd)
                                hist_idx = None
                        input_line = ""
                    elif ch == "\x7f" or ch == "\b":
                        input_line = input_line[:-1]
                    elif ch == "\x1b":
                        if show_help:
                            show_help = False
                            help_requested = False
                            help_scroll = 0
                        else:
                            input_line = ""
                    else:
                        if ch.isprintable():
                            input_line += ch
                else:
                    if ch == curses.KEY_PPAGE:
                        if show_help:
                            page = max(1, (sh - 2) - 1)
                            help_scroll = max(0, min(max(0, help_pad_h - (sh - 2)), help_scroll + page))
                        else:
                            page = max(1, (sh - (max_table_h + 1) - 5))
                            scroll_offset = min(max(0, len(log) - 1), scroll_offset + page)
                    elif ch == curses.KEY_NPAGE:
                        if show_help:
                            page = max(1, (sh - 2) - 1)
                            help_scroll = max(0, help_scroll - page)
                        else:
                            page = max(1, (sh - (max_table_h + 1) - 5))
                            scroll_offset = max(0, scroll_offset - page)
                    elif ch == curses.KEY_UP:
                        if history:
                            if hist_idx is None:
                                hist_idx = len(history) - 1
                            elif hist_idx > 0:
                                hist_idx -= 1
                            input_line = history[hist_idx]
                    elif ch == curses.KEY_DOWN:
                        if history and hist_idx is not None:
                            if hist_idx < len(history) - 1:
                                hist_idx += 1
                                input_line = history[hist_idx]
                            else:
                                hist_idx = None
                                input_line = ""

        try:
            curses.wrapper(_loop)
            return 0
        except KeyboardInterrupt:
            return 130

