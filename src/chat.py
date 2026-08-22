import json
import sys
from dataclasses import dataclass, field
from .config import DATA_DIR, HISTORY_FILE, DEFAULT_AI_NAME
from .providers import AIProvider

@dataclass
class Chat:
    provider: AIProvider
    messages: list[dict[str, str]] = field(default_factory=list)
    memories: list[str] = field(default_factory=list)
    summary: str = ""
    ai_name: str = DEFAULT_AI_NAME

    def context_messages(self) -> list[dict[str, str]]:
        parts = [f"Your name is {self.ai_name}. Respond naturally as this assistant."]
        if self.memories:
            parts.append("Persistent memory:\n" + "\n".join(f"- {m}" for m in self.memories))
        if self.summary:
            parts.append(f"Conversation summary:\n{self.summary}")
        return [{"role": "system", "content": "\n\n".join(parts)}] + self.messages

    def send(self, text: str) -> str:
        text = text.strip()
        if not text: return ""
        self.messages.append({"role": "user", "content": text})
        try: answer = self.provider.reply(self.context_messages())
        except Exception:
            self.messages.pop(); raise
        self.messages.append({"role": "assistant", "content": answer})
        return answer

    def remember(self, fact: str) -> bool:
        fact = fact.strip()
        if not fact or fact in self.memories: return False
        self.memories.append(fact); return True

    def auto_remember(self, text: str) -> list[str]:
        if len(text) > 240: return []
        lower = text.lower()
        triggers = ("my name is ", "i'm ", "i am ", "i like ", "i love ", "i prefer ", "i use ", "i'm working on ", "i am working on ")
        if not lower.startswith(triggers): return []
        fact = text.strip().rstrip(".!?")
        return [fact] if self.remember(fact) else []

    def search(self, query: str) -> list[dict[str, str]]:
        q = query.lower().strip()
        return [m for m in self.messages if q in m.get("content", "").lower()] if q else []

    def to_data(self) -> dict:
        return {"version": 4, "ai_name": self.ai_name, "memories": self.memories, "summary": self.summary, "messages": self.messages}

    def from_data(self, data: dict) -> None:
        if not isinstance(data, dict): return
        messages, memories = data.get("messages", []), data.get("memories", [])
        self.messages = [m for m in messages if isinstance(m, dict) and m.get("role") in {"user", "assistant", "system"} and isinstance(m.get("content"), str)] if isinstance(messages, list) else []
        self.memories = [m.strip() for m in memories if isinstance(m, str) and m.strip()] if isinstance(memories, list) else []
        self.summary = data.get("summary", "") if isinstance(data.get("summary", ""), str) else ""
        name = data.get("ai_name", DEFAULT_AI_NAME)
        if isinstance(name, str) and name.strip(): self.ai_name = name.strip()[:40]

    def save(self) -> None:
        DATA_DIR.mkdir(parents=True, exist_ok=True)
        self._write(HISTORY_FILE)

    def _write(self, path) -> None:
        temp = path.with_suffix(path.suffix + ".tmp")
        temp.write_text(json.dumps(self.to_data(), indent=2, ensure_ascii=False), encoding="utf-8")
        temp.replace(path)

    def load(self) -> None:
        if not HISTORY_FILE.exists(): return
        try:
            data = json.loads(HISTORY_FILE.read_text(encoding="utf-8"))
            if isinstance(data, list):
                self.messages = [m for m in data if isinstance(m, dict)]
                return
            self.from_data(data)
        except (OSError, json.JSONDecodeError) as exc:
            print(f"Warning: couldn't load legacy history: {exc}", file=sys.stderr)

def print_history(chat: Chat) -> None:
    if not chat.messages: print("No messages yet."); return
    for message in chat.messages:
        role = "You" if message["role"] == "user" else chat.ai_name if message["role"] == "assistant" else "System"
        print(f"{role}: {message['content']}")

def print_memory(chat: Chat) -> None:
    if not chat.memories: print("No saved memories."); return
    print("Saved memories:")
    for i, memory in enumerate(chat.memories, 1): print(f"{i}. {memory}")
