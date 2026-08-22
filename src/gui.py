"""Desktop entry point for AI Chat."""
from __future__ import annotations


def main() -> int:
    try:
        from .glass import install_glass
        install_glass()
        from .qt_gui import main as qt_main
    except ImportError as exc:
        if exc.name == "PySide6":
            print("AI Chat now uses PySide6 for the desktop UI.")
            print("Install it with: python3 -m pip install -r requirements.txt")
            return 1
        raise
    return qt_main()


if __name__ == "__main__":
    raise SystemExit(main())
