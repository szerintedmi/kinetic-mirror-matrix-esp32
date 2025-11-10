from __future__ import annotations

import os
import subprocess
from typing import Any

try:  # PlatformIO's SCons runtime
    from SCons.Script import Import as scons_import  # type: ignore[import-not-found]
except Exception:  # pragma: no cover

    def scons_import(_name: str) -> None:
        raise RuntimeError("SCons Import unavailable; run via PlatformIO")


scons_import("env")
if "env" not in globals():  # pragma: no cover - only true outside PIO
    env: Any = None  # type: ignore[assignment]


def run_clang_format(target, source, env):
    project_dir = env["PROJECT_DIR"]
    git_cmd = ["git", "ls-files", "-z", "*.c", "*.cc", "*.cpp", "*.cxx", "*.h", "*.hpp", "*.hxx"]
    try:
        files_bytes = subprocess.check_output(git_cmd, cwd=project_dir)
    except subprocess.CalledProcessError as exc:
        env.Exit(f"git ls-files failed: {exc}")
    files = [path for path in files_bytes.split(b"\0") if path]
    if not files:
        return
    fix_env = os.environ.get("CLANG_FORMAT_FIX", "").lower()
    fix_mode = fix_env in ("1", "true", "yes")
    clang_cmd = ["clang-format", "--style=file"]
    if fix_mode:
        clang_cmd.append("-i")
    else:
        clang_cmd += ["--dry-run", "--Werror"]
    clang_cmd += [path.decode("utf-8") for path in files]
    try:
        subprocess.check_call(clang_cmd, cwd=project_dir)
    except FileNotFoundError:
        env.Exit("clang-format is not installed. Install it and rerun pio check.")
    except subprocess.CalledProcessError as exc:
        env.Exit(f"clang-format failed: exit {exc.returncode}")


env.AddPreAction("check", run_clang_format)
