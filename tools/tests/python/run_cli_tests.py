#!/usr/bin/env python3
import subprocess
import sys
from pathlib import Path


def run(path: Path) -> int:
    print(f"[RUN] {path}")
    proc = subprocess.run([sys.executable, str(path)])
    return proc.returncode


def main() -> int:
    root = Path(__file__).resolve().parent
    tests = [
        root / "test_cli_mapping.py",
        root / "test_cli_interactive.py",
    ]
    failed = 0
    for t in tests:
        rc = run(t)
        if rc != 0:
            failed += 1
    if failed:
        print(f"[FAIL] {failed} test script(s) failed.")
        return 1
    print("[OK] CLI tests passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

