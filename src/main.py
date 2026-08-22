#!/usr/bin/env python3
"""Tiny AI Playground entry point and terminal controller."""
from __future__ import annotations
import os
import select
import sys
import termios
import tty
from .chat import Chat, print_history, print_memory
from .config import DEFAULT_MODEL, DEFAULT_AI_NAME, MODELS
from .providers import HuggingFaceProvider, make_provider

HELP = """Commands:
  /help              Show this help
  /history           Show conversation
  /memory            Show persistent memories
  /remember <text>   Save a persistent memory
  /forget <number>   Remove a memory
  /clear             Clear conversation (keeps memories)
  /save              Save now
  /model             Show current model
  /models            Choose another model
  /name <name>       Rename the AI
  /ui                Open the main menu
  /quit              Save and exit
"""

def clear_screen() -> None:
    sys.stdout.write("\x1b[2J\x1b[H")
    sys.stdout.flush()

def read_byte(fd: int, timeout: float = 0.0) -> str:
    if timeout and not select.select([fd], [], [], timeout)[0]: return ""
    try: return os.read(fd, 1).decode("utf-8", errors="ignore")
    except OSError: return ""

def read_action(fd: int) -> tuple[str, int | None]:
    first = read_byte(fd)
    if first in {"w", "W"}: return "up", None
    if first in {"s", "S"}: return "down", None
    if first in {"\r", "\n"}: return "enter", None
    if first == "\x03": return "quit", None
    if first != "\x1b": return "other", None
    second = read_byte(fd, .08)
    if second != "[": return "escape", None
    seq = ""
    while len(seq) < 64:
        char = read_byte(fd, .08)
        if not char: break
        seq += char
        if char in "ABCD": return {"A":"up","B":"down","C":"right","D":"left"}[char], None
    return "escape", None

def cooked_input(fd: int, old: list, prompt: str) -> str:
    termios.tcsetattr(fd, termios.TCSADRAIN, old)
    try: return input(prompt).strip()
    except (EOFError, KeyboardInterrupt): return ""
    finally: tty.setcbreak(fd)

def render(title: str, options: list[str], index: int, footer: str = "↑/↓ or W/S: move   Enter: select   Esc: back") -> None:
    clear_screen()
    print("=" * 56)
    print(f" {title}")
    print("=" * 56)
    for i, option in enumerate(options): print(f" {'▶' if i == index else ' '} {option}")
    print(f"\n{footer}")
    sys.stdout.flush()

def model_menu(chat: Chat, model: str, fd: int) -> str:
    old = termios.tcgetattr(fd)
    options = MODELS + ["Cancel"]
    index = MODELS.index(model) if model in MODELS else 0
    while True:
        render("SELECT MODEL", options, index)
        action, _ = read_action(fd)
        if action == "up": index = (index - 1) % len(options)
        elif action == "down": index = (index + 1) % len(options)
        elif action in {"escape", "quit"}: return model
        elif action == "enter":
            if index == len(MODELS): return model
            model = MODELS[index]
            chat.provider = make_provider(model)
            return model

def run_menu(chat: Chat, model: str) -> tuple[bool, str]:
    options = ["Continue current chat", "New chat", "Rename AI", "Switch model", "History", "Memory", "Clear conversation", "Save", "Help", "Quit"]
    index, fd = 0, sys.stdin.fileno()
    old = termios.tcgetattr(fd)
    try:
        tty.setcbreak(fd)
        while True:
            render(f"{chat.ai_name} — Tiny AI Playground", options, index)
            action, _ = read_action(fd)
            if action == "up": index = (index - 1) % len(options); continue
            if action == "down": index = (index + 1) % len(options); continue
            if action == "escape": return False, model
            if action == "quit": chat.save(); return True, model
            if action != "enter": continue
            selected = options[index]
            if selected == "Continue current chat": return False, model
            if selected == "New chat": chat.messages.clear(); chat.summary = ""; chat.save(); return False, model
            if selected == "Rename AI":
                name = cooked_input(fd, old, f"AI name [{chat.ai_name}]> ")
                if name: chat.ai_name = name[:40]; chat.save()
                tty.setcbreak(fd); continue
            if selected == "Switch model":
                model = model_menu(chat, model, fd); tty.setcbreak(fd); continue
            if selected == "History":
                clear_screen(); print_history(chat); cooked_input(fd, old, "Press Enter to continue..."); tty.setcbreak(fd); continue
            if selected == "Memory":
                clear_screen(); print_memory(chat); cooked_input(fd, old, "Press Enter to continue..."); tty.setcbreak(fd); continue
            if selected == "Clear conversation": chat.messages.clear(); chat.summary = ""; chat.save(); continue
            if selected == "Save": chat.save(); continue
            if selected == "Help":
                clear_screen(); print(HELP); cooked_input(fd, old, "Press Enter to continue..."); tty.setcbreak(fd); continue
            if selected == "Quit": chat.save(); return True, model
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old)
        clear_screen()

def chat_loop(chat: Chat, model: str) -> int:
    clear_screen()
    backend = f"Hugging Face ({model})" if isinstance(chat.provider, HuggingFaceProvider) else "offline demo (HF_TOKEN is not set)"
    print(f"Backend: {backend}")
    print(f"{chat.ai_name} — Tiny AI Playground")
    print("Type /ui for menu, /help for commands, /quit to exit.\n")
    while True:
        try: text = input("you> ").strip()
        except (EOFError, KeyboardInterrupt): chat.save(); print("\nBye!"); return 0
        if not text: continue
        if text in {"/quit", "/exit"}: chat.save(); print("History saved. Bye!"); return 0
        if text == "/ui": return 1
        if text == "/help": print(HELP); continue
        if text == "/history": print_history(chat); continue
        if text == "/memory": print_memory(chat); continue
        if text.startswith("/remember"):
            fact = text[len("/remember"):].strip()
            print("Memory saved." if fact and chat.remember(fact) else "Usage: /remember <something to remember>")
            if fact: chat.save()
            continue
        if text.startswith("/forget"):
            arg = text[len("/forget"):].strip()
            if arg.isdigit() and 1 <= int(arg) <= len(chat.memories):
                print(f"Forgot: {chat.memories.pop(int(arg)-1)}"); chat.save()
            else: print("Usage: /forget <memory number>")
            continue
        if text == "/clear": chat.messages.clear(); chat.save(); print("Conversation cleared; memories kept."); continue
        if text == "/save": chat.save(); print("Saved."); continue
        if text == "/model": print(model); continue
        if text == "/models":
            fd = sys.stdin.fileno(); old = termios.tcgetattr(fd); tty.setcbreak(fd)
            try: model = model_menu(chat, model, fd)
            finally: termios.tcsetattr(fd, termios.TCSADRAIN, old); clear_screen()
            continue
        if text.startswith("/name"):
            name = text[len("/name"):].strip()
            if name: chat.ai_name = name[:40]; chat.save(); print(f"AI renamed to {chat.ai_name}.")
            else: print("Usage: /name <name>")
            continue
        try: print(f"{chat.ai_name}> {chat.send(text)}\n")
        except RuntimeError as exc: print(f"{chat.ai_name}> Error: {exc}\n", file=sys.stderr)

def main() -> int:
    model = os.environ.get("HF_MODEL", DEFAULT_MODEL).strip() or DEFAULT_MODEL
    chat = Chat(make_provider(model)); chat.load()
    while True:
        quit_requested, model = run_menu(chat, model)
        if quit_requested: return 0
        result = chat_loop(chat, model)
        if result == 0: return 0

if __name__ == "__main__": raise SystemExit(main())
