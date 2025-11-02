from __future__ import annotations

import re
from dataclasses import dataclass
from typing import Dict, List, Optional, Sequence


class CommandParseError(Exception):
    pass


class UnsupportedCommandError(CommandParseError):
    pass


@dataclass
class CommandRequest:
    action: str
    params: Dict[str, object]
    raw: str
    cmd_id: Optional[str] = None


_BATCH_SPLIT_RE = re.compile(r';(?=(?:[^"]*"[^"]*")*[^"]*$)')


def split_batches(command: str) -> List[str]:
    return [part.strip() for part in _BATCH_SPLIT_RE.split(command.strip()) if part.strip()]


def _parse_target(token: str) -> object:
    token = token.strip()
    if not token:
        raise CommandParseError("target selector missing")
    upper = token.upper()
    if upper == "ALL":
        return "ALL"
    if not re.fullmatch(r"-?\d+", token):
        raise CommandParseError("target selector invalid")
    return int(token)


def _parse_int(token: str, field: str) -> int:
    token = token.strip()
    if not re.fullmatch(r"-?\d+", token):
        raise CommandParseError(f"{field} must be integer")
    return int(token)


def _parse_optional_int(token: str, field: str) -> Optional[int]:
    token = token.strip()
    if token == "":
        return None
    return _parse_int(token, field)


def _parse_csv_arguments(arg_string: str) -> List[str]:
    args: List[str] = []
    current = []
    in_quotes = False
    escape = False
    for ch in arg_string:
        if in_quotes:
            if escape:
                current.append(ch)
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == '"':
                in_quotes = False
            else:
                current.append(ch)
        else:
            if ch == ",":
                args.append("".join(current).strip())
                current = []
            elif ch == '"':
                in_quotes = True
            else:
                current.append(ch)
    if in_quotes or escape:
        raise CommandParseError("unterminated quoted argument")
    args.append("".join(current).strip())
    return args


def _parse_set_assignments(tokens: Sequence[str]) -> Dict[str, object]:
    if len(tokens) != 1:
        raise CommandParseError("SET expects single assignment")
    token = tokens[0]
    if "=" not in token:
        raise CommandParseError("SET assignment missing '='")
    key, value = token.split("=", 1)
    name = key.strip().upper()
    value = value.strip()
    if name == "THERMAL_LIMITING":
        upper = value.upper()
        if upper not in {"ON", "OFF"}:
            raise CommandParseError("THERMAL_LIMITING must be ON or OFF")
        return {"THERMAL_LIMITING": upper}
    if name == "SPEED":
        return {"speed_sps": _parse_int(value, "SPEED")}
    if name == "ACCEL":
        return {"accel_sps2": _parse_int(value, "ACCEL")}
    if name == "DECEL":
        parsed = _parse_int(value, "DECEL")
        if parsed < 0:
            raise CommandParseError("DECEL must be >= 0")
        return {"decel_sps2": parsed}
    raise UnsupportedCommandError(f"unsupported SET field '{name}'")


def parse_serial_command(command: str) -> CommandRequest:
    raw = command.strip()
    if not raw:
        raise CommandParseError("empty command")
    upper = raw.upper()

    if upper.startswith("HELP"):
        if upper != "HELP":
            raise CommandParseError("HELP does not take arguments")
        return CommandRequest(action="HELP", params={}, raw=raw)

    if upper in {"STATUS", "ST"}:
        raise UnsupportedCommandError("STATUS not supported over MQTT")

    if raw.upper().startswith("NET:"):
        prefix, rest = raw.split(":", 1)
        prefix = prefix.strip().upper()
        sub_token = rest.split(",", 1)[0].strip()
        action = f"{prefix}:{sub_token.upper()}"
        remainder = rest[len(sub_token):]
        if remainder.startswith(","):
            remainder = remainder[1:]
        if action in {"NET:STATUS", "NET:RESET", "NET:LIST"}:
            if remainder.strip():
                raise CommandParseError(f"{action} does not accept arguments")
            return CommandRequest(action=action, params={}, raw=raw)
        if action == "NET:SET":
            args = _parse_csv_arguments(remainder)
            if len(args) < 2:
                raise CommandParseError("NET:SET requires ssid and pass")
            ssid = args[0]
            password = args[1]
            return CommandRequest(
                action=action,
                params={
                    "ssid": ssid,
                    "pass": password,
                },
                raw=raw,
            )
        raise UnsupportedCommandError(f"unsupported NET action '{action}'")

    # MQTT config commands passthrough
    if raw.upper().startswith("MQTT:"):
        prefix, rest = raw.split(":", 1)
        rest = rest.strip()
        if not rest:
            raise CommandParseError("MQTT action required")
        parts = rest.split(None, 1)
        sub = parts[0].strip().upper()
        args = parts[1].strip() if len(parts) > 1 else ""
        action = f"MQTT:{sub}"
        if sub == "GET_CONFIG":
            if args:
                raise CommandParseError("MQTT:GET_CONFIG does not accept arguments")
            return CommandRequest(action=action, params={}, raw=raw)
        if sub == "SET_CONFIG":
            if args.upper() == "RESET":
                return CommandRequest(action=action, params={"reset": True}, raw=raw)
            if not args:
                raise CommandParseError("MQTT:SET_CONFIG requires assignments or RESET")
            # Parse space-delimited assignments like host=foo port=1883 user=me pass=secret
            params: Dict[str, object] = {}
            # simple split preserving quoted values
            tokens: List[str] = []
            cur = []
            in_quotes = False
            escape = False
            for ch in args:
                if in_quotes:
                    if escape:
                        cur.append(ch)
                        escape = False
                    elif ch == "\\":
                        escape = True
                    elif ch == '"':
                        in_quotes = False
                    else:
                        cur.append(ch)
                else:
                    if ch.isspace():
                        if cur:
                            tokens.append("".join(cur))
                            cur = []
                    elif ch == '"':
                        in_quotes = True
                    else:
                        cur.append(ch)
            if cur:
                tokens.append("".join(cur))

            for tok in tokens:
                if "=" not in tok:
                    raise CommandParseError("invalid assignment; expected key=value")
                k, v = tok.split("=", 1)
                key = k.strip().lower()
                value = v.strip()
                if value.startswith('"') and value.endswith('"') and len(value) >= 2:
                    value = value[1:-1]
                if key == "host":
                    params["host"] = value
                elif key == "port":
                    if not re.fullmatch(r"\d+", value):
                        raise CommandParseError("port must be integer")
                    iv = int(value)
                    if iv <= 0 or iv > 65535:
                        raise CommandParseError("port out of range")
                    params["port"] = iv
                elif key == "user":
                    params["user"] = value
                elif key in ("pass", "password"):
                    params["pass"] = value
                else:
                    raise UnsupportedCommandError(f"unsupported MQTT field '{key}'")
            return CommandRequest(action=action, params=params, raw=raw)
        raise UnsupportedCommandError(f"unsupported MQTT action '{sub}'")

    if ":" in raw.split()[0]:
        action, arg_string = raw.split(":", 1)
        action = action.strip().upper()
        if action in {"MOVE", "M"}:
            args = _parse_csv_arguments(arg_string)
            if len(args) < 2:
                raise CommandParseError("MOVE requires target and position")
            target = _parse_target(args[0])
            position = _parse_int(args[1], "position")
            params: Dict[str, object] = {
                "target_ids": target,
                "position_steps": position,
            }
            if len(args) >= 3 and args[2] != "":
                params["speed_sps"] = _parse_int(args[2], "speed")
            if len(args) >= 4 and args[3] != "":
                params["accel_sps2"] = _parse_int(args[3], "accel")
            return CommandRequest(action="MOVE", params=params, raw=raw)
        if action in {"HOME", "H"}:
            args = _parse_csv_arguments(arg_string)
            if not args:
                raise CommandParseError("HOME requires target selector")
            target = _parse_target(args[0])
            params = {"target_ids": target}
            optional_fields = [
                ("overshoot_steps", 1),
                ("backoff_steps", 2),
                ("speed_sps", 3),
                ("accel_sps2", 4),
                ("full_range_steps", 5),
            ]
            for key, idx in optional_fields:
                if idx < len(args) and args[idx] != "":
                    params[key] = _parse_int(args[idx], key)
            return CommandRequest(action="HOME", params=params, raw=raw)
        if action in {"WAKE", "SLEEP"}:
            args = _parse_csv_arguments(arg_string)
            if not args or not args[0]:
                raise CommandParseError(f"{action} requires target selector")
            target = _parse_target(args[0])
            return CommandRequest(action=action, params={"target_ids": target}, raw=raw)
        raise UnsupportedCommandError(f"unsupported command '{action}'")

    tokens = raw.split()
    action = tokens[0].upper()

    if action == "MOVE":
        raise CommandParseError("MOVE must use colon syntax (MOVE:<id>,<steps>)")
    if action == "HOME":
        raise CommandParseError("HOME must use colon syntax (HOME:<id>[,...])")

    if action == "WAKE" or action == "SLEEP":
        if len(tokens) != 2:
            raise CommandParseError(f"{action} requires target selector")
        return CommandRequest(action=action, params={"target_ids": _parse_target(tokens[1])}, raw=raw)

    if action == "GET":
        if len(tokens) == 1:
            return CommandRequest(action="GET", params={"resource": "ALL"}, raw=raw)
        resource_token = tokens[1]
        target_suffix = None
        if ":" in resource_token:
            resource, target_suffix = resource_token.split(":", 1)
        else:
            resource = resource_token
        resource = resource.upper()
        params: Dict[str, object] = {"resource": resource}
        if resource == "LAST_OP_TIMING":
            selector = target_suffix
            if selector is None and len(tokens) >= 3:
                selector = tokens[2]
            if selector:
                params["target_ids"] = _parse_target(selector)
        return CommandRequest(action="GET", params=params, raw=raw)

    if action == "SET":
        assignments = _parse_set_assignments(tokens[1:])
        return CommandRequest(action="SET", params=assignments, raw=raw)

    raise UnsupportedCommandError(f"unsupported command '{action}'")


def build_requests(serial_command: str) -> List[CommandRequest]:
    batches = split_batches(serial_command)
    requests = []
    for cmd in batches:
        requests.append(parse_serial_command(cmd))
    return requests


__all__ = [
    "CommandParseError",
    "CommandRequest",
    "UnsupportedCommandError",
    "build_requests",
    "split_batches",
]
