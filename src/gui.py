"""Lightweight desktop UI for Tiny AI Playground."""
from __future__ import annotations

import pathlib
import threading
import tkinter as tk
from tkinter import messagebox, simpledialog, ttk

from .chat import Chat
from .config import MODELS
from .providers import make_provider
from .sessions import SessionStore, safe_name
from .settings import APPEARANCES, THEMES, Settings, effective_appearance
from .terminal_render import render

APP_NAME = "AI Chat"
APP_CLASS = "AIChat"
ACCENTS = {"default": ("#2563eb", "#eaf2ff"), "cyan": ("#0891b2", "#e6faff"), "green": ("#16a34a", "#eaf8ee"), "magenta": ("#c026d3", "#fbeafa")}

PALETTES = {
    "light": {"bg": "#f5f7fb", "panel": "#ffffff", "input": "#ffffff", "text": "#1f2937", "muted": "#6b7280", "border": "#dbe2ea", "accent": "#2563eb", "accent_fg": "#ffffff", "user_bg": "#eaf2ff", "ai_bg": "#ffffff"},
    "dark": {"bg": "#111318", "panel": "#181b22", "input": "#20242d", "text": "#f2f4f7", "muted": "#9aa3b2", "border": "#2a303b", "accent": "#5b8cff", "accent_fg": "#ffffff", "user_bg": "#1d2a40", "ai_bg": "#181b22"},
}


def _rounded_points(x1, y1, x2, y2, r):
    return [x1+r, y1, x2-r, y1, x2, y1+r, x2, y2-r, x2-r, y2, x1+r, y2, x1, y2-r, x1, y1+r]


class RoundedButton(tk.Canvas):
    def __init__(self, master, text, command, palette, accent=False, **kwargs):
        super().__init__(master, height=38, highlightthickness=0, bd=0, bg=palette["panel"], **kwargs)
        self.command = command; self.text = text; self.palette = palette; self.accent = accent; self.bind("<Button-1>", self._click); self.bind("<Enter>", lambda _: self._draw(True)); self.bind("<Leave>", lambda _: self._draw(False)); self.bind("<Configure>", lambda _: self._draw(False)); self._draw(False)
    def _draw(self, hover):
        self.delete("all"); w=max(80,self.winfo_width()); h=max(30,self.winfo_height()); fill=self.palette["accent"] if self.accent else self.palette["input"]; fg=self.palette["accent_fg"] if self.accent else self.palette["text"]; border=self.palette["accent"] if hover else self.palette["border"]
        self.create_polygon(_rounded_points(1,1,w-1,h-1,11), smooth=True, fill=fill, outline=border, width=1); self.create_text(w/2,h/2,text=self.text,fill=fg,font=("Noto Sans" if self._font_available() else "TkDefaultFont",10,"bold" if self.accent else "normal"))
    def _font_available(self): return "Noto Sans" in self.winfo_toplevel().tk.call("font","families")
    def _click(self, _):
        if self.command:self.command()


class VaxxApp(tk.Tk):
    def __init__(self):
        super().__init__(className=APP_CLASS)
        self.settings=Settings.load(); self.store=SessionStore.load(lambda _:make_provider(self.settings.model,self.settings.provider)); self.current=self.settings.current_chat if self.settings.current_chat in self.store.chats else next(iter(self.store.chats)); self._busy=False
        self._load_icon(); self.title(f"{APP_NAME} — {self.store.chats[self.current].ai_name}"); self.geometry("1080x720"); self.minsize(800,560); self._apply_theme(); self._build(); self._refresh_chats(); self._show_chat(); self.protocol("WM_DELETE_WINDOW",self._quit)
    def _load_icon(self):
        icon=pathlib.Path(__file__).resolve().parent.parent/"icons"/"Temp app icon.png"
        if icon.exists():
            try:self._app_icon=tk.PhotoImage(file=str(icon)); self.iconphoto(True,self._app_icon)
            except tk.TclError:pass
    def _font(self,size=11,bold=False):
        family="Noto Sans" if "Noto Sans" in self.tk.call("font","families") else "TkDefaultFont"; return (family,size,"bold") if bold else (family,size)
    def _apply_theme(self):
        appearance=effective_appearance(self.settings.appearance); self.palette=dict(PALETTES[appearance]); accent,soft=ACCENTS.get(self.settings.theme,ACCENTS["default"]); self.palette["accent"]=accent; self.palette["user_bg"]=soft; self.palette["accent_fg"]="#ffffff"
        self.configure(bg=self.palette["bg"]); style=ttk.Style(self); style.theme_use("clam")
        style.configure("TFrame",background=self.palette["bg"]); style.configure("Panel.TFrame",background=self.palette["panel"]); style.configure("TLabel",background=self.palette["bg"],foreground=self.palette["text"],font=self._font()); style.configure("Panel.TLabel",background=self.palette["panel"],foreground=self.palette["text"],font=self._font()); style.configure("TLabelframe",background=self.palette["panel"],foreground=self.palette["text"]); style.configure("TCheckbutton",background=self.palette["panel"],foreground=self.palette["text"]); style.configure("TCombobox",fieldbackground=self.palette["input"],background=self.palette["input"],foreground=self.palette["text"])
    def _retheme_widgets(self): self._apply_theme(); self._build(True); self._refresh_chats(); self._show_chat()
    def _build(self, rebuild=False):
        for child in self.winfo_children(): child.destroy()
        self.grid_columnconfigure(0,weight=0); self.grid_columnconfigure(1,weight=1); self.grid_rowconfigure(0,weight=1)
        side=ttk.Frame(self,style="Panel.TFrame",padding=14); side.grid(row=0,column=0,sticky="nsew",padx=(12,7),pady=12); side.grid_rowconfigure(2,weight=1)
        ttk.Label(side,text=APP_NAME,font=self._font(19,True),style="Panel.TLabel").grid(row=0,column=0,sticky="w"); ttk.Label(side,text="Lightweight • private local state",font=self._font(9),style="Panel.TLabel").grid(row=0,column=0,sticky="w",pady=(34,0))
        RoundedButton(side,"＋ New chat",self._new_chat,self.palette,accent=True).grid(row=1,column=0,sticky="ew",pady=(16,8))
        self.chat_list=tk.Listbox(side,exportselection=False,activestyle="none",font=self._font(),bg=self.palette["panel"],fg=self.palette["text"],selectbackground=self.palette["accent"],selectforeground=self.palette["accent_fg"],highlightthickness=1,highlightbackground=self.palette["border"],relief="flat",bd=0); self.chat_list.grid(row=2,column=0,sticky="nsew",pady=(2,8)); self.chat_list.bind("<<ListboxSelect>>",self._select_chat)
        for row,text,cmd in [(3,"Rename chat",self._rename_chat),(4,"Delete chat",self._delete_chat),(5,"Memory",self._show_memory),(6,"Settings",self._settings)]: RoundedButton(side,text,cmd,self.palette).grid(row=row,column=0,sticky="ew",pady=3)
        main=ttk.Frame(self,padding=(8,12,12,12)); main.grid(row=0,column=1,sticky="nsew"); main.grid_rowconfigure(1,weight=1); main.grid_columnconfigure(0,weight=1)
        ttk.Label(main,text=f"{self._chat().ai_name}  ·  {self.current}",font=self._font(14,True),style="TLabel").grid(row=0,column=0,sticky="w",pady=(0,10))
        body=tk.Frame(main,bg=self.palette["panel"],highlightbackground=self.palette["border"],highlightthickness=1,bd=0); body.grid(row=1,column=0,sticky="nsew"); body.grid_rowconfigure(0,weight=1); body.grid_columnconfigure(0,weight=1)
        self.output=tk.Text(body,wrap="word",state="disabled",padx=18,pady=16,font=self._font(),bg=self.palette["panel"],fg=self.palette["text"],insertbackground=self.palette["text"],selectbackground=self.palette["accent"],relief="flat",bd=0,spacing3=5); self.output.grid(row=0,column=0,sticky="nsew"); scroll=ttk.Scrollbar(body,command=self.output.yview); scroll.grid(row=0,column=1,sticky="ns"); self.output.configure(yscrollcommand=scroll.set)
        bottom=tk.Frame(main,bg=self.palette["bg"]); bottom.grid(row=2,column=0,sticky="ew",pady=(10,0)); bottom.grid_columnconfigure(0,weight=1); input_frame=tk.Frame(bottom,bg=self.palette["input"],highlightbackground=self.palette["border"],highlightthickness=1,bd=0); input_frame.grid(row=0,column=0,sticky="ew"); input_frame.grid_columnconfigure(0,weight=1)
        self.entry=tk.Text(input_frame,height=3,wrap="word",font=self._font(),bg=self.palette["input"],fg=self.palette["text"],insertbackground=self.palette["text"],relief="flat",bd=0,padx=10,pady=8); self.entry.grid(row=0,column=0,sticky="ew"); self.entry.bind("<Control-Return>",lambda _:self._send()); RoundedButton(bottom,"Send",self._send,self.palette,accent=True).grid(row=0,column=1,sticky="ns",padx=(8,0)); ttk.Label(main,text="Ctrl+Enter to send • chats and settings save locally",foreground=self.palette["muted"],style="TLabel",font=self._font(9)).grid(row=3,column=0,sticky="w",pady=(6,0))
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
        chat=self._chat();
        if not hasattr(self,"output"):return
        self.title(f"{APP_NAME} — {chat.ai_name}"); self.output.config(state="normal"); self.output.delete("1.0","end")
        if not chat.messages:self.output.insert("end","Start a conversation with Vaxx.\n")
        for m in chat.messages:
            if m.get("role")=="system":continue
            who="You" if m["role"]=="user" else chat.ai_name; self.output.insert("end",f"{who}\n",("role_you" if who=="You" else "role_ai")); self.output.insert("end",render(m["content"])+"\n\n")
        self.output.tag_config("role_you",foreground=self.palette["accent"],font=self._font(10,True)); self.output.tag_config("role_ai",foreground=self.palette["text"],font=self._font(10,True)); self.output.config(state="disabled"); self.output.see("end")
    def _append(self,who,text): self.output.config(state="normal"); self.output.insert("end",f"{who}\n",("role_you" if who=="You" else "role_ai")); self.output.insert("end",render(text)+"\n\n"); self.output.config(state="disabled"); self.output.see("end")
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
    def _finish(self,answer,error): self._append(self._chat().ai_name,f"Error: {error}" if error else answer); self._busy=False; self.entry.config(state="normal"); self.entry.focus_set()
    def _new_chat(self):
        name=simpledialog.askstring("New chat","Chat name:",parent=self); key=safe_name(name or "")
        if key and key not in self.store.chats:self.store.chats[key]=Chat(self._provider()); self.store.save(key,self.store.chats[key]); self.current=key; self.settings.current_chat=key; self.settings.save(); self._refresh_chats(); self._show_chat()
    def _rename_chat(self):
        name=simpledialog.askstring("Rename chat","New name:",initialvalue=self.current,parent=self); key=safe_name(name or "")
        if not key or key==self.current or key in self.store.chats:return
        self.store.rename(self.current,key); self.current=key; self.settings.current_chat=key; self.settings.save(); self._refresh_chats(); self._show_chat()
    def _delete_chat(self):
        if len(self.store.chats)<=1:return
        if not messagebox.askyesno("Delete chat",f"Delete '{self.current}'?",parent=self):return
        self.store.delete(self.current); self.current=next(iter(self.store.chats)); self.settings.current_chat=self.current; self.settings.save(); self._refresh_chats(); self._show_chat()
    def _show_memory(self):
        memories=self._chat().memories; message="No saved memories." if not memories else "\n".join(f"{i}. {m}" for i,m in enumerate(memories,1)); messagebox.showinfo("Memory",message,parent=self)
    def _settings(self):
        win=tk.Toplevel(self); win.title("Settings"); win.transient(self); win.grab_set(); win.resizable(False,False); win.configure(bg=self.palette["bg"]); frame=tk.Frame(win,bg=self.palette["panel"],highlightbackground=self.palette["border"],highlightthickness=1,padx=22,pady=18); frame.grid(row=0,column=0,padx=14,pady=14)
        label=lambda text,row: tk.Label(frame,text=text,bg=self.palette["panel"],fg=self.palette["text"],font=self._font(10,True)).grid(row=row,column=0,sticky="w",padx=(0,20),pady=7); ttk.Style(win).configure("Settings.TCombobox",fieldbackground=self.palette["input"],background=self.palette["input"],foreground=self.palette["text"])
        tk.Label(frame,text="AI Chat Settings",bg=self.palette["panel"],fg=self.palette["text"],font=self._font(16,True)).grid(row=0,column=0,columnspan=2,sticky="w",pady=(0,14)); label("AI name",1)
        name_var=tk.StringVar(value=self._chat().ai_name); tk.Entry(frame,textvariable=name_var,width=44,bg=self.palette["input"],fg=self.palette["text"],insertbackground=self.palette["text"],relief="flat").grid(row=1,column=1,sticky="ew",pady=7)
        provider_var=tk.StringVar(value=self.settings.provider); label("API provider",2); ttk.Combobox(frame,textvariable=provider_var,state="readonly",values=["huggingface","openai"],width=41,style="Settings.TCombobox").grid(row=2,column=1,sticky="ew",pady=7)
        model_values=list(MODELS); model_values += [] if self.settings.model in model_values else [self.settings.model]; model_var=tk.StringVar(value=self.settings.model); label("Model",3); ttk.Combobox(frame,textvariable=model_var,state="readonly",values=model_values,width=41,style="Settings.TCombobox").grid(row=3,column=1,sticky="ew",pady=7)
        appearance_var=tk.StringVar(value=self.settings.appearance); label("Appearance",4); ttk.Combobox(frame,textvariable=appearance_var,state="readonly",values=list(APPEARANCES),width=41,style="Settings.TCombobox").grid(row=4,column=1,sticky="ew",pady=7)
        theme_var=tk.StringVar(value=self.settings.theme); label("Accent theme",5); ttk.Combobox(frame,textvariable=theme_var,state="readonly",values=list(THEMES),width=41,style="Settings.TCombobox").grid(row=5,column=1,sticky="ew",pady=7)
        stream_var=tk.BooleanVar(value=self.settings.stream); memory_var=tk.BooleanVar(value=self.settings.auto_memory); summary_var=tk.BooleanVar(value=self.settings.auto_summary)
        for row,text,var in [(6,"Enable streaming responses",stream_var),(7,"Automatically remember simple personal facts",memory_var),(8,"Automatically summarize long conversations",summary_var)]: tk.Checkbutton(frame,text=text,variable=var,bg=self.palette["panel"],fg=self.palette["text"],activebackground=self.palette["panel"],activeforeground=self.palette["text"],selectcolor=self.palette["input"],font=self._font()).grid(row=row,column=1,sticky="w",pady=4)
        def save_settings():
            name=name_var.get().strip();
            if name:self._chat().ai_name=name[:40]
            self.settings.provider=provider_var.get(); self.settings.model=model_var.get(); self.settings.appearance=appearance_var.get(); self.settings.theme=theme_var.get(); self.settings.stream=stream_var.get(); self.settings.auto_memory=memory_var.get(); self.settings.auto_summary=summary_var.get(); self.settings.save(); self._chat().provider=self._provider(); self.store.save(self.current,self._chat()); win.destroy(); self._retheme_widgets()
        buttons=tk.Frame(frame,bg=self.palette["panel"]); buttons.grid(row=10,column=0,columnspan=2,sticky="e",pady=(14,0)); RoundedButton(buttons,"Cancel",win.destroy,self.palette).grid(row=0,column=0,padx=(0,8)); RoundedButton(buttons,"Save",save_settings,self.palette,accent=True).grid(row=0,column=1)
        win.bind("<Escape>",lambda _:win.destroy()); win.bind("<Return>",lambda _:save_settings()); win.update_idletasks(); x=self.winfo_rootx()+(self.winfo_width()-win.winfo_width())//2; y=self.winfo_rooty()+(self.winfo_height()-win.winfo_height())//2; win.geometry(f"+{x}+{y}"); win.focus_force()
    def _quit(self):
        try:self.store.save(self.current,self._chat()); self.settings.current_chat=self.current; self.settings.save()
        finally:self.destroy()

def main():VaxxApp().mainloop()
if __name__=="__main__":main()
