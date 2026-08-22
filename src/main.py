#!/usr/bin/env python3
"""Tiny AI Playground: multi-chat terminal app, TUI, memory, search and providers."""
from __future__ import annotations
import os, select, sys, termios, tty
from .chat import Chat, print_history, print_memory
from .config import DEFAULT_MODEL, MODELS
from .providers import make_provider
from .sessions import SessionStore, safe_name
from .settings import Settings, THEMES

HELP = """Commands:
  /help                  Show help
  /ui                    Open the main menu
  /chat                  Switch chats
  /new <name>            Create a chat
  /search <text>         Search this chat
  /memory                Show memories
  /remember <text>       Save a memory
  /forget <number>       Forget a memory
  /clear                 Clear this chat
  /model                 Show model
  /models                Switch model
  /provider              Switch API provider
  /theme                 Switch terminal theme
  /name <name>           Rename the AI
  /save                  Save
  /quit                  Save and exit
"""

def clear_screen():
    sys.stdout.write("\033[2J\033[H"); sys.stdout.flush()

def theme_code(settings, key): return THEMES.get(settings.theme, THEMES["default"])[key]

def read_byte(fd, timeout=0.0):
    if timeout and not select.select([fd], [], [], timeout)[0]: return ""
    try: return os.read(fd, 1).decode("utf-8", errors="ignore")
    except OSError: return ""

def read_action(fd):
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

def cooked(fd, old, prompt):
    termios.tcsetattr(fd, termios.TCSADRAIN, old)
    try: return input(prompt).strip()
    except (EOFError, KeyboardInterrupt): return ""
    finally: tty.setcbreak(fd)

def render(title, options, index, settings, footer="↑/↓ W/S: move   Enter: select   Esc: back"):
    clear_screen(); accent=theme_code(settings,"accent"); muted=theme_code(settings,"muted"); reset=theme_code(settings,"reset")
    print("="*60); print(f" {accent}{title}{reset}"); print("="*60)
    for i, option in enumerate(options): print(f" {'▶' if i==index else ' '} {option}")
    print(f"\n{muted}{footer}{reset}"); sys.stdout.flush()

def select_menu(title, options, settings, fd, old, index=0):
    index %= len(options)
    while True:
        render(title, options, index, settings)
        action,_=read_action(fd)
        if action=="up": index=(index-1)%len(options)
        elif action=="down": index=(index+1)%len(options)
        elif action in {"escape","quit"}: return None
        elif action=="enter": return index

def pause(fd, old): cooked(fd, old, "Press Enter to continue...")

def provider_label(settings): return settings.provider.title()

def rebuild(chat, settings): chat.provider=make_provider(settings.model, settings.provider)

def save_current(store, name, chat, settings):
    store.save(name, chat); settings.save()

def model_menu(chat, settings, fd, old):
    options=MODELS+["Cancel"]; choice=select_menu("SELECT MODEL", options, settings, fd, old, MODELS.index(settings.model) if settings.model in MODELS else 0)
    if choice is not None and choice < len(MODELS): settings.model=MODELS[choice]; rebuild(chat,settings); settings.save()

def provider_menu(chat, settings, fd, old):
    options=["Hugging Face (HF_TOKEN)","OpenAI-compatible (OPENAI_API_KEY)","Cancel"]
    choice=select_menu("SELECT API PROVIDER", options, settings, fd, old, 0 if settings.provider=="huggingface" else 1)
    if choice==0: settings.provider="huggingface"; rebuild(chat,settings); settings.save()
    elif choice==1: settings.provider="openai"; rebuild(chat,settings); settings.save()

def theme_menu(settings, fd, old):
    names=list(THEMES); choice=select_menu("SELECT THEME", names+["Cancel"], settings, fd, old, names.index(settings.theme) if settings.theme in names else 0)
    if choice is not None and choice < len(names): settings.theme=names[choice]; settings.save()

def search_view(chat, query, fd, old, settings):
    clear_screen(); results=chat.search(query); print(f"Search: {query}\n")
    if not results: print("No matches.")
    for m in results:
        who="You" if m["role"]=="user" else chat.ai_name; print(f"{who}: {m['content']}\n")
    pause(fd,old)

def chat_menu(store, current, settings, fd, old):
    names=list(store.chats); options=names+["New chat","Rename chat","Delete chat","Cancel"]
    choice=select_menu("CHATS",options,settings,fd,names.index(current) if current in names else 0)
    if choice is None or choice==len(options)-1: return current
    if choice < len(names): return names[choice]
    if choice==len(names):
        name=cooked(fd,old,"New chat name> ")
        key=safe_name(name)
        if key and key not in store.chats:
            store.chats[key]=Chat(make_provider(settings.model,settings.provider)); store.save(key,store.chats[key]); return key
        return current
    if choice==len(names)+1:
        name=cooked(fd,old,f"New name [{current}]> "); key=safe_name(name)
        if key and key!=current and key not in store.chats:
            store.chats[key]=store.chats.pop(current); store.save(key,store.chats[key]);
            oldpath=store.chats.get(current)
            settings.current_chat=key; settings.save(); return key
        return current
    if choice==len(names)+2 and len(names)>1:
        if cooked(fd,old,f"Delete '{current}'? type DELETE> ")=="DELETE": store.delete(current); return next(iter(store.chats))
    return current

def settings_menu(chat, settings, fd, old):
    options=[f"Theme: {settings.theme}",f"Streaming: {'on' if settings.stream else 'off'}",f"Auto memory: {'on' if settings.auto_memory else 'off'}","API provider","Model","Back"]
    while True:
        choice=select_menu("SETTINGS",options,settings,fd,old)
        if choice is None or choice==5: return
        if choice==0: theme_menu(settings,fd,old)
        elif choice==1: settings.stream=not settings.stream; settings.save()
        elif choice==2: settings.auto_memory=not settings.auto_memory; settings.save()
        elif choice==3: provider_menu(chat,settings,fd,old)
        elif choice==4: model_menu(chat,settings,fd,old)
        options=[f"Theme: {settings.theme}",f"Streaming: {'on' if settings.stream else 'off'}",f"Auto memory: {'on' if settings.auto_memory else 'off'}","API provider","Model","Back"]

def main_menu(store,current,settings):
    fd=sys.stdin.fileno(); old=termios.tcgetattr(fd); options=["Continue current chat","New chat","Switch chat","Search","Memory","Rename AI","Switch model","API provider","Settings","History","Clear conversation","Save","Help","Quit"]; index=0
    try:
        tty.setcbreak(fd)
        while True:
            chat=store.chats[current]; render(f"{chat.ai_name} — Tiny AI Playground  [{current}]",options,index,settings)
            action,_=read_action(fd)
            if action=="up": index=(index-1)%len(options); continue
            if action=="down": index=(index+1)%len(options); continue
            if action=="escape" or action=="quit": store.save(current,chat); settings.save(); return True,current
            if action!="enter": continue
            selected=options[index]
            if selected=="Continue current chat": return False,current
            if selected=="New chat":
                name=cooked(fd,old,"New chat name> "); key=safe_name(name)
                if key and key not in store.chats: store.chats[key]=Chat(make_provider(settings.model,settings.provider)); store.save(key,store.chats[key]); current=key
            elif selected=="Switch chat": current=chat_menu(store,current,settings,fd,old)
            elif selected=="Search":
                q=cooked(fd,old,"Search> ");
                if q: search_view(store.chats[current],q,fd,old,settings)
            elif selected=="Memory": clear_screen(); print_memory(chat); pause(fd,old)
            elif selected=="Rename AI":
                name=cooked(fd,old,f"AI name [{chat.ai_name}]> ");
                if name: chat.ai_name=name[:40]; store.save(current,chat)
            elif selected=="Switch model": model_menu(chat,settings,fd,old); store.save(current,chat)
            elif selected=="API provider": provider_menu(chat,settings,fd,old); store.save(current,chat)
            elif selected=="Settings": settings_menu(chat,settings,fd,old); rebuild(chat,settings)
            elif selected=="History": clear_screen(); print_history(chat); pause(fd,old)
            elif selected=="Clear conversation": chat.messages.clear(); chat.summary=""; store.save(current,chat)
            elif selected=="Save": store.save(current,chat); settings.save()
            elif selected=="Help": clear_screen(); print(HELP); pause(fd,old)
            elif selected=="Quit": store.save(current,chat); settings.save(); return True,current
    finally:
        termios.tcsetattr(fd,termios.TCSADRAIN,old); clear_screen()

def chat_loop(store,current,settings):
    chat=store.chats[current]; rebuild(chat,settings); clear_screen();
    print(f"Provider: {provider_label(settings)} | Model: {settings.model}"); print(f"{chat.ai_name} — {current}"); print("Type /ui for menu, /help for commands, /quit to exit.\n")
    while True:
        try: text=input("you> ").strip()
        except (EOFError,KeyboardInterrupt): store.save(current,chat); return 0
        if not text: continue
        if text in {"/quit","/exit"}: store.save(current,chat); settings.save(); return 0
        if text=="/ui": return 1
        if text=="/help": print(HELP); continue
        if text=="/history": print_history(chat); continue
        if text=="/memory": print_memory(chat); continue
        if text.startswith("/remember"):
            fact=text[len("/remember"):].strip(); ok=chat.remember(fact) if fact else False; print("Memory saved." if ok else "Usage: /remember <text>");
            if ok: store.save(current,chat)
            continue
        if text.startswith("/forget"):
            arg=text[len("/forget"):].strip()
            if arg.isdigit() and 1<=int(arg)<=len(chat.memories): print(f"Forgot: {chat.memories.pop(int(arg)-1)}"); store.save(current,chat)
            else: print("Usage: /forget <memory number>")
            continue
        if text.startswith("/search"):
            q=text[len("/search"):].strip(); print(f"Matches: {len(chat.search(q))}");
            for m in chat.search(q): print(f"{'You' if m['role']=='user' else chat.ai_name}: {m['content']}")
            continue
        if text=="/clear": chat.messages.clear(); chat.summary=""; store.save(current,chat); print("Conversation cleared; memories kept."); continue
        if text=="/save": store.save(current,chat); settings.save(); print("Saved."); continue
        if text=="/model": print(settings.model); continue
        if text=="/models":
            fd=sys.stdin.fileno(); old=termios.tcgetattr(fd); tty.setcbreak(fd)
            try: model_menu(chat,settings,fd,old)
            finally: termios.tcsetattr(fd,termios.TCSADRAIN,old); clear_screen()
            continue
        if text=="/provider":
            fd=sys.stdin.fileno(); old=termios.tcgetattr(fd); tty.setcbreak(fd)
            try: provider_menu(chat,settings,fd,old)
            finally: termios.tcsetattr(fd,termios.TCSADRAIN,old); clear_screen()
            continue
        if text=="/theme":
            fd=sys.stdin.fileno(); old=termios.tcgetattr(fd); tty.setcbreak(fd)
            try: theme_menu(settings,fd,old)
            finally: termios.tcsetattr(fd,termios.TCSADRAIN,old); clear_screen()
            continue
        if text.startswith("/name"):
            name=text[len("/name"):].strip();
            if name: chat.ai_name=name[:40]; store.save(current,chat); print(f"AI renamed to {chat.ai_name}.")
            else: print("Usage: /name <name>")
            continue
        try:
            if settings.stream and hasattr(chat.provider,"stream_reply"):
                chat.messages.append({"role":"user","content":text}); pieces=[]
                print(f"{chat.ai_name}> ",end="",flush=True)
                try:
                    for piece in chat.provider.stream_reply(chat.context_messages()): print(piece,end="",flush=True); pieces.append(piece)
                    answer="".join(pieces).strip(); chat.messages.append({"role":"assistant","content":answer}); print("\n")
                except Exception:
                    chat.messages.pop(); raise
            else: print(f"{chat.ai_name}> {chat.send(text)}\n")
            if settings.auto_memory:
                new=chat.auto_remember(text)
                if new: store.save(current,chat)
            else: store.save(current,chat)
        except Exception as exc: print(f"{chat.ai_name}> Error: {exc}\n",file=sys.stderr)

def main():
    settings=Settings.load();
    def factory(_name): return make_provider(settings.model,settings.provider)
    store=SessionStore.load(factory)
    # Migrate the old single history file into the main chat once.
    main_chat=store.chats.get("main")
    if main_chat and not main_chat.messages:
        legacy=Chat(make_provider(settings.model,settings.provider)); legacy.load()
        if legacy.messages or legacy.memories: store.chats["main"]=legacy
    current=settings.current_chat if settings.current_chat in store.chats else next(iter(store.chats))
    while True:
        quit_requested,current=main_menu(store,current,settings)
        settings.current_chat=current; settings.save()
        if quit_requested: return 0
        if chat_loop(store,current,settings)==0: return 0

if __name__=="__main__": raise SystemExit(main())
