from __future__ import annotations

import subprocess
import sys
from pathlib import Path
from typing import Any, List

try:
    _THIS_FILE = Path(__file__).resolve()
except NameError:  # pragma: no cover - some SCons invocations omit __file__
    _THIS_FILE = Path.cwd() / "tools" / "python_quality_target.py"

PROJECT_ROOT = _THIS_FILE.parents[1] if _THIS_FILE.parent.parent else Path.cwd()
PYPROJECT = PROJECT_ROOT / "pyproject.toml"
DEFAULT_TARGETS = ["serial_cli", "tools", "test"]

try:  # PlatformIO provides SCons' Import
    from SCons.Script import Import as scons_import  # type: ignore[import-not-found]
except Exception:  # pragma: no cover - allows linting outside PIO
    SCONS_ENV: Any = None
else:
    scons_import("env")
    SCONS_ENV = globals().get("env")


def _ruff_invocation() -> List[str]:
    """Prefer the repo virtualenv Ruff binary, fall back to module execution."""

    venv_bin = PROJECT_ROOT / ".venv"
    candidates = [venv_bin / "bin" / "ruff", venv_bin / "Scripts" / "ruff.exe"]
    for candidate in candidates:
        if candidate.exists():
            return [str(candidate)]
    python_exe = None
    if SCONS_ENV is not None and hasattr(SCONS_ENV, "get"):
        try:
            python_exe = SCONS_ENV.get("PYTHONEXE")
        except Exception:  # pragma: no cover - env may not behave like dict during lint
            python_exe = None
    return [python_exe or sys.executable, "-m", "ruff"]


def _run_ruff(args: List[str]) -> None:
    cmd = _ruff_invocation() + args
    result = subprocess.call(cmd, cwd=str(PROJECT_ROOT))
    if result != 0:
        raise SystemExit(result)


def _lint_action(target, source, env):  # pylint: disable=unused-argument
    _run_ruff(["check", "--config", str(PYPROJECT), *DEFAULT_TARGETS])


def _format_action(target, source, env):  # pylint: disable=unused-argument
    _run_ruff(["format", "--config", str(PYPROJECT), *DEFAULT_TARGETS])


if SCONS_ENV is not None:
    env = SCONS_ENV
    env.AddCustomTarget(
        "lint_python",
        None,
        env.VerboseAction(_lint_action, "Linting Python host tooling with Ruff"),
        title="Python Ruff Lint",
        description="Run Ruff check across serial_cli/, tools/, and test/.",
    )

    env.AddCustomTarget(
        "format_python",
        None,
        env.VerboseAction(_format_action, "Formatting Python host tooling with Ruff"),
        title="Python Ruff Format",
        description="Reformat Python sources with Ruff to enforce repo standards.",
    )
