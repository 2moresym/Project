import json
import os
from dataclasses import dataclass, asdict
from .config import SETTINGS_FILE, DATA_DIR, DEFAULT_MODEL, DEFAULT_AI_NAME

@dataclass
class Settings:
    model: str = DEFAULT_MODEL
    provider: str = "huggingface"
    ai_name: str = DEFAULT_AI_NAME
    theme: str = "default"  # accent theme
    appearance: str = "system"  # system, light, dark
    stream: bool = True
    auto_memory: bool = True
    auto_summary: bool = True
    current_chat: str = "main"

    @classmethod
    def load(cls):
        try:
            data=json.loads(SETTINGS_FILE.read_text(encoding="utf-8")) if SETTINGS_FILE.exists() else {}
            values={k:v for k,v in data.items() if k in cls.__dataclass_fields__}
            return cls(**values) if isinstance(data,dict) else cls()
        except (OSError,json.JSONDecodeError,TypeError): return cls()

    def save(self):
        DATA_DIR.mkdir(parents=True,exist_ok=True)
        temp=SETTINGS_FILE.with_suffix(".tmp")
        temp.write_text(json.dumps(asdict(self),indent=2),encoding="utf-8")
        temp.replace(SETTINGS_FILE)

THEMES={
    "default":{"title":"","accent":"","muted":"\033[90m","reset":"\033[0m"},
    "cyan":{"title":"\033[96m","accent":"\033[96m","muted":"\033[90m","reset":"\033[0m"},
    "green":{"title":"\033[92m","accent":"\033[92m","muted":"\033[90m","reset":"\033[0m"},
    "magenta":{"title":"\033[95m","accent":"\033[95m","muted":"\033[90m","reset":"\033[0m"},
}

APPEARANCES=("system", "light", "dark")


def effective_appearance(appearance: str) -> str:
    if appearance in {"light", "dark"}: return appearance
    gtk_theme = os.environ.get("GTK_THEME", "").lower()
    return "dark" if "dark" in gtk_theme else "light"
