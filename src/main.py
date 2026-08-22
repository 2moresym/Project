#!/usr/bin/env python3
"""Tiny terminal AI playground with persistent memory and a keyboard/mouse TUI."""

from __future__ import annotations

import json
import os
import select
import sys
import termios
import tty
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
        user = messages[-1].get("content", "").strip() if messages else ""
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
            content = data["choices"][0]["message"]["content"]
        except (KeyError, IndexError, TypeError) as exc:
            raise RuntimeError(f"Unexpected Hugging Face response: {data}") from exc
        if not isinstance(content, str):
            raise RuntimeError(f"Unexpected Hugging Face response: {data}")
        return content.strip()


@dataclass
class Chat:
    provider: AIProvider
    messages: list[dict[str, str]] = field(default_factory=list)
    memories: list[str] = field(default_factory=list)
    summary: str = ""

    def context_messages(self) -> list[dict[str, str]]:
        context: list[dict[str, str]] = []
        if self.memories or self.summary:
            memory_text = "Persistent memory:\n"
            if self.memories:
                memory_text += "\n".join(f"- {memory}" for memory in self.memories)
            if self.summary:
                memory_text += f"\n\nConversation summary:\n{self.summary}"
            context.append({"role": "system", "content": memory_text})
        context.extend(self.messages)
        return context

    def send(self, text: str) -> str:
        text = text.strip()
        if not text:
            return ""
        self.messages.append({"role": "user", "content": text})
        try:
            answer = self.provider.reply(self.context_messages())
        except Exception:
            self.messages.pop()
            raise
        self.messages.append({"role": "assistant", "content": answer})
        return answer

    def remember(self, fact: str) -> bool:
        fact = fact.strip()
        if not fact or fact in self.memories:
            return False
        self.memories.append(fact)
        return True

    def save(self) -> None:
        DATA_DIR.mkdir(parents=True, exist_ok=True)
        payload = {
            "version": 2,
            "memories": self.memories,
            "summary": self.summary,
            "messages": self.messages,
        }
        temp = HISTORY_FILE.with_suffix(".json.tmp")
        temp.write_text(json.dumps(payload, indent=2, ensure_ascii=False), encoding="utf-8")
        temp.replace(HISTORY_FILE)

    def load(self) -> None:
        if not HISTORY_FILE.exists():
            return
        try:
            data = json.loads(HISTORY_FILE.read_text(encoding="utf-8"))
            if isinstance(data, list):
                self.messages = [
                    m for m in data
                    if isinstance(m, dict) and m.get("role") in {"user", "assistant", "system"}
                    and isinstance(m.get("content"), str)
                ]
                return
            if isinstance(data, dict):
                messages = data.get("messages", [])
                memories = data.get("memories", [])
                self.messages = [
                    m for m in messages
                    if isinstance(m, dict) and m.get("role") in {"user", "assistant", "system"}
                    and isinstance(m.get("content"), str)
                ] if isinstance(messages, list) else []
                self.memories = [
                    str(m).strip() for m in memories
                    if isinstance(m, str) and m.strip()
                ] if isinstance(memories, list) else []
                self.summary = data.get("summary", "") if isinstance(data.get("summary", ""), str) else ""
        except (OSError, json.JSONDecodeError) as exc:
            print(f"Warning: couldn't load saved history: {exc}", file=sys.stderr)


def print_history(chat: Chat) -> None:
    if not chat.messages:
        print("No messages yet.")
        return
    for message in chat.messages:
        role = "You" if message["role"] == "user" else "AI" if message["role"] == "assistant" else "System"
        print(f"{role}: {message['content']}")


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
  /ui                Open the keyboard/mouse menu
  /quit              Save and exit

In the menu: Arrow keys, W/S, Enter, Esc, and mouse clicks are supported.
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
    return models[int(choice) - 1] if choice.isdigit() and 1 <= int(choice) <= len(models) else current


def _render_menu(options: list[str], index: int) -> None:
    sys.stdout.write("\x1b[2J\x1b[H")
    print("=" * 48)
    print(" PROJECT — Tiny AI Playground")
    print("=" * 48)
    for i, option in enumerate(options):
        print(f" {'▶' if i == index else ' '} {option}")
    print("\n↑/↓ or W/S: move   Enter/click: select   Esc: back")
    sys.stdout.flush()


def _read_byte(fd: int, timeout: float = 0.0) -> str:
    if timeout and not select.select([fd], [], [], timeout)[0]:
        return ""
    return os.read(fd, 1).decode("utf-8", errors="ignore")


def _read_menu_action(fd: int) -> tuple[str, int | None]:
    first = _read_byte(fd)
    if first in {"w", "W"}:
        return "up", None
    if first in {"s", "S"}:
        return "down", None
    if first in {"\r", "\n"}:
        return "enter", None
    if first == "\x03":
        return "quit", None
    if first != "\x1b":
        return "other", None

    second = _read_byte(fd, 0.05)
    if second != "[":
        return "escape", None
    sequence = ""
    while len(sequence) < 32:
        char = _read_byte(fd, 0.05)
        if not char:
            break
        sequence += char
        if char in "ABCD":
            return {"A": "up", "B": "down", "C": "right", "D": "left"}[char], None
        if char == "M" and sequence.startswith("<"):
            break
    if sequence.startswith("<") and sequence.endswith("M"):
        try:
            button, _x, y = map(int, sequence[1:-1].split(";"))
            return ("click" if button == 0 else "other"), y
        except ValueError:
            pass
    return "escape", None


def _pause_menu(fd: int, old_settings: list) -> None:
    termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
    try:
        input("Press Enter to continue...")
    except (EOFError, KeyboardInterrupt):
        pass
    tty.setcbreak(fd)


def _activate_menu_selection(chat: Chat, model: str, selected: str, fd: int, old_settings: list) -> tuple[str, str | None, bool]:
    """Return (action, new_model, should_quit)."""
    if selected == "New message":
        return "back", None, False
    if selected == "History":
        _pause_menu(fd, old_settings)
        print_history(chat)
        _pause_menu(fd, old_settings)
        return "stay", None, False
    if selected == "Memory":
        _pause_menu(fd, old_settings)
        print_memory(chat)
        _pause_menu(fd, old_settings)
        return "stay", None, False
    if selected == "Switch model":
        _pause_menu(fd, old_settings)
        new_model = choose_model(model)
        return "model", new_model, False
    if selected == "Clear conversation":
        chat.messages.clear()
        chat.save()
        _pause_menu(fd, old_settings)
        print("Conversation cleared; memories kept.")
        _pause_menu(fd, old_settings)
        return "stay", None, False
    if selected == "Save":
        chat.save()
        _pause_menu(fd, old_settings)
        print("Saved.")
        _pause_menu(fd, old_settings)
        return "stay", None, False
    if selected == "Help":
        _pause_menu(fd, old_settings)
        print(HELP)
        _pause_menu(fd, old_settings)
        return "stay", None, False
    if selected == "Quit":
        chat.save()
        return "quit", None, True
    return "stay", None, False


def run_menu(chat: Chat, model: str) -> tuple[bool, str]:
    """Run the TUI and always return a valid (quit, model) tuple."""
    options = ["New message", "History", "Memory", "Switch model", "Clear conversation", "Save", "Help", "Quit"]
    index = 0
    fd = sys.stdin.fileno()
    old_settings = termios.tcgetattr(fd)
    mouse_enabled = False
    try:
        tty.setcbreak(fd)
        # Button events are enabled with SGR coordinates (1006) and basic clicks (1000).
        sys.stdout.write("\x1b[?1000h\x1b[?1006h\x1b[?25l")
        sys.stdout.flush()
        mouse_enabled = True
        while True:
            _render_menu(options, index)
            action, y = _read_menu_action(fd)
            if action == "up":
                index = (index - 1) % len(options)
                continue
            if action == "down":
                index = (index + 1) % len(options)
                continue
            if action == "quit":
                chat.save()
                return True, model
            if action == "escape":
                return False, model
            if action == "click" and y is not None:
                # Header is four terminal rows; options occupy rows 5..12.
                clicked = y - 5
                if 0 <= clicked < len(options):
                    index = clicked
                    action = "enter"
                else:
                    continue
            if action == "enter":
                result, new_model, should_quit = _activate_menu_selection(chat, model, options[index], fd, old_settings)
                if new_model is not None:
                    model = new_model
                    chat.provider = make_provider(model)
                if should_quit or result == "quit":
                    return True, model
                if result == "back":
                    return False, model
    finally:
        if mouse_enabled:
            sys.stdout.write("\x1b[?1006l\x1b[?1000l\x1b[?25h")
            sys.stdout.flush()
        termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)


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
            try:
                should_quit, model = run_menu(chat, model)
            except (OSError, termios.error) as exc:
                print(f"TUI unavailable: {exc}")
                continue
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
            if chat.remember(fact):
                chat.save()
                print("Memory saved.")
            elif fact:
                print("That memory is already saved.")
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
        except Exception as exc:
            print(f"ai> Unexpected error: {type(exc).__name__}: {exc}\n", file=sys.stderr)


if __name__ == "__main__":
    raise SystemExit(main())
