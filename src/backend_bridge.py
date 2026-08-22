"""Line-oriented Python AI backend used by the native C++ UI."""
from __future__ import annotations

import json
import os
import sys

from .chat import Chat
from .config import DEFAULT_MODEL
from .providers import make_provider
from .sessions import SessionStore, safe_name


def _configured_provider() -> tuple[str, str]:
    provider = os.environ.get("PROJECT_PROVIDER", "").strip().lower()
    hf_token = os.environ.get("HF_TOKEN", "").strip()
    openai_key = os.environ.get("OPENAI_API_KEY", "").strip()
    if provider not in {"huggingface", "openai"}:
        provider = "openai" if openai_key and not hf_token else "huggingface"
    elif provider == "huggingface" and not hf_token and openai_key:
        provider = "openai"
    if provider == "openai":
        model = os.environ.get("OPENAI_MODEL", "gpt-4o-mini").strip() or "gpt-4o-mini"
    else:
        model = os.environ.get("HF_MODEL", DEFAULT_MODEL).strip() or DEFAULT_MODEL
    return provider, model


def _messages(chat: Chat) -> list[dict[str, str]]:
    return [m for m in chat.messages if m.get("role") in {"user", "assistant"}]


def _chat_title(name: str, chat: Chat) -> str:
    messages = _messages(chat)
    if messages:
        first = str(messages[0].get("content", "")).replace("\n", " ").strip()
        if first:
            return first[:42] + ("…" if len(first) > 42 else "")
    return name.replace("_", " ") or name


def main() -> int:
    try:
        provider_name, model = _configured_provider()
        print(f"backend initialized provider={provider_name} model={model}", file=sys.stderr, flush=True)
        store = SessionStore.load(lambda _: make_provider(model, provider_name))
        current = next(iter(store.chats))
    except Exception as exc:
        print(f"backend initialization failed: {exc}", file=sys.stderr, flush=True)
        return 2

    for raw in sys.stdin:
        try:
            request = json.loads(raw)
            action = request.get("action")
            if action == "list_chats":
                response = {
                    "action": "chat_list",
                    "ok": True,
                    "current": current,
                    "chats": [{"name": name, "title": _chat_title(name, chat)} for name, chat in store.chats.items()],
                    "messages": _messages(store.chats[current]),
                }
            elif action == "select_chat":
                name = safe_name(str(request.get("name", "")))
                if name not in store.chats:
                    response = {"ok": False, "error": "Chat not found."}
                else:
                    current = name
                    response = {"action": "history", "ok": True, "name": name, "messages": _messages(store.chats[current])}
            elif action == "new_chat":
                name = safe_name(str(request.get("name", "")))
                if name in store.chats:
                    response = {"ok": False, "error": "A chat with that name already exists."}
                else:
                    store.chats[name] = Chat(make_provider(model, provider_name))
                    store.save(name, store.chats[name])
                    current = name
                    response = {"action": "created", "ok": True, "name": name}
            elif action == "reply":
                chat = store.chats[current]
                answer = chat.send(str(request.get("text", "")))
                store.save(current, chat)
                response = {"ok": True, "answer": answer}
            elif action == "memory":
                response = {"action": "memory", "ok": True, "memories": store.chats[current].memories}
            else:
                response = {"ok": False, "error": "Unknown backend action"}
        except Exception as exc:
            response = {"ok": False, "error": str(exc)}

        sys.stdout.write(json.dumps(response, ensure_ascii=False) + "\n")
        sys.stdout.flush()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
