#!/usr/bin/env python3
"""Tiny AI Playground terminal controller."""
from __future__ import annotations
import os, select, sys, termios, tty
from .chat import Chat, print_history, print_memory
from .config import DEFAULT_MODEL, MODELS
from .providers import make_provider
from .sessions import SessionStore, safe_name
from .settings import Settings, THEMES
from .terminal_render import render

HELP="""Commands: /ui /chat /new <name> /search <text> /memory /remember <text> /forget <n> /clear /model /models /provider /theme /name <name> /save /quit"""

def clear(): sys.stdout.write("\033[2J\033[H"); sys.stdout.flush()
def code(s,k): return THEMES.get(s.theme,THEMES["default"])[k]
def byte(fd,t=0):
    if t and not select.select([fd],[],[],t)[0]: return ""
    try:return os.read(fd,1).decode(errors="ignore")
    except OSError:return ""
def action(fd):
    c=byte(fd)
    if c in "wW":return "up",None
    if c in "sS":return "down",None
    if c in "\r\n":return "enter",None
    if c=="\x03":return "quit",None
    if c!="\x1b":return "other",None
    if byte(fd,.08)!="[":return "escape",None
    seq=""
    while len(seq)<64:
        c=byte(fd,.08)
        if not c:break
        seq+=c
        if c in "ABCD":return {"A":"up","B":"down","C":"right","D":"left"}[c],None
    return "escape",None

def cooked(fd,old,prompt):
    termios.tcsetattr(fd,termios.TCSADRAIN,old)
    try:return input(prompt).strip()
    except (EOFError,KeyboardInterrupt):return ""
    finally:tty.setcbreak(fd)

def read_chat_line(fd,old,prompt):
    termios.tcsetattr(fd,termios.TCSADRAIN,old);tty.setcbreak(fd);sys.stdout.write(prompt);sys.stdout.flush();chars=[]
    try:
        while True:
            c=byte(fd)
            if not c:continue
            if c=="\x1b":
                if select.select([fd],[],[],0.04)[0] and byte(fd)=="[":
                    while select.select([fd],[],[],0.01)[0]:
                        if byte(fd) in "~ABCDEFGH":break
                    continue
                sys.stdout.write("\n");sys.stdout.flush();return None
            if c in "\r\n":sys.stdout.write("\n");sys.stdout.flush();return "".join(chars).strip()
            if c in ("\x7f","\b"):
                if chars:chars.pop();sys.stdout.write("\b \b");sys.stdout.flush()
                continue
            if c=="\x03":sys.stdout.write("\n");sys.stdout.flush();return "/quit"
            if c.isprintable():chars.append(c);sys.stdout.write(c);sys.stdout.flush()
    finally:termios.tcsetattr(fd,termios.TCSADRAIN,old)

def pause(fd,old):cooked(fd,old,"Press Enter to continue...")
def rebuild(chat,s):chat.provider=make_provider(s.model,s.provider)
def model_menu(chat,s,fd):
    opts=MODELS+["Cancel"];i=menu("SELECT MODEL",opts,s,fd,MODELS.index(s.model) if s.model in MODELS else 0)
    if i is not None and i<len(MODELS):s.model=MODELS[i];rebuild(chat,s);s.save()
def provider_menu(chat,s,fd):
    opts=["Hugging Face (HF_TOKEN)","OpenAI-compatible (OPENAI_API_KEY)","Cancel"];i=menu("SELECT API PROVIDER",opts,s,fd,0 if s.provider=="huggingface" else 1)
    if i==0:s.provider="huggingface";rebuild(chat,s);s.save()
    elif i==1:s.provider="openai";rebuild(chat,s);s.save()
def theme_menu(s,fd):
    names=list(THEMES);i=menu("SELECT THEME",names+["Cancel"],s,fd,names.index(s.theme) if s.theme in names else 0)
    if i is not None and i<len(names):s.theme=names[i];s.save()
def menu(title,opts,s,fd,index=0):
    index%=len(opts)
    while True:
        clear();print("="*60);print(f" {code(s,'accent')}{title}{code(s,'reset')}");print("="*60)
        for i,o in enumerate(opts):print(f" {'▶' if i==index else ' '} {o}")
        print(f"\n{code(s,'muted')}↑/↓ W/S: move   Enter: select   Esc: back{code(s,'reset')}")
        a,_=action(fd)
        if a=="up":index=(index-1)%len(opts)
        elif a=="down":index=(index+1)%len(opts)
        elif a=="enter":return index
        elif a in {"escape","quit"}:return None

def search_view(chat,q,fd,old,s):
    clear();print(f"Search: {q}\n");r=chat.search(q);print("No matches." if not r else "")
    for m in r:print(f"{'You' if m['role']=='user' else chat.ai_name}: {render(m['content'])}\n")
    pause(fd,old)

def chat_menu(store,current,s,fd,old):
    names=list(store.chats);opts=names+["New chat","Rename chat","Delete chat","Cancel"];i=menu("CHATS",opts,s,fd,names.index(current) if current in names else 0)
    if i is None or i==len(opts)-1:return current
    if i<len(names):return names[i]
    if i==len(names):
        key=safe_name(cooked(fd,old,"New chat name> "))
        if key and key not in store.chats:store.chats[key]=Chat(make_provider(s.model,s.provider));store.save(key,store.chats[key]);return key
    if i==len(names)+1:
        key=safe_name(cooked(fd,old,f"New name [{current}]> "))
        if key and key!=current and key not in store.chats:store.chats[key]=store.chats.pop(current);store.save(key,store.chats[key]);return key
    if i==len(names)+2 and len(names)>1 and cooked(fd,old,f"Delete '{current}'? type DELETE> ")=="DELETE":store.delete(current);return next(iter(store.chats))
    return current

def settings_menu(chat,s,fd,old):
    while True:
        opts=[f"Theme: {s.theme}",f"Streaming: {'on' if s.stream else 'off'}",f"Auto memory: {'on' if s.auto_memory else 'off'}",f"Auto summary: {'on' if s.auto_summary else 'off'}","API provider","Model","Back"];i=menu("SETTINGS",opts,s,fd)
        if i is None or i==6:return
        if i==0:theme_menu(s,fd)
        elif i==1:s.stream=not s.stream;s.save()
        elif i==2:s.auto_memory=not s.auto_memory;s.save()
        elif i==3:s.auto_summary=not s.auto_summary;s.save()
        elif i==4:provider_menu(chat,s,fd)
        elif i==5:model_menu(chat,s,fd)

def main_menu(store,current,s):
    fd=sys.stdin.fileno();old=termios.tcgetattr(fd);opts=["Continue current chat","New chat","Switch chat","Search","Memory","Rename AI","Switch model","API provider","Settings","History","Clear conversation","Save","Help","Quit"]
    try:
        tty.setcbreak(fd);i=0
        while True:
            chat=store.chats[current];clear();print("="*60);print(f" {code(s,'accent')}{chat.ai_name} — Tiny AI Playground [{current}]{code(s,'reset')}");print("="*60)
            for n,o in enumerate(opts):print(f" {'▶' if n==i else ' '} {o}")
            print(f"\n{code(s,'muted')}↑/↓ W/S: move   Enter: select{code(s,'reset')}");a,_=action(fd)
            if a=="up":i=(i-1)%len(opts);continue
            if a=="down":i=(i+1)%len(opts);continue
            if a in {"escape","quit"}:store.save(current,chat);s.save();return True,current
            if a!="enter":continue
            x=opts[i]
            if x=="Continue current chat":return False,current
            if x=="New chat":
                key=safe_name(cooked(fd,old,"New chat name> "))
                if key and key not in store.chats:store.chats[key]=Chat(make_provider(s.model,s.provider));store.save(key,store.chats[key]);current=key
            elif x=="Switch chat":current=chat_menu(store,current,s,fd,old)
            elif x=="Search":
                q=cooked(fd,old,"Search> ");
                if q:search_view(store.chats[current],q,fd,old,s)
            elif x=="Memory":clear();print_memory(chat);pause(fd,old)
            elif x=="Rename AI":
                n=cooked(fd,old,f"AI name [{chat.ai_name}]> ");
                if n:chat.ai_name=n[:40];store.save(current,chat)
            elif x=="Switch model":model_menu(chat,s,fd);store.save(current,chat)
            elif x=="API provider":provider_menu(chat,s,fd);store.save(current,chat)
            elif x=="Settings":settings_menu(chat,s,fd,old);rebuild(chat,s)
            elif x=="History":clear();print_history(chat);pause(fd,old)
            elif x=="Clear conversation":chat.messages.clear();chat.summary="";store.save(current,chat)
            elif x=="Save":store.save(current,chat);s.save()
            elif x=="Help":clear();print(HELP);pause(fd,old)
            elif x=="Quit":store.save(current,chat);s.save();return True,current
    finally:termios.tcsetattr(fd,termios.TCSADRAIN,old);clear()

def chat_loop(store,current,s):
    chat=store.chats[current];rebuild(chat,s);clear();print(f"Provider: {s.provider} | Model: {s.model}\n{chat.ai_name} — {current}\nPress Esc anytime to return to the UI.\n")
    fd=sys.stdin.fileno();old=termios.tcgetattr(fd)
    while True:
        text=read_chat_line(fd,old,"you> ")
        if text is None:clear();return 1
        if not text:continue
        if text in {"/quit","/exit"}:store.save(current,chat);s.save();return 0
        if text=="/ui":clear();return 1
        if text=="/help":print(HELP);continue
        if text=="/history":print_history(chat);continue
        if text=="/memory":print_memory(chat);continue
        if text.startswith("/remember"):
            fact=text[9:].strip();ok=chat.remember(fact) if fact else False;print("Memory saved." if ok else "Usage: /remember <text>")
            if ok:store.save(current,chat)
            continue
        if text.startswith("/forget"):
            a=text[7:].strip()
            if a.isdigit() and 1<=int(a)<=len(chat.memories):print(f"Forgot: {chat.memories.pop(int(a)-1)}");store.save(current,chat)
            else:print("Usage: /forget <number>")
            continue
        if text.startswith("/search"):
            q=text[7:].strip();r=chat.search(q);print(f"Matches: {len(r)}");[print(f"{'You' if m['role']=='user' else chat.ai_name}: {render(m['content'])}") for m in r];continue
        if text=="/clear":chat.messages.clear();chat.summary="";store.save(current,chat);print("Conversation cleared; memories kept.");continue
        if text=="/save":store.save(current,chat);s.save();print("Saved.");continue
        if text=="/model":print(s.model);continue
        if text in {"/models","/provider","/theme"}:
            old_menu=termios.tcgetattr(fd);tty.setcbreak(fd)
            try:
                if text=="/models":model_menu(chat,s,fd)
                elif text=="/provider":provider_menu(chat,s,fd)
                else:theme_menu(s,fd)
            finally:termios.tcsetattr(fd,termios.TCSADRAIN,old_menu);clear()
            continue
        if text.startswith("/name"):
            n=text[5:].strip()
            if n:chat.ai_name=n[:40];store.save(current,chat);print(f"AI renamed to {chat.ai_name}.")
            else:print("Usage: /name <name>")
            continue
        try:
            chat.messages.append({"role":"user","content":text});pieces=[]
            if s.stream and hasattr(chat.provider,"stream_reply"):
                print(f"{chat.ai_name}> ",end="",flush=True)
                for piece in chat.provider.stream_reply(chat.context_messages()):print(render(piece),end="",flush=True);pieces.append(piece)
                answer="".join(pieces).strip();print("\n")
            else:
                answer=chat.provider.reply(chat.context_messages())
                print(f"{chat.ai_name}> {render(answer)}\n")
            chat.messages.append({"role":"assistant","content":answer})
            if s.auto_memory:chat.auto_remember(text)
            if s.auto_summary:chat.maybe_summarize()
            store.save(current,chat)
        except Exception as exc:
            if chat.messages and chat.messages[-1].get("role")=="user":chat.messages.pop()
            print(f"{chat.ai_name}> Error: {exc}\n",file=sys.stderr)

def main():
    s=Settings.load();store=SessionStore.load(lambda _:make_provider(s.model,s.provider))
    if "main" in store.chats and not store.chats["main"].messages:
        legacy=Chat(make_provider(s.model,s.provider));legacy.load()
        if legacy.messages or legacy.memories:store.chats["main"]=legacy
    current=s.current_chat if s.current_chat in store.chats else next(iter(store.chats))
    while True:
        quit_requested,current=main_menu(store,current,s);s.current_chat=current;s.save()
        if quit_requested:return 0
        if chat_loop(store,current,s)==0:return 0

if __name__=="__main__":raise SystemExit(main())
