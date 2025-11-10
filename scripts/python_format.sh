#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ -n "${RUFF_BIN:-}" ]]; then
  RUFF_CMD="$RUFF_BIN"
elif [[ -x "$ROOT_DIR/.venv/bin/ruff" ]]; then
  RUFF_CMD="$ROOT_DIR/.venv/bin/ruff"
elif command -v ruff >/dev/null 2>&1; then
  RUFF_CMD="$(command -v ruff)"
else
  echo "error: Ruff is not installed. Run 'pip install ruff' or set RUFF_BIN" >&2
  exit 1
fi

if [[ $# -gt 0 ]]; then
  exec "$RUFF_CMD" format --config "$ROOT_DIR/pyproject.toml" "$@"
fi

exec "$RUFF_CMD" format --config "$ROOT_DIR/pyproject.toml" serial_cli tools test
