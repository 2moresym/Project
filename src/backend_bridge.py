"""Line-oriented Python AI backend used by the native C++ UI."""
from __future__ import annotations

import json
import os
import sys

from .chat import Chat
from .config import DEFAULT_MODEL
from .providers import make_provider


def _configured_provider() -> tuple[str, str]:
    provider = os.environ.get("PROJECT_PROVIDER", "huggingface").strip().lower() or "huggingface"
    if provider not in {"huggingface", "openai"}:
        provider = "huggingface"
    if provider == "openai":
        model = os.environ.get("OPENAI_MODEL", "gpt-4o-mini").strip() or "gpt-4o-mini"
    else:
        model = os.environ.get("HF_MODEL", DEFAULT_MODEL).strip() or DEFAULT_MODEL
    return provider, model


def main() -> int:
    try:
        provider_name, model = _configured_provider()
        chat = Chat(make_provider(model, provider_name))
        chat.load()
    except Exception as exc:
        print(f"backend initialization failed: {exc}", file=sys.stderr, flush=True)
        return 2

    for raw in sys.stdin:
        try:
            request = json.loads(raw)
            action = request.get("action")
            if action == "reply":
                answer = chat.send(str(request.get("text", "")))
                chat.save()
                response = {"ok": True, "answer": answer}
            elif action == "memory":
                response = {"ok": True, "memories": chat.memories}
            else:
                response = {"ok": False, "error": "Unknown backend action"}
        except Exception as exc:
            response = {"ok": False, "error": str(exc)}

        sys.stdout.write(json.dumps(response, ensure_ascii=False) + "\n")
        sys.stdout.flush()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
