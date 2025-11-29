"""Response parsing utilities for serial and MQTT responses."""

from __future__ import annotations

import shlex
import time
from typing import TYPE_CHECKING, Dict, List, Optional, Tuple

if TYPE_CHECKING:
    from typing import Any


def read_response(ser: Any, timeout: float, idle_grace: float = 0.10) -> str:
    """Read until a quiet period (idle_grace) or timeout.

    This avoids truncating multi-line responses where lines arrive in bursts
    (e.g., multi-command batches returning multiple CTRL:OK lines).
    """
    ser.timeout = max(0.01, timeout)
    chunks: List[str] = []
    start = time.time()
    last_rx: Optional[float] = None
    while time.time() - start < timeout:
        data = ser.read(1024)
        if data:
            s = data.decode(errors="replace")
            chunks.append(s)
            last_rx = time.time()
        else:
            # If we've received something and the line has gone quiet long enough,
            # treat the response as complete.
            if last_rx is not None and (time.time() - last_rx) >= idle_grace:
                break
            time.sleep(0.01)
    return "".join(chunks).strip()


def parse_thermal_get_response(text: str) -> Optional[Tuple[bool, Optional[int]]]:
    """Parse GET THERMAL_LIMITING response.
    Returns (enabled, max_budget_s) or None if not parseable.
    """
    if not text:
        return None
    enabled: Optional[bool] = None
    max_budget: Optional[int] = None
    try:
        # Expect line like: CTRL:OK THERMAL_LIMITING=ON max_budget_s=90
        for tok in text.strip().split():
            if tok.startswith("THERMAL_LIMITING="):
                val = tok.split("=", 1)[1].strip().upper()
                enabled = val == "ON"
            elif tok.startswith("max_budget_s="):
                try:
                    max_budget = int(tok.split("=", 1)[1])
                except Exception:
                    max_budget = None
        if enabled is None:
            return None
        return (enabled, max_budget)
    except Exception:
        return None


def extract_est_ms_from_ctrl_ok(text: str) -> Optional[int]:
    """Extract est_ms=N from the final CTRL:ACK (or legacy CTRL:OK) line in a response.
    Safe when WARN lines precede ACK; returns None if not present.
    """
    if not text:
        return None
    lines = text.splitlines()
    for ln in reversed(lines):
        ln = ln.strip()
        if ln.startswith("CTRL:ACK") or ln.startswith("CTRL:OK"):
            for part in ln.split():
                if part.startswith("est_ms="):
                    try:
                        return int(part.split("=", 1)[1])
                    except Exception:
                        return None
            return None
    return None


def parse_status_lines(text: str) -> List[Dict[str, str]]:
    """Parse STATUS response into list of motor state dictionaries."""
    lines = [ln.strip() for ln in text.splitlines() if ln.strip()]
    if not lines:
        return []
    if "," in lines[0] and "=" not in lines[0]:
        headers = [h.strip() for h in lines[0].split(",")]
        rows: List[Dict[str, str]] = []
        for ln in lines[1:]:
            parts = [p.strip() for p in ln.split(",")]
            row = {headers[i]: parts[i] if i < len(parts) else "" for i in range(len(headers))}
            rows.append(row)
        return rows
    out: List[Dict[str, str]] = []
    for ln in lines:
        row: Dict[str, str] = {}
        for tok in ln.split():
            if "=" in tok:
                k, v = tok.split("=", 1)
                row[k] = v
        if row:
            out.append(row)
    return out


def parse_kv_line(text: str) -> Dict[str, str]:
    """Parse key=value pairs from the last CTRL:ACK/CTRL:OK line in a response."""
    out: Dict[str, str] = {}
    if not text:
        return out
    # take last line beginning with CTRL:ACK (or legacy CTRL:OK) if present
    lines = [ln.strip() for ln in text.splitlines() if ln.strip()]
    if not lines:
        return out
    ln = lines[-1]
    upper = ln.upper()
    if upper.startswith("CTRL:"):
        try:
            parts = shlex.split(ln)
        except ValueError:
            parts = ln.split()
        for tok in parts[1:]:
            if "=" in tok:
                k, v = tok.split("=", 1)
                out[k] = v
    return out


__all__ = [
    "extract_est_ms_from_ctrl_ok",
    "parse_kv_line",
    "parse_status_lines",
    "parse_thermal_get_response",
    "read_response",
]
