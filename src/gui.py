"""Lightweight desktop UI for Tiny AI Playground."""
from __future__ import annotations
import threading
import tkinter as tk
from tkinter import messagebox, simpledialog, ttk
from .chat import Chat
from .providers import make_provider
from .sessions import SessionStore, safe_name
from .settings import Settings
from .terminal_render import render

APP_NAME = "AI Chat"

class VaxxApp(tk.Tk):
    def __init__(self):
        super().__init__(); self.settings=Settings.load(); self.store=SessionStore.load(lambda _:make_provider(self.settings.model,self.settings.provider)); self.current=self.settings.current_chat if self.settings.current_chat in self.store.chats else next(iter(self.store.chats)); self._busy=False
        self.title(f"{APP_NAME} — {self.store.chats[self.current].ai_name}"); self.geometry("1050x700"); self.minsize(760,500); self._build(); self._refresh_chats(); self._show_chat(); self.protocol("WM_DELETE_WINDOW",self._quit)
    def _build(self):
        self.grid_columnconfigure(1,weight=1); self.grid_rowconfigure(0,weight=1); side=ttk.Frame(self,padding=10); side.grid(row=0,column=0,sticky="nsew"); side.grid_rowconfigure(2,weight=1)
        ttk.Label(side,text=APP_NAME,font=("TkDefaultFont",18,"bold")).grid(row=0,column=0,sticky="w"); ttk.Button(side,text="＋ New chat",command=self._new_chat).grid(row=1,column=0,sticky="ew",pady=(10,6)); self.chat_list=tk.Listbox(side,exportselection=False,activestyle="none"); self.chat_list.grid(row=2,column=0,sticky="nsew"); self.chat_list.bind("<<ListboxSelect>>",self._select_chat)
        for row,text,cmd in [(3,"Rename chat",self._rename_chat),(4,"Delete chat",self._delete_chat),(5,"Memory",self._show_memory),(6,"Settings",self._settings),(7,"Quit",self._quit)]:ttk.Button(side,text=text,command=cmd).grid(row=row,column=0,sticky="ew",pady=3)
        main=ttk.Frame(self,padding=(0,10,10,10)); main.grid(row=0,column=1,sticky="nsew"); main.grid_rowconfigure(1,weight=1); main.grid_columnconfigure(0,weight=1); self.header=ttk.Label(main,text="",font=("TkDefaultFont",13,"bold")); self.header.grid(row=0,column=0,sticky="w",pady=(0,8))
        body=ttk.Frame(main); body.grid(row=1,column=0,sticky="nsew"); body.grid_rowconfigure(0,weight=1); body.grid_columnconfigure(0,weight=1); self.output=tk.Text(body,wrap="word",state="disabled",padx=16,pady=14,font=("TkDefaultFont",11),undo=False); self.output.grid(row=0,column=0,sticky="nsew"); scroll=ttk.Scrollbar(body,command=self.output.yview); scroll.grid(row=0,column=1,sticky="ns"); self.output.configure(yscrollcommand=scroll.set)
        bottom=ttk.Frame(main); bottom.grid(row=2,column=0,sticky="ew",pady=(8,0)); bottom.grid_columnconfigure(0,weight=1); self.entry=tk.Text(bottom,height=3,wrap="word",font=("TkDefaultFont",11)); self.entry.grid(row=0,column=0,sticky="ew"); self.entry.bind("<Control-Return>",lambda _:self._send()); ttk.Button(bottom,text="Send",command=self._send).grid(row=0,column=1,sticky="ns",padx=(8,0)); ttk.Label(main,text="Ctrl+Enter to send • chats and settings are saved locally",foreground="gray").grid(row=3,column=0,sticky="w",pady=(5,0))
    def _chat(self):return self.store.chats[self.current]
    def _provider(self):return make_provider(self.settings.model,self.settings.provider)
    def _refresh_chats(self):
        names=list(self.store.chats); self.chat_list.delete(0,"end")
        for name in names:self.chat_list.insert("end",name)
        if self.current in names:self.chat_list.selection_set(names.index(self.current)); self.chat_list.see(names.index(self.current))
    def _select_chat(self,_=None):
        sel=self.chat_list.curselection()
        if sel:self.current=list(self.store.chats)[sel[0]]; self.settings.current_chat=self.current; self.settings.save(); self._show_chat()
    def _show_chat(self):
        chat=self._chat(); self.title(f"{APP_NAME} — {chat.ai_name}"); self.header.config(text=f"{chat.ai_name}  ·  {self.current}  ·  {self.settings.model}"); self.output.config(state="normal"); self.output.delete("1.0","end")
        if not chat.messages:self.output.insert("end","Start a conversation with Vaxx.\n")
        for m in chat.messages:
            if m.get("role")=="system":continue
            who="You" if m["role"]=="user" else chat.ai_name; self.output.insert("end",f"{who}\n","role"); self.output.insert("end",render(m["content"])+"\n\n")
        self.output.config(state="disabled"); self.output.see("end")
    def _append(self,who,text):
        self.output.config(state="normal"); self.output.insert("end",f"{who}\n","role"); self.output.insert("end",render(text)+"\n\n"); self.output.config(state="disabled"); self.output.see("end")
    def _send(self):
        if self._busy:return
        text=self.entry.get("1.0","end").strip()
        if not text:return
        self.entry.delete("1.0","end"); chat=self._chat(); chat.provider=self._provider(); self._append("You",text); self._busy=True; self.entry.config(state="disabled"); threading.Thread(target=self._request,args=(chat,text),daemon=True).start()
    def _request(self,chat,text):
        try:
            answer=chat.send(text)
            if self.settings.auto_memory:chat.auto_remember(text)
            if self.settings.auto_summary:chat.maybe_summarize()
            self.store.save(self.current,chat); self.after(0,lambda:self._finish(answer,None))
        except Exception as exc:self.after(0,lambda:self._finish("",exc))
    def _finish(self,answer,error):
        self._append(self._chat().ai_name,f"Error: {error}" if error else answer); self._busy=False; self.entry.config(state="normal"); self.entry.focus_set()
    def _new_chat(self):
        name=simpledialog.askstring("New chat","Chat name:",parent=self); key=safe_name(name or "")
        if key and key not in self.store.chats:self.store.chats[key]=Chat(self._provider()); self.store.save(key,self.store.chats[key]); self.current=key; self.settings.current_chat=key; self.settings.save(); self._refresh_chats(); self._show_chat()
    def _rename_chat(self):
        name=simpledialog.askstring("Rename chat","New name:",initialvalue=self.current,parent=self); key=safe_name(name or "")
        if not key or key==self.current or key in self.store.chats:return
        old=self.current; chat=self.store.chats[old]; self.store.rename(old,key); self.current=key; self.settings.current_chat=key; self.settings.save(); self._refresh_chats(); self._show_chat()
    def _delete_chat(self):
        if len(self.store.chats)<=1:return
        if not messagebox.askyesno("Delete chat",f"Delete '{self.current}'?",parent=self):return
        self.store.delete(self.current); self.current=next(iter(self.store.chats)); self.settings.current_chat=self.current; self.settings.save(); self._refresh_chats(); self._show_chat()
    def _show_memory(self):
        memories=self._chat().memories; message="No saved memories." if not memories else "\n".join(f"{i}. {m}" for i,m in enumerate(memories,1)); messagebox.showinfo("Memory",message,parent=self)
    def _settings(self):
        model=simpledialog.askstring("Model","Model:",initialvalue=self.settings.model,parent=self); self.settings.model=model.strip() if model else self.settings.model; provider=simpledialog.askstring("Provider","Provider (huggingface/openai):",initialvalue=self.settings.provider,parent=self); self.settings.provider=provider if provider in {"huggingface","openai"} else self.settings.provider; name=simpledialog.askstring("AI name","AI name:",initialvalue=self._chat().ai_name,parent=self); self._chat().ai_name=name.strip()[:40] if name else self._chat().ai_name; self.settings.save(); self.store.save(self.current,self._chat()); self._show_chat()
    def _quit(self):
        try:self.store.save(self.current,self._chat()); self.settings.current_chat=self.current; self.settings.save()
        finally:self.destroy()
def main():VaxxApp().mainloop()
if __name__=="__main__":main()
