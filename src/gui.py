"""Compatibility desktop entry point for the native C++/Qt UI."""
from __future__ import annotations

import pathlib
import subprocess
import sys


def main() -> int:
    project_root = pathlib.Path(__file__).resolve().parent.parent
    native = project_root / "build" / "ai_chat_native"
    if not native.exists():
        print("Native desktop UI is not built yet.")
        print("Run: make native-build")
        return 1
    return subprocess.call([str(native)], cwd=project_root)


if __name__ == "__main__":
    raise SystemExit(main())
