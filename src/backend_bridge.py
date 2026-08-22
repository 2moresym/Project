"""Line-oriented backend bridge used by the native C++ desktop UI."""
from __future__ import annotations

import json
import os
import sys

from .chat import Chat
from .config import DEFAULT_MODEL
from .providers import make_provider


def main() -> int:
    provider_name = "openai" if os.environ.get("OPENAI_API_KEY", "").strip() else "huggingface"
    model = os.environ.get("OPENAI_MODEL", DEFAULT_MODEL).strip() or DEFAULT_MODEL
    chat = Chat(make_provider(model, provider_name))
    chat.load()

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
