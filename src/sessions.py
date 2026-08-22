import json
from dataclasses import dataclass
from .config import CHATS_DIR, DEFAULT_AI_NAME
from .chat import Chat

@dataclass
class SessionStore:
    chats: dict[str, Chat]

    @classmethod
    def load(cls, provider_factory):
        CHATS_DIR.mkdir(parents=True,exist_ok=True);chats={}
        for path in sorted(CHATS_DIR.glob("*.json")):
            try:
                data=json.loads(path.read_text(encoding="utf-8"));chat=Chat(provider_factory(path.stem));chat.from_data(data);chats[path.stem]=chat
            except (OSError,json.JSONDecodeError,TypeError):continue
        if not chats:chats["main"]=Chat(provider_factory("main"),ai_name=DEFAULT_AI_NAME)
        return cls(chats)
    def save(self,name,chat):
        CHATS_DIR.mkdir(parents=True,exist_ok=True);path=CHATS_DIR/f"{safe_name(name)}.json";temp=path.with_suffix(".tmp");temp.write_text(json.dumps(chat.to_data(),indent=2,ensure_ascii=False),encoding="utf-8");temp.replace(path)
    def rename(self,old,new):
        old_path=CHATS_DIR/f"{safe_name(old)}.json";new_path=CHATS_DIR/f"{safe_name(new)}.json"
        if old_path.exists() and not new_path.exists():old_path.replace(new_path)
        self.chats[new]=self.chats.pop(old)
    def delete(self,name):
        try:(CHATS_DIR/f"{safe_name(name)}.json").unlink()
        except FileNotFoundError:pass
        self.chats.pop(name,None)
def safe_name(name):
    value="".join(c if c.isalnum() or c in "-_" else "_" for c in name.strip())[:48]
    return value or "chat"
