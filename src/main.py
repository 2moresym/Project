#!/usr/bin/env python3
"""Tiny terminal AI playground with a Hugging Face backend.

The app stays dependency-free by using Python's standard-library HTTP client.
Set HF_TOKEN to enable a real model; without it, the offline demo still works.
"""

from __future__ import annotations

import json
import os
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Protocol
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen


ROOT = Path(__file__).resolve().parent.parent
DATA_DIR = ROOT / "data"
HISTORY_FILE = DATA_DIR / "history.json"
HF_ENDPOINT = "https://router.huggingface.co/v1/chat/completions"
# Avoid the Cerebras :fastest route by default. The provider can still be
# changed at runtime with HF_MODEL if desired.
DEFAULT_MODEL = "openai/gpt-oss-120b:groq"
DEFAULT_USER_AGENT = "Project-TinyAIPlayground/1.0 (Python urllib)"


class AIProvider(Protocol):
    def reply(self, messages: list[dict[str, str]]) -> str: ...


@dataclass
class DemoProvider:
    """Small offline fallback used when no HF token is configured."""

    def reply(self, messages: list[dict[str, str]]) -> str:
        user = messages[-1]["content"].strip()
        lowered = user.lower()
        if lowered in {"hello", "hi", "hey"}:
            return "Hey! I'm the tiny AI playground."
        if "who are you" in lowered:
            return "I'm Project, a small Python AI playground running in your terminal."
        if "help" in lowered:
            return "Try chatting with me, /history, /clear, /save, /model, /help, or /quit."
        return f"Demo backend received: {user}\n\nSet HF_TOKEN to use a real model."


@dataclass
class HuggingFaceProvider:
    """OpenAI-compatible Hugging Face Inference Providers client using stdlib HTTP."""

    token: str
    model: str = DEFAULT_MODEL
    user_agent: str = DEFAULT_USER_AGENT

    def reply(self, messages: list[dict[str, str]]) -> str:
        payload = json.dumps({
            "model": self.model,
            "messages": messages,
            "temperature": 0.7,
            "max_tokens": 512,
            "stream": False,
        }).encode("utf-8")

        request = Request(
            HF_ENDPOINT,
            data=payload,
            headers={
                "Authorization": f"Bearer {self.token}",
                "Content-Type": "application/json",
                "Accept": "application/json",
                # Python's default urllib user-agent can be rejected by
                # provider-side Cloudflare bot/signature rules.
                "User-Agent": self.user_agent,
            },
            method="POST",
        )

        try:
            with urlopen(request, timeout=120) as response:
                data = json.loads(response.read().decode("utf-8"))
        except HTTPError as exc:
            detail = exc.read().decode("utf-8", errors="replace")
            raise RuntimeError(f"Hugging Face HTTP {exc.code}: {detail}") from exc
        except URLError as exc:
            raise RuntimeError(f"Network error: {exc.reason}") from exc
        except TimeoutError as exc:
            raise RuntimeError("Hugging Face request timed out.") from exc

        try:
            content = data["choices"][0]["message"]["content"]
        except (KeyError, IndexError, TypeError) as exc:
            raise RuntimeError(f"Unexpected Hugging Face response: {data}") from exc
        return str(content).strip()


@dataclass
class Chat:
    provider: AIProvider
    messages: list[dict[str, str]] = field(default_factory=list)

    def send(self, text: str) -> str:
        self.messages.append({"role": "user", "content": text})
        try:
            answer = self.provider.reply(self.messages)
        except RuntimeError:
            # Remove the unsent user message so a failed request does not poison history.
            self.messages.pop()
            raise
        self.messages.append({"role": "assistant", "content": answer})
        return answer

    def save(self) -> None:
        DATA_DIR.mkdir(parents=True, exist_ok=True)
        HISTORY_FILE.write_text(json.dumps(self.messages, indent=2), encoding="utf-8")

    def load(self) -> None:
        if not HISTORY_FILE.exists():
            return
        try:
            data = json.loads(HISTORY_FILE.read_text(encoding="utf-8"))
            if isinstance(data, list):
                self.messages = [m for m in data if isinstance(m, dict)]
        except (OSError, json.JSONDecodeError):
            print("Warning: couldn't load saved history.", file=sys.stderr)


def print_history(chat: Chat) -> None:
    if not chat.messages:
        print("No messages yet.")
        return
    for message in chat.messages:
        role = "You" if message["role"] == "user" else "AI"
        print(f"{role}: {message['content']}")


def make_provider() -> AIProvider:
    token = os.environ.get("HF_TOKEN", "").strip()
    model = os.environ.get("HF_MODEL", DEFAULT_MODEL).strip() or DEFAULT_MODEL
    user_agent = os.environ.get("HF_USER_AGENT", DEFAULT_USER_AGENT).strip() or DEFAULT_USER_AGENT
    if token:
        print(f"Backend: Hugging Face ({model})")
        return HuggingFaceProvider(token=token, model=model, user_agent=user_agent)
    print("Backend: offline demo (HF_TOKEN is not set)")
    return DemoProvider()


def main() -> int:
    chat = Chat(make_provider())
    chat.load()

    print("Project — Tiny AI Playground")
    print("Type /help for commands, /quit to exit.\n")

    while True:
        try:
            text = input("you> ").strip()
        except (EOFError, KeyboardInterrupt):
            print("\nBye!")
            chat.save()
            return 0

        if not text:
            continue
        if text in {"/quit", "/exit"}:
            chat.save()
            print("History saved. Bye!")
            return 0
        if text == "/help":
            print("/history  show conversation\n/clear    clear conversation\n/save     save conversation\n/model    show selected model\n/quit     save and exit")
            continue
        if text == "/history":
            print_history(chat)
            continue
        if text == "/clear":
            chat.messages.clear()
            print("Conversation cleared.")
            continue
        if text == "/save":
            chat.save()
            print(f"Saved to {HISTORY_FILE.relative_to(ROOT)}")
            continue
        if text == "/model":
            print(os.environ.get("HF_MODEL", DEFAULT_MODEL))
            continue

        try:
            print(f"ai> {chat.send(text)}\n")
        except RuntimeError as exc:
            print(f"ai> Error: {exc}\n", file=sys.stderr)


if __name__ == "__main__":
    raise SystemExit(main())
