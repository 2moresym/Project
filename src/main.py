#!/usr/bin/env python3
"""Tiny, dependency-free terminal AI playground.

The first version is intentionally small: it provides a local chat shell and a
provider interface. The built-in provider is a deterministic demo backend so
the project runs with only Python 3. A real model can be added later without
changing the terminal UI.
"""

from __future__ import annotations

import json
import os
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Protocol


ROOT = Path(__file__).resolve().parent.parent
DATA_DIR = ROOT / "data"
HISTORY_FILE = DATA_DIR / "history.json"


class AIProvider(Protocol):
    def reply(self, messages: list[dict[str, str]]) -> str: ...


@dataclass
class DemoProvider:
    """A tiny offline provider used until a real model backend is configured."""

    def reply(self, messages: list[dict[str, str]]) -> str:
        user = messages[-1]["content"].strip()
        lowered = user.lower()

        if lowered in {"hello", "hi", "hey"}:
            return "Hey! I'm the tiny AI playground."
        if "who are you" in lowered:
            return "I'm Project, a small Python AI playground running in your terminal."
        if "help" in lowered:
            return "Try chatting with me, /history, /clear, /save, /help, or /quit."
        return f"Demo backend received: {user}\n\nA real model provider can be plugged in here later."


@dataclass
class Chat:
    provider: AIProvider
    messages: list[dict[str, str]] = field(default_factory=list)

    def send(self, text: str) -> str:
        self.messages.append({"role": "user", "content": text})
        answer = self.provider.reply(self.messages)
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


def main() -> int:
    chat = Chat(DemoProvider())
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
        if text == "/quit" or text == "/exit":
            chat.save()
            print("History saved. Bye!")
            return 0
        if text == "/help":
            print("/history  show conversation\n/clear    clear conversation\n/save     save conversation\n/quit     exit")
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

        print(f"ai> {chat.send(text)}\n")


if __name__ == "__main__":
    raise SystemExit(main())
