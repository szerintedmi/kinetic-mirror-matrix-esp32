from __future__ import annotations

import enum
from dataclasses import dataclass, field
from typing import Dict, List, Optional


class EventType(str, enum.Enum):
    ACK = "ack"
    DONE = "done"
    WARN = "warn"
    ERROR = "error"
    INFO = "info"
    DATA = "data"


@dataclass
class ResponseEvent:
    """Normalized representation of dispatcher-driven response events."""

    source: str  # "serial" or "mqtt"
    raw: str
    event_type: EventType
    action: Optional[str] = None
    cmd_id: Optional[str] = None
    msg_id: Optional[str] = None
    status: Optional[str] = None
    code: Optional[str] = None
    reason: Optional[str] = None
    attributes: Dict[str, str] = field(default_factory=dict)
    warnings: List[Dict[str, str]] = field(default_factory=list)
    errors: List[Dict[str, str]] = field(default_factory=list)

    def attribute(self, key: str, default: Optional[str] = None) -> Optional[str]:
        return self.attributes.get(key, default)


def _strip_quotes(value: str) -> str:
    if len(value) >= 2 and value[0] == value[-1] == '"':
        return value[1:-1]
    return value


def parse_serial_line(line: str) -> Optional[ResponseEvent]:
    raw = line.strip()
    if not raw:
        return None
    upper = raw.upper()

    if upper.startswith("CTRL:"):
        tokens = raw.split()
        kind = tokens[0][5:].upper()
        if kind == "ACK":
            event_type = EventType.ACK
        elif kind == "DONE":
            event_type = EventType.DONE
        elif kind == "WARN":
            event_type = EventType.WARN
        elif kind == "ERR":
            event_type = EventType.ERROR
        elif kind == "INFO":
            event_type = EventType.INFO
        else:
            event_type = EventType.INFO
        attrs: Dict[str, str] = {}
        code: Optional[str] = None
        reason: Optional[str] = None
        msg_id: Optional[str] = None
        cmd_id: Optional[str] = None
        action: Optional[str] = None
        status: Optional[str] = None
        remaining: List[str] = []

        for token in tokens[1:]:
            if "=" in token:
                key, value = token.split("=", 1)
                value = value.strip()
                if value.endswith(","):
                    value = value[:-1]
                if key == "msg_id":
                    msg_id = value
                elif key == "cmd_id":
                    cmd_id = value
                elif key == "action":
                    action = value
                elif key == "status":
                    status = value
                else:
                    attrs[key] = value
            else:
                remaining.append(token)

        if event_type in (EventType.WARN, EventType.ERROR):
            if remaining:
                code = remaining[0]
                reason = " ".join(remaining[1:]) if len(remaining) > 1 else ""
            elif "code" in attrs:
                code = attrs.pop("code")
                reason = attrs.pop("reason", None)
        else:
            if remaining and not status:
                reason = " ".join(remaining)

        if event_type is EventType.INFO and action is None:
            action = attrs.get("action")

        return ResponseEvent(
            source="serial",
            raw=raw,
            event_type=event_type,
            action=action,
            cmd_id=cmd_id or msg_id,
            msg_id=msg_id,
            status=status,
            code=code,
            reason=reason,
            attributes=attrs,
        )

    if upper.startswith("NET:") or upper.startswith("SSID="):
        attrs: Dict[str, str] = {}
        parts = raw.replace(",", " ").split()
        for part in parts:
            if "=" in part:
                key, value = part.split("=", 1)
                attrs[key] = _strip_quotes(value)
        return ResponseEvent(
            source="serial",
            raw=raw,
            event_type=EventType.DATA,
            action=None,
            cmd_id=attrs.get("cmd_id") or attrs.get("msg_id"),
            msg_id=attrs.get("msg_id"),
            attributes=attrs,
        )

    return None


def parse_mqtt_payload(payload: Dict[str, object]) -> Optional[ResponseEvent]:
    if not isinstance(payload, dict):
        return None

    status = str(payload.get("status", "") or "").lower()
    action = payload.get("action")
    cmd_id = payload.get("cmd_id") or payload.get("msg_id")
    event_type = EventType.INFO

    if status == "ack":
        event_type = EventType.ACK
    elif status == "done":
        event_type = EventType.DONE
    elif status == "error":
        event_type = EventType.ERROR
    elif status == "warn":
        event_type = EventType.WARN

    attrs: Dict[str, str] = {}
    result = payload.get("result") or {}
    if isinstance(result, dict):
        for key, value in result.items():
            attrs[str(key)] = "" if value is None else str(value)

    warnings: List[Dict[str, str]] = []
    raw_warnings = payload.get("warnings") or []
    if isinstance(raw_warnings, list):
        for item in raw_warnings:
            if isinstance(item, dict):
                warnings.append({str(k): str(v) for k, v in item.items()})

    errors: List[Dict[str, str]] = []
    raw_errors = payload.get("errors") or []
    if isinstance(raw_errors, list):
        for item in raw_errors:
            if isinstance(item, dict):
                errors.append({str(k): str(v) for k, v in item.items()})

    code = None
    reason = None
    if errors:
        first = errors[0]
        code = first.get("code")
        reason = first.get("reason") or first.get("message")

    return ResponseEvent(
        source="mqtt",
        raw="",
        event_type=event_type,
        action=str(action) if action is not None else None,
        cmd_id=str(cmd_id) if cmd_id is not None else None,
        status=status or None,
        code=code,
        reason=reason,
        attributes=attrs,
        warnings=warnings,
        errors=errors,
    )


def format_event(event: ResponseEvent, *, latency_ms: Optional[float] = None) -> str:
    headline = f"[{event.event_type.value.upper()}]"
    parts: List[str] = [headline]
    if event.action:
        parts.append(f"action={event.action}")
    if event.cmd_id:
        parts.append(f"cmd_id={event.cmd_id}")
    elif event.msg_id:
        parts.append(f"msg_id={event.msg_id}")
    if event.status and event.status.lower() not in (event.event_type.value, ""):
        parts.append(f"status={event.status}")
    if event.code:
        parts.append(f"code={event.code}")
    if event.reason:
        parts.append(event.reason)
    extra_block: Optional[str] = None
    attrs = dict(event.attributes) if event.attributes else {}
    text_value = attrs.pop("text", None)

    if attrs:
        for key in sorted(attrs.keys()):
            value = attrs[key]
            parts.append(f"{key}={value}")
    if text_value:
        lines = text_value.splitlines()
        if lines:
            extra_block = "\n".join(f"  {line}" for line in lines)
    if event.warnings:
        formatted = ",".join(
            f"{w.get('code', 'WARN')}:{w.get('reason', w.get('message', ''))}".strip(":")
            for w in event.warnings
        )
        if formatted:
            parts.append(f"warnings=[{formatted}]")
    if event.errors:
        formatted = ",".join(
            f"{e.get('code', 'ERR')}:{e.get('reason', e.get('message', ''))}".strip(":")
            for e in event.errors
        )
        if formatted:
            parts.append(f"errors=[{formatted}]")
    if latency_ms is not None:
        parts.append(f"latency_ms={latency_ms:.0f}")
    base = " ".join(parts)
    if extra_block:
        return f"{base}\n{extra_block}"
    return base


__all__ = [
    "EventType",
    "ResponseEvent",
    "format_event",
    "parse_mqtt_payload",
    "parse_serial_line",
]
