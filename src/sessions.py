import json
from dataclasses import dataclass
from pathlib import Path
from .config import CHATS_DIR, DEFAULT_AI_NAME
from .chat import Chat

@dataclass
class SessionStore:
    chats: dict[str, Chat]

    @classmethod
    def load(cls, provider_factory) -> "SessionStore":
        CHATS_DIR.mkdir(parents=True, exist_ok=True)
        chats = {}
        for path in sorted(CHATS_DIR.glob("*.json")):
            name = path.stem
            try:
                data = json.loads(path.read_text(encoding="utf-8"))
                chat = Chat(provider_factory(name))
                chat.from_data(data)
                chats[name] = chat
            except (OSError, json.JSONDecodeError, TypeError):
                continue
        if not chats:
            chat = Chat(provider_factory("main"), ai_name=DEFAULT_AI_NAME)
            chats["main"] = chat
        return cls(chats)

    def save(self, name: str, chat: Chat) -> None:
        CHATS_DIR.mkdir(parents=True, exist_ok=True)
        path = CHATS_DIR / f"{safe_name(name)}.json"
        temp = path.with_suffix(".tmp")
        temp.write_text(json.dumps(chat.to_data(), indent=2, ensure_ascii=False), encoding="utf-8")
        temp.replace(path)

    def delete(self, name: str) -> None:
        path = CHATS_DIR / f"{safe_name(name)}.json"
        try: path.unlink()
        except FileNotFoundError: pass
        self.chats.pop(name, None)

def safe_name(name: str) -> str:
    value = "".join(c if c.isalnum() or c in "-_" else "_" for c in name.strip())[:48]
    return value or "chat"
