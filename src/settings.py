import json
from dataclasses import dataclass, asdict
from .config import SETTINGS_FILE, DATA_DIR, DEFAULT_MODEL, DEFAULT_AI_NAME

@dataclass
class Settings:
    model: str = DEFAULT_MODEL
    provider: str = "huggingface"
    ai_name: str = DEFAULT_AI_NAME
    theme: str = "default"
    stream: bool = True
    auto_memory: bool = True
    current_chat: str = "main"

    @classmethod
    def load(cls) -> "Settings":
        try:
            data = json.loads(SETTINGS_FILE.read_text(encoding="utf-8")) if SETTINGS_FILE.exists() else {}
            if not isinstance(data, dict): data = {}
            values = {k: v for k, v in data.items() if k in cls.__dataclass_fields__}
            return cls(**values)
        except (OSError, json.JSONDecodeError, TypeError):
            return cls()

    def save(self) -> None:
        DATA_DIR.mkdir(parents=True, exist_ok=True)
        temp = SETTINGS_FILE.with_suffix(".tmp")
        temp.write_text(json.dumps(asdict(self), indent=2), encoding="utf-8")
        temp.replace(SETTINGS_FILE)

THEMES = {
    "default": {"title": "", "accent": "", "muted": "", "reset": "\033[0m"},
    "cyan": {"title": "\033[96m", "accent": "\033[96m", "muted": "\033[90m", "reset": "\033[0m"},
    "green": {"title": "\033[92m", "accent": "\033[92m", "muted": "\033[90m", "reset": "\033[0m"},
    "magenta": {"title": "\033[95m", "accent": "\033[95m", "muted": "\033[90m", "reset": "\033[0m"},
}
