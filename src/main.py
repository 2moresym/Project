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
DEFAULT_AI_NAME = "Project"


def clear_screen() -> None:
    """Clear the terminal and move the cursor to the top-left."""
    sys.stdout.write("\x1b[2J\x1b[H")
    sys.stdout.flush()


def clear_to_prompt() -> None:
    clear_screen()


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
        payload = {"version": 3, "ai_name": self.ai_name, "memories": self.memories,
                   "summary": self.summary, "messages": self.messages}
        temp = HISTORY_FILE.with_suffix(".json.tmp")
        temp.write_text(json.dumps(payload, indent=2, ensure_ascii=False), encoding="utf-8")
        temp.replace(HISTORY_FILE)

    def load(self) -> None:
        if not HISTORY_FILE.exists():
            return
        try:
            data = json.loads(HISTORY_FILE.read_text(encoding="utf-8"))
            if isinstance(data, list):
                self.messages = [m for m in data if isinstance(m, dict) and m.get("role") in {"user", "assistant", "system"} and isinstance(m.get("content"), str)]
                return
            if not isinstance(data, dict):
                return
            messages = data.get("messages", [])
            memories = data.get("memories", [])
            self.messages = [m for m in messages if isinstance(m, dict) and m.get("role") in {"user", "assistant", "system"} and isinstance(m.get("content"), str)] if isinstance(messages, list) else []
            self.memories = [m.strip() for m in memories if isinstance(m, str) and m.strip()] if isinstance(memories, list) else []
            self.summary = data.get("summary", "") if isinstance(data.get("summary", ""), str) else ""
            name = data.get("ai_name", DEFAULT_AI_NAME)
            if isinstance(name, str) and name.strip():
                self.ai_name = name.strip()[:40]
        except (OSError, json.JSONDecodeError) as exc:
            print(f"Warning: couldn't load saved history: {exc}", file=sys.stderr)


def print_history(chat: Chat) -> None:
    if not chat.messages:
        print("No messages yet.")
        return
    for message in chat.messages:
        role = "You" if message["role"] == "user" else chat.ai_name if message["role"] == "assistant" else "System"
        print(f"{role}: {message['content']}")


def print_memory(chat: Chat) -> None:
    if not chat.memories:
        print("No saved memories.")
        return
    print("Saved memories:")
    for i, memory in enumerate(chat.memories, 1):
        print(f"{i}. {memory}")


def make_provider(model: str | None = None) -> AIProvider:
    token = os.environ.get("HF_TOKEN", "").strip()
    selected = (model or os.environ.get("HF_MODEL", DEFAULT_MODEL)).strip() or DEFAULT_MODEL
    ua = os.environ.get("HF_USER_AGENT", DEFAULT_USER_AGENT).strip() or DEFAULT_USER_AGENT
    return HuggingFaceProvider(token, selected, ua) if token else DemoProvider()


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
  /forget <number>   Remove a memory
  /clear             Clear conversation (keeps memories)
  /save              Save now
  /model             Show current model
  /models            Choose another Hugging Face model/provider
  /name <name>       Rename the AI
  /ui                Open the main menu
  /quit              Save and exit

Menus support Arrow keys, W/S, Enter, Esc, and mouse clicks.
"""


def render_menu(title: str, options: list[str], index: int, footer: str = "↑/↓ or W/S: move   Enter/click: select   Esc: back") -> None:
    clear_screen()
    print("=" * 56)
    print(f" {title}")
    print("=" * 56)
    for i, option in enumerate(options):
        print(f" {'▶' if i == index else ' '} {option}")
    print(f"\n{footer}")
    sys.stdout.flush()


def read_byte(fd: int, timeout: float = 0.0) -> str:
    if timeout and not select.select([fd], [], [], timeout)[0]:
        return ""
    try:
        return os.read(fd, 1).decode("utf-8", errors="ignore")
    except OSError:
        return ""


def read_action(fd: int) -> tuple[str, int | None]:
    first = read_byte(fd)
    if first in {"w", "W"}: return "up", None
    if first in {"s", "S"}: return "down", None
    if first in {"\r", "\n"}: return "enter", None
    if first == "\x03": return "quit", None
    if first != "\x1b": return "other", None
    second = read_byte(fd, 0.08)
    if second != "[": return "escape", None
    sequence = ""
    while len(sequence) < 64:
        char = read_byte(fd, 0.08)
        if not char: break
        sequence += char
        if char in "ABCD":
            return {"A": "up", "B": "down", "C": "right", "D": "left"}[char], None
        if char == "M": break
    if sequence.startswith("<") and sequence.endswith("M"):
        try:
            button, _x, y = map(int, sequence[1:-1].split(";"))
            return ("click" if button == 0 else "other"), y
        except ValueError:
            pass
    return "escape", None


def cooked_input(fd: int, old_settings: list, prompt: str) -> str:
    termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
    try:
        return input(prompt).strip()
    except (EOFError, KeyboardInterrupt):
        return ""
    finally:
        tty.setcbreak(fd)


def pause_menu(fd: int, old_settings: list) -> None:
    cooked_input(fd, old_settings, "Press Enter to continue...")


def model_menu(chat: Chat, current: str, fd: int, old_settings: list) -> str:
    models = available_models()
    options = models + ["Cancel"]
    index = models.index(current) if current in models else 0
    while True:
        render_menu("SELECT MODEL", options, index, "↑/↓ or W/S: move   Enter/click: select   Esc: cancel")
        action, y = read_action(fd)
        if action == "up": index = (index - 1) % len(options)
        elif action == "down": index = (index + 1) % len(options)
        elif action in {"escape", "quit"}: return current
        elif action == "click" and y is not None:
            clicked = y - 4
            if 0 <= clicked < len(options): index, action = clicked, "enter"
        if action == "enter":
            if index >= len(models): return current
            chosen = models[index]
            chat.provider = make_provider(chosen)
            return chosen


def rename_ai(chat: Chat, fd: int, old_settings: list) -> None:
    name = cooked_input(fd, old_settings, f"AI name [{chat.ai_name}]> ")
    if name:
        chat.ai_name = name[:40]
        chat.save()


def activate(chat: Chat, model: str, selected: str, fd: int, old_settings: list) -> tuple[str, str, bool]:
    if selected == "Continue current chat": return "chat", model, False
    if selected == "New chat":
        chat.messages.clear(); chat.summary = ""; chat.save(); return "chat", model, False
    if selected == "Rename AI":
        rename_ai(chat, fd, old_settings); return "stay", model, False
    if selected == "Switch model":
        return "model", model_menu(chat, model, fd, old_settings), False
    if selected == "History":
        clear_screen(); print_history(chat); pause_menu(fd, old_settings); return "stay", model, False
    if selected == "Memory":
        clear_screen(); print_memory(chat); pause_menu(fd, old_settings); return "stay", model, False
    if selected == "Clear conversation":
        chat.messages.clear(); chat.summary = ""; chat.save(); return "stay", model, False
    if selected == "Save":
        chat.save(); return "stay", model, False
    if selected == "Help":
        clear_screen(); print(HELP); pause_menu(fd, old_settings); return "stay", model, False
    if selected == "Quit":
        chat.save(); return "quit", model, True
    return "stay", model, False


def run_menu(chat: Chat, model: str) -> tuple[bool, str]:
    options = ["Continue current chat", "New chat", "Rename AI", "Switch model", "History", "Memory", "Clear conversation", "Save", "Help", "Quit"]
    index = 0
    fd = sys.stdin.fileno()
    old_settings = termios.tcgetattr(fd)
    mouse_enabled = False
    try:
        tty.setcbreak(fd)
        sys.stdout.write("\x1b[?1000h\x1b[?1006h\x1b[?25l"); sys.stdout.flush(); mouse_enabled = True
        while True:
            render_menu(f"{chat.ai_name} — Tiny AI Playground", options, index)
            action, y = read_action(fd)
            if action == "up": index = (index - 1) % len(options)
            elif action == "down": index = (index + 1) % len(options)
            elif action == "quit": chat.save(); return True, model
            elif action == "escape": return False, model
            elif action == "click" and y is not None:
                clicked = y - 4
                if 0 <= clicked < len(options): index, action = clicked, "enter"
            if action == "enter":
                result, new_model, should_quit = activate(chat, model, options[index], fd, old_settings)
                model = new_model
                if should_quit: return True, model
                if result == "chat": return False, model
    finally:
        if mouse_enabled:
            sys.stdout.write("\x1b[?1006l\x1b[?1000l\x1b[?25h"); sys.stdout.flush()
        termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
        clear_screen()


def chat_loop(chat: Chat, model: str) -> int:
    clear_to_prompt()
    if isinstance(chat.provider, HuggingFaceProvider): print(f"Backend: Hugging Face ({model})")
    else: print("Backend: offline demo (HF_TOKEN is not set)")
    print(f"{chat.ai_name} — Tiny AI Playground")
    print("Type /ui for menu, /help for commands, /quit to exit.\n")
    while True:
        try: text = input("you> ").strip()
        except (EOFError, KeyboardInterrupt): chat.save(); print("\nBye!"); return 0
        if not text: continue
        if text in {"/quit", "/exit"}: chat.save(); print("History saved. Bye!"); return 0
        if text == "/ui":
            should_quit, model = run_menu(chat, model)
            if should_quit: return 0
            clear_to_prompt()
            if isinstance(chat.provider, HuggingFaceProvider): print(f"Backend: Hugging Face ({model})")
            else: print("Backend: offline demo (HF_TOKEN is not set)")
            print(f"{chat.ai_name} — Tiny AI Playground\n")
            continue
        if text == "/help": print(HELP); continue
        if text == "/history": print_history(chat); continue
        if text == "/memory": print_memory(chat); continue
        if text.startswith("/remember"):
            fact = text[len("/remember"):].strip()
            if chat.remember(fact): chat.save(); print("Memory saved.")
            elif fact: print("That memory is already saved.")
            else: print("Usage: /remember <something to remember>")
            continue
        if text.startswith("/forget"):
            arg = text[len("/forget"):].strip()
            if arg.isdigit() and 1 <= int(arg) <= len(chat.memories):
                removed = chat.memories.pop(int(arg)-1); chat.save(); print(f"Forgot: {removed}")
            else: print("Usage: /forget <memory number>")
            continue
        if text == "/clear": chat.messages.clear(); chat.summary = ""; chat.save(); print("Conversation cleared; memories kept."); continue
        if text == "/save": chat.save(); print("Saved."); continue
        if text == "/model": print(model); continue
        if text == "/models":
            clear_screen(); fd = sys.stdin.fileno(); old = termios.tcgetattr(fd)
            try: tty.setcbreak(fd); model = model_menu(chat, model, fd, old)
            finally: termios.tcsetattr(fd, termios.TCSADRAIN, old); clear_screen()
            print(f"Model: {model}\n"); continue
        if text.startswith("/name"):
            name = text[len("/name"):].strip()
            if name: chat.ai_name = name[:40]; chat.save(); print(f"AI renamed to {chat.ai_name}.")
            else: print(f"Current AI name: {chat.ai_name}")
            continue
        try: print(f"{chat.ai_name}> {chat.send(text)}\n")
        except Exception as exc: print(f"{chat.ai_name}> Error: {exc}\n", file=sys.stderr)


def main() -> int:
    configured_model = os.environ.get("HF_MODEL", DEFAULT_MODEL).strip() or DEFAULT_MODEL
    chat = Chat(make_provider(configured_model)); chat.load()
    model = configured_model
    clear_screen()
    try:
        should_quit, model = run_menu(chat, model)
        if should_quit: return 0
    except (OSError, termios.error) as exc:
        clear_screen(); print(f"TUI unavailable: {exc}\n")
    return chat_loop(chat, model)


if __name__ == "__main__":
    raise SystemExit(main())
