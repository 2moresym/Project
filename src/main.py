#!/usr/bin/env python3
"""Tiny terminal AI playground with persistent memory and a navigable TUI."""

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
DEFAULT_MODEL = "openai/gpt-oss-120b:groq"
DEFAULT_USER_AGENT = "Project-TinyAIPlayground/1.0 (Python urllib)"


class AIProvider(Protocol):
    def reply(self, messages: list[dict[str, str]]) -> str: ...


@dataclass
class DemoProvider:
    def reply(self, messages: list[dict[str, str]]) -> str:
        user = messages[-1]["content"].strip()
        lowered = user.lower()
        if lowered in {"hello", "hi", "hey"}:
            return "Hey! I'm the tiny AI playground."
        if "who are you" in lowered:
            return "I'm Project, a small Python AI playground running in your terminal."
        return f"Demo backend received: {user}\n\nSet HF_TOKEN to use a real model."


@dataclass
class HuggingFaceProvider:
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
            return str(data["choices"][0]["message"]["content"]).strip()
        except (KeyError, IndexError, TypeError) as exc:
            raise RuntimeError(f"Unexpected Hugging Face response: {data}") from exc


@dataclass
class Chat:
    provider: AIProvider
    messages: list[dict[str, str]] = field(default_factory=list)
    memories: list[str] = field(default_factory=list)
    summary: str = ""

    def context_messages(self) -> list[dict[str, str]]:
        context: list[dict[str, str]] = []
        if self.memories or self.summary:
            memory_text = "Persistent memory:\n" + "\n".join(
                f"- {memory}" for memory in self.memories
            )
            if self.summary:
                memory_text += f"\n\nConversation summary:\n{self.summary}"
            context.append({"role": "system", "content": memory_text})
        context.extend(self.messages)
        return context

    def send(self, text: str) -> str:
        self.messages.append({"role": "user", "content": text})
        try:
            answer = self.provider.reply(self.context_messages())
        except RuntimeError:
            self.messages.pop()
            raise
        self.messages.append({"role": "assistant", "content": answer})
        return answer

    def remember(self, fact: str) -> None:
        fact = fact.strip()
        if fact and fact not in self.memories:
            self.memories.append(fact)

    def save(self) -> None:
        DATA_DIR.mkdir(parents=True, exist_ok=True)
        payload = {
            "version": 2,
            "memories": self.memories,
            "summary": self.summary,
            "messages": self.messages,
        }
        HISTORY_FILE.write_text(json.dumps(payload, indent=2, ensure_ascii=False), encoding="utf-8")

    def load(self) -> None:
        if not HISTORY_FILE.exists():
            return
        try:
            data = json.loads(HISTORY_FILE.read_text(encoding="utf-8"))
            if isinstance(data, list):
                self.messages = [m for m in data if isinstance(m, dict)]
                return
            if isinstance(data, dict):
                messages = data.get("messages", [])
                memories = data.get("memories", [])
                self.messages = [m for m in messages if isinstance(m, dict)] if isinstance(messages, list) else []
                self.memories = [str(m).strip() for m in memories if str(m).strip()] if isinstance(memories, list) else []
                self.summary = str(data.get("summary", "")).strip()
        except (OSError, json.JSONDecodeError):
            print("Warning: couldn't load saved history.", file=sys.stderr)


def print_history(chat: Chat) -> None:
    if not chat.messages:
        print("No messages yet.")
        return
    for message in chat.messages:
        role = "You" if message.get("role") == "user" else "AI"
        print(f"{role}: {message.get('content', '')}")


def print_memory(chat: Chat) -> None:
    if not chat.memories:
        print("No saved memories.")
        return
    print("Saved memories:")
    for index, memory in enumerate(chat.memories, 1):
        print(f"{index}. {memory}")


def make_provider(model: str | None = None) -> AIProvider:
    token = os.environ.get("HF_TOKEN", "").strip()
    selected_model = (model or os.environ.get("HF_MODEL", DEFAULT_MODEL)).strip() or DEFAULT_MODEL
    user_agent = os.environ.get("HF_USER_AGENT", DEFAULT_USER_AGENT).strip() or DEFAULT_USER_AGENT
    if token:
        return HuggingFaceProvider(token=token, model=selected_model, user_agent=user_agent)
    return DemoProvider()


def available_models() -> list[str]:
    return [
        "openai/gpt-oss-120b:groq",
        "openai/gpt-oss-120b:nscale",
        "openai/gpt-oss-120b:together",
        "openai/gpt-oss-20b:groq",
    ]


HELP = """Commands:
  /help              Show this help
  /history           Show conversation
  /memory            Show persistent memories
  /remember <text>   Save a persistent memory
  /forget <number>  Remove a memory
  /clear             Clear conversation (keeps memories)
  /save              Save now
  /model             Show current model
  /models            Choose another Hugging Face model/provider
  /ui                Toggle the keyboard-friendly menu
  /quit              Save and exit

In the menu: Arrow keys navigate, Enter selects, Esc closes.
"""


def choose_model(current: str) -> str:
    models = available_models()
    print("\nSelect a Hugging Face provider/model:")
    for index, model in enumerate(models, 1):
        marker = "*" if model == current else " "
        print(f"  {index}. [{marker}] {model}")
    print("  0. Cancel")
    try:
        choice = input("Select> ").strip()
    except (EOFError, KeyboardInterrupt):
        return current
    if choice.isdigit() and 1 <= int(choice) <= len(models):
        return models[int(choice) - 1]
    return current


def run_menu(chat: Chat, model: str) -> tuple[bool, str]:
    options = ["New message", "History", "Memory", "Switch model", "Clear conversation", "Save", "Help", "Quit"]
    index = 0
    while True:
        print("\n" + "=" * 48)
        print(" PROJECT — Tiny AI Playground")
        print("=" * 48)
        for i, option in enumerate(options):
            cursor = ">" if i == index else " "
            print(f" {cursor} {option}")
        print("\nUse ↑/↓ and Enter, or press Esc to return to chat.")
        try:
            key = input("Menu> ")
        except (EOFError, KeyboardInterrupt):
            return True, model
        if key.lower() in {"q", "quit"}:
            chat.save()
            return True, model
        if key.lower() == "w" or key == "\x1b[A":
            index = (index - 1) % len(options)
        elif key.lower() == "s" or key == "\x1b[B":
            index = (index + 1) % len(options)
        elif key in {"", "\r", "\n"}:
            selected = options[index]
            if selected == "New message":
                return False, model
            if selected == "History":
                print_history(chat)
            elif selected == "Memory":
                print_memory(chat)
            elif selected == "Switch model":
                model = choose_model(model)
                chat.provider = make_provider(model)
                print(f"Model set to: {model}")
            elif selected == "Clear conversation":
                chat.messages.clear()
                print("Conversation cleared; memories kept.")
            elif selected == "Save":
                chat.save()
                print("Saved.")
            elif selected == "Help":
                print(HELP)
            elif selected == "Quit":
                chat.save()
                return True, model
        elif key == "\x1b":
            return False, model


def main() -> int:
    configured_model = os.environ.get("HF_MODEL", DEFAULT_MODEL).strip() or DEFAULT_MODEL
    chat = Chat(make_provider(configured_model))
    chat.load()
    model = configured_model
    if isinstance(chat.provider, HuggingFaceProvider):
        print(f"Backend: Hugging Face ({model})")
    else:
        print("Backend: offline demo (HF_TOKEN is not set)")
    print("Project — Tiny AI Playground")
    print("Type /help for commands, /ui for the menu, /quit to exit.\n")

    while True:
        try:
            text = input("you> ").strip()
        except (EOFError, KeyboardInterrupt):
            chat.save()
            print("\nBye!")
            return 0
        if not text:
            continue
        if text in {"/quit", "/exit"}:
            chat.save()
            print("History saved. Bye!")
            return 0
        if text == "/ui":
            should_quit, model = run_menu(chat, model)
            if should_quit:
                return 0
            continue
        if text == "/help":
            print(HELP)
            continue
        if text == "/history":
            print_history(chat)
            continue
        if text == "/memory":
            print_memory(chat)
            continue
        if text.startswith("/remember"):
            fact = text[len("/remember"):].strip()
            if fact:
                chat.remember(fact)
                chat.save()
                print("Memory saved.")
            else:
                print("Usage: /remember <something to remember>")
            continue
        if text.startswith("/forget"):
            argument = text[len("/forget"):].strip()
            if argument.isdigit() and 1 <= int(argument) <= len(chat.memories):
                removed = chat.memories.pop(int(argument) - 1)
                chat.save()
                print(f"Forgot: {removed}")
            else:
                print("Usage: /forget <memory number>")
            continue
        if text == "/clear":
            chat.messages.clear()
            chat.save()
            print("Conversation cleared; memories kept.")
            continue
        if text == "/save":
            chat.save()
            print("Saved.")
            continue
        if text == "/model":
            print(model)
            continue
        if text == "/models":
            model = choose_model(model)
            chat.provider = make_provider(model)
            print(f"Model set to: {model}")
            continue
        try:
            print(f"ai> {chat.send(text)}\n")
        except RuntimeError as exc:
            print(f"ai> Error: {exc}\n", file=sys.stderr)


if __name__ == "__main__":
    raise SystemExit(main())
