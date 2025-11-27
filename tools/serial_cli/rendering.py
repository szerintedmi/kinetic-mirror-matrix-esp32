"""Table rendering utilities for motor status and presence data."""

from __future__ import annotations

import time
from typing import Dict, List


def _render_presence_table(rows: List[Dict[str, str]]) -> str:
    """Render MQTT presence/device table."""
    cols = [
        ("device", 18, "device"),
        ("state", 8, "state"),
        ("ip", 16, "ip"),
        ("age_s", 8, "age_s"),
        ("msg_id", 36, "msg_id"),
    ]
    header = " ".join([label[:width].rjust(width) for key, width, label in cols])
    lines = [header, "-" * len(header)]
    for entry in rows:
        line_parts = []
        for key, width, label in cols:
            if key == "age_s":
                val = entry.get("age_s", "")
                if (val is None or val == "") and "last_seen" in entry:
                    try:
                        now = time.time()
                        val = max(0.0, now - float(entry["last_seen"]))
                    except Exception:
                        val = ""
                try:
                    val = f"{float(val):.1f}"
                except Exception:
                    val = ""
            else:
                val = entry.get(label, "")
            line_parts.append(str(val)[:width].rjust(width))
        lines.append(" ".join(line_parts))
    if len(lines) == 2:
        lines.append("(no presence updates yet)".rjust(len(header)))
    return "\n".join(lines)


def render_table(rows: List[Dict[str, str]]) -> str:
    """Render motor status table or presence table based on row structure."""
    if rows and isinstance(rows[0], dict) and "device" in rows[0] and "id" not in rows[0]:
        return _render_presence_table(rows)
    cols = [
        ("id", 2, "id"),
        ("pos", 6, "pos"),
        ("moving", 6, "moving"),
        ("awake", 6, "awake"),
        ("homed", 6, "homed"),
        ("steps_since_home", 18, "steps_since_home"),
        ("budget_s", 15, "budget_s"),
        ("ttfc_s", 7, "ttfc_s"),
        # New timing columns (mirrors GET LAST_OP_TIMING fields)
        ("est_ms", 8, "est_ms"),
        ("started_ms", 10, "started_ms"),
        ("actual_ms", 9, "actual_ms"),
    ]
    header = " ".join([label[:width].rjust(width) for key, width, label in cols])
    lines = [header, "-" * len(header)]
    for r in rows:
        line = []
        for key, width, _label in cols:
            val = r.get(key, "")
            line.append(str(val).rjust(width))
        lines.append(" ".join(line))
    return "\n".join(lines)


__all__ = [
    "render_table",
]
