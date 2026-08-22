"""Lightweight desktop UI for Tiny AI Playground."""
from __future__ import annotations

import pathlib
import re
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
ACCENTS = {
    "default": ("#2563eb", "#eaf2ff"),
    "cyan": ("#0891b2", "#e6faff"),
    "green": ("#16a34a", "#eaf8ee"),
    "magenta": ("#c026d3", "#fbeafa"),
}
PALETTES = {
    "light": {"bg":"#f5f7fb","panel":"#ffffff","input":"#ffffff","text":"#1f2937","muted":"#6b7280","border":"#dbe2ea","accent":"#2563eb","accent_fg":"#ffffff"},
    "dark": {"bg":"#111318","panel":"#181b22","input":"#20242d","text":"#f2f4f7","muted":"#9aa3b2","border":"#2a303b","accent":"#5b8cff","accent_fg":"#ffffff"},
}

class VaxxApp(tk.Tk):
    def __init__(self):
        super().__init__(className=APP_CLASS)
        self.settings = Settings.load()
        self.store = SessionStore.load(lambda _: make_provider(self.settings.model, self.settings.provider))
        self.current = self.settings.current_chat if self.settings.current_chat in self.store.chats else next(iter(self.store.chats))
        self._busy = False; self.sidebar_open = True
        self._load_icon(); self.title(f"{APP_NAME} — {self.store.chats[self.current].ai_name}"); self.geometry("1080x720"); self.minsize(800,560)
        self._apply_theme(); self._build(); self._refresh_chats(); self._show_chat(); self.protocol("WM_DELETE_WINDOW", self._quit)

    def _load_icon(self):
        icon = pathlib.Path(__file__).resolve().parent.parent / "icons" / "Temp app icon.png"
        if icon.exists():
            try: self._app_icon = tk.PhotoImage(file=str(icon)); self.iconphoto(True, self._app_icon)
            except tk.TclError: pass

    def _font(self,size=11,bold=False):
        family = "Noto Sans" if "Noto Sans" in self.tk.call("font","families") else "TkDefaultFont"
        return (family,size,"bold") if bold else (family,size)

    def _apply_theme(self):
        appearance = effective_appearance(self.settings.appearance); self.palette = dict(PALETTES[appearance]); self.palette["accent"] = ACCENTS.get(self.settings.theme, ACCENTS["default"])[0]
        self.configure(bg=self.palette["bg"]); style = ttk.Style(self); style.theme_use("clam")
        style.configure("TFrame", background=self.palette["bg"]); style.configure("Panel.TFrame", background=self.palette["panel"])
        style.configure("TLabel", background=self.palette["bg"], foreground=self.palette["text"], font=self._font()); style.configure("Panel.TLabel", background=self.palette["panel"], foreground=self.palette["text"], font=self._font())
        style.configure("TButton", background=self.palette["input"], foreground=self.palette["text"], padding=(12,7), borderwidth=0, relief="flat"); style.map("TButton", background=[("active",self.palette["border"]),("pressed",self.palette["border"])])
        style.configure("Accent.TButton", background=self.palette["accent"], foreground="#ffffff", padding=(14,8), borderwidth=0, relief="flat", font=self._font(10,True)); style.map("Accent.TButton", background=[("active",self.palette["accent"]),("pressed",self.palette["accent"])])
        style.configure("Icon.TButton", background=self.palette["panel"], foreground=self.palette["text"], padding=(8,6), borderwidth=0, relief="flat", font=self._font(12,True)); style.configure("TCombobox", fieldbackground=self.palette["input"], background=self.palette["input"], foreground=self.palette["text"]); style.configure("Settings.TCombobox", fieldbackground=self.palette["input"], background=self.palette["input"], foreground=self.palette["text"]); style.configure("TCheckbutton", background=self.palette["panel"], foreground=self.palette["text"])

    def _retheme_widgets(self): self._apply_theme(); self._build(); self._refresh_chats(); self._show_chat()

    def _build(self):
        for child in self.winfo_children(): child.destroy()
        self.grid_columnconfigure(0,weight=0,minsize=0); self.grid_columnconfigure(1,weight=1); self.grid_rowconfigure(0,weight=1)
        self.side = ttk.Frame(self,style="Panel.TFrame",padding=12); self.side.grid(row=0,column=0,sticky="nsew",padx=(10,6),pady=10); self.side.grid_rowconfigure(3,weight=1); self.side.grid_columnconfigure(0,weight=1)
        head=ttk.Frame(self.side,style="Panel.TFrame"); head.grid(row=0,column=0,sticky="ew",pady=(0,2)); head.grid_columnconfigure(0,weight=1); ttk.Label(head,text=APP_NAME,font=self._font(18,True),style="Panel.TLabel").grid(row=0,column=0,sticky="w"); ttk.Button(head,text="‹",style="Icon.TButton",command=self._toggle_sidebar).grid(row=0,column=1,sticky="e")
        ttk.Label(self.side,text="Lightweight • private local state",font=self._font(9),style="Panel.TLabel").grid(row=1,column=0,sticky="w",pady=(0,10)); ttk.Button(self.side,text="＋  New chat",style="Accent.TButton",command=self._new_chat).grid(row=2,column=0,sticky="ew",pady=(0,8))
        self.chat_list=tk.Listbox(self.side,exportselection=False,activestyle="none",font=self._font(),bg=self.palette["panel"],fg=self.palette["text"],selectbackground=self.palette["accent"],selectforeground="#ffffff",highlightthickness=1,highlightbackground=self.palette["border"],relief="flat",bd=0); self.chat_list.grid(row=3,column=0,sticky="nsew",pady=(2,8)); self.chat_list.bind("<<ListboxSelect>>",self._select_chat)
        for row,text,cmd in [(4,"Rename chat",self._rename_chat),(5,"Delete chat",self._delete_chat),(6,"Memory",self._show_memory),(7,"Settings",self._settings)]: ttk.Button(self.side,text=text,command=cmd).grid(row=row,column=0,sticky="ew",pady=3)
        self.main=ttk.Frame(self,padding=(6,10,12,10)); self.main.grid(row=0,column=1,sticky="nsew"); self.main.grid_rowconfigure(1,weight=1); self.main.grid_columnconfigure(0,weight=1)
        top=ttk.Frame(self.main); top.grid(row=0,column=0,sticky="ew",pady=(0,8)); top.grid_columnconfigure(0,weight=1); ttk.Label(top,text=f"{self._chat().ai_name}  ·  {self.current}",font=self._font(14,True),style="TLabel").grid(row=0,column=0,sticky="w"); ttk.Button(top,text="☰",style="Icon.TButton",command=self._toggle_sidebar).grid(row=0,column=1,sticky="e")
        body=tk.Frame(self.main,bg=self.palette["panel"],highlightbackground=self.palette["border"],highlightthickness=1,bd=0); body.grid(row=1,column=0,sticky="nsew"); body.grid_rowconfigure(0,weight=1); body.grid_columnconfigure(0,weight=1)
        self.output=tk.Text(body,wrap="word",state="normal",padx=18,pady=16,font=self._font(),bg=self.palette["panel"],fg=self.palette["text"],insertbackground=self.palette["text"],selectbackground=self.palette["accent"],relief="flat",bd=0,spacing3=5); self.output.grid(row=0,column=0,sticky="nsew"); self.output.bind("<Key>",self._block_chat_edit); self.output.bind("<Control-c>",self._copy_selection); self.output.bind("<Control-a>",self._select_all_output); self.output.bind("<Button-3>",self._chat_context_menu)
        scroll=ttk.Scrollbar(body,command=self.output.yview); scroll.grid(row=0,column=1,sticky="ns"); self.output.configure(yscrollcommand=scroll.set)
        bottom=tk.Frame(self.main,bg=self.palette["bg"]); bottom.grid(row=2,column=0,sticky="ew",pady=(10,0)); bottom.grid_columnconfigure(0,weight=1); input_frame=tk.Frame(bottom,bg=self.palette["input"],highlightbackground=self.palette["border"],highlightthickness=1,bd=0); input_frame.grid(row=0,column=0,sticky="ew"); input_frame.grid_columnconfigure(0,weight=1)
        self.entry=tk.Text(input_frame,height=3,wrap="word",font=self._font(),bg=self.palette["input"],fg=self.palette["text"],insertbackground=self.palette["text"],relief="flat",bd=0,padx=10,pady=8); self.entry.grid(row=0,column=0,sticky="ew"); self.entry.bind("<Control-Return>",lambda _:self._send()); ttk.Button(bottom,text="Send",style="Accent.TButton",command=self._send).grid(row=0,column=1,sticky="ns",padx=(8,0)); ttk.Label(self.main,text="Ctrl+Enter to send • right-click chat text to copy",foreground=self.palette["muted"],style="TLabel",font=self._font(9)).grid(row=3,column=0,sticky="w",pady=(6,0)); self._update_sidebar_visibility()

    def _block_chat_edit(self,event):
        if event.state & 0x4 and event.keysym.lower() in {"c","a"}: return None
        if event.keysym in {"Left","Right","Up","Down","Home","End","Prior","Next","Shift_L","Shift_R","Control_L","Control_R","Alt_L","Alt_R"}: return None
        return "break"
    def _copy_selection(self,_event=None):
        try: selected=self.output.get("sel.first","sel.last")
        except tk.TclError: return "break"
        self.clipboard_clear(); self.clipboard_append(selected); self.update(); return "break"
    def _select_all_output(self,_event=None): self.output.tag_add("sel","1.0","end-1c"); return "break"
    def _chat_context_menu(self,event):
        menu=tk.Menu(self,tearoff=False,bg=self.palette["panel"],fg=self.palette["text"]); menu.add_command(label="Copy",command=self._copy_selection); menu.add_command(label="Select all",command=self._select_all_output); menu.tk_popup(event.x_root,event.y_root)
    def _toggle_sidebar(self): self.sidebar_open=not self.sidebar_open; self._update_sidebar_visibility()
    def _update_sidebar_visibility(self):
        if self.sidebar_open: self.side.grid(); self.grid_columnconfigure(0,minsize=255)
        else: self.side.grid_remove(); self.grid_columnconfigure(0,minsize=0)

    def _insert_markdown(self,text):
        """Insert a rendered line into Tk Text with lightweight Markdown emphasis."""
        pattern=re.compile(r"(\*\*.+?\*\*|__.+?__|`[^`]+`|(?<!\*)\*[^*\n]+\*(?!\*)|(?<!\w)_[^_\n]+_(?!\w))")
        cursor=0
        for match in pattern.finditer(text):
            if match.start()>cursor:self.output.insert("end",text[cursor:match.start()])
            token=match.group(0)
            if token.startswith(("**","__")):
                self.output.insert("end",token[2:-2],"md_bold")
            elif token.startswith("`"):
                self.output.insert("end",token[1:-1],"md_code")
            else:
                self.output.insert("end",token[1:-1],"md_italic")
            cursor=match.end()
        if cursor<len(text):self.output.insert("end",text[cursor:])

    def _insert_rich(self,text):
        """Render Unicode/Markdown while keeping fenced code readable."""
        rendered=render(text)
        lines=rendered.splitlines()
        in_code=False
        for line in lines:
            if line.strip().startswith("```") or line.strip().startswith("~~~"):
                in_code=not in_code
                continue
            if in_code:
                self.output.insert("end",line+"\n","md_code")
                continue
            heading=re.match(r"^\s*(#{1,6})\s+(.*)$",line)
            if heading:
                self.output.insert("end",heading.group(2)+"\n","md_heading"); continue
            self._insert_markdown(line); self.output.insert("end","\n")

    def _style_markdown(self):
        self.output.tag_config("md_heading",foreground=self.palette["text"],font=self._font(13,True),spacing1=7,spacing3=5)
        self.output.tag_config("md_bold",foreground=self.palette["text"],font=self._font(11,True))
        self.output.tag_config("md_italic",foreground=self.palette["text"],font=("Noto Sans" if "Noto Sans" in self.tk.call("font","families") else "TkDefaultFont",11,"italic"))
        self.output.tag_config("md_code",foreground=self.palette["accent"],background=self.palette["input"],font=("DejaVu Sans Mono" if "DejaVu Sans Mono" in self.tk.call("font","families") else "TkFixedFont",10))
        self.output.tag_config("role_you",foreground=self.palette["accent"],font=self._font(10,True)); self.output.tag_config("role_ai",foreground=self.palette["text"],font=self._font(10,True))

    def _show_chat(self):
        chat=self._chat()
        if not hasattr(self,"output"):return
        self.title(f"{APP_NAME} — {chat.ai_name}"); self.output.delete("1.0","end"); self._style_markdown()
        if not chat.messages:self.output.insert("end","Start a conversation with Vaxx.\n")
        for message in chat.messages:
            if message.get("role")=="system":continue
            who="You" if message["role"]=="user" else chat.ai_name
            self.output.insert("end",f"{who}\n","role_you" if who=="You" else "role_ai"); self._insert_rich(message["content"]); self.output.insert("end","\n")
        self.output.see("end")

    def _append(self,who,text):
        self.output.insert("end",f"{who}\n","role_you" if who=="You" else "role_ai"); self._insert_rich(text); self.output.insert("end","\n"); self.output.see("end")

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
        win=tk.Toplevel(self); win.title("Settings"); win.transient(self); win.resizable(False,False); win.configure(bg=self.palette["bg"])
        frame=tk.Frame(win,bg=self.palette["panel"],highlightbackground=self.palette["border"],highlightthickness=1,padx=22,pady=18); frame.grid(row=0,column=0,padx=14,pady=14); frame.columnconfigure(1,weight=1)
        def label(text,row):tk.Label(frame,text=text,bg=self.palette["panel"],fg=self.palette["text"],font=self._font(10,True)).grid(row=row,column=0,sticky="w",padx=(0,20),pady=7)
        tk.Label(frame,text="AI Chat Settings",bg=self.palette["panel"],fg=self.palette["text"],font=self._font(16,True)).grid(row=0,column=0,columnspan=2,sticky="w",pady=(0,14))
        name_var=tk.StringVar(value=self._chat().ai_name); label("AI name",1); tk.Entry(frame,textvariable=name_var,width=44,bg=self.palette["input"],fg=self.palette["text"],insertbackground=self.palette["text"],relief="flat").grid(row=1,column=1,sticky="ew",pady=7)
        provider_var=tk.StringVar(value=self.settings.provider); label("API provider",2); ttk.Combobox(frame,textvariable=provider_var,state="readonly",values=["huggingface","openai"],width=41,style="Settings.TCombobox").grid(row=2,column=1,sticky="ew",pady=7)
        model_values=list(MODELS); model_values += [] if self.settings.model in model_values else [self.settings.model]; model_var=tk.StringVar(value=self.settings.model); label("Model",3); ttk.Combobox(frame,textvariable=model_var,state="readonly",values=model_values,width=41,style="Settings.TCombobox").grid(row=3,column=1,sticky="ew",pady=7)
        appearance_var=tk.StringVar(value=self.settings.appearance); label("Appearance",4); ttk.Combobox(frame,textvariable=appearance_var,state="readonly",values=list(APPEARANCES),width=41,style="Settings.TCombobox").grid(row=4,column=1,sticky="ew",pady=7)
        theme_var=tk.StringVar(value=self.settings.theme); label("Accent theme",5); ttk.Combobox(frame,textvariable=theme_var,state="readonly",values=list(THEMES),width=41,style="Settings.TCombobox").grid(row=5,column=1,sticky="ew",pady=7)
        stream_var=tk.BooleanVar(value=self.settings.stream); memory_var=tk.BooleanVar(value=self.settings.auto_memory); summary_var=tk.BooleanVar(value=self.settings.auto_summary)
        for row,text,var in [(6,"Enable streaming responses",stream_var),(7,"Automatically remember simple personal facts",memory_var),(8,"Automatically summarize long conversations",summary_var)]:tk.Checkbutton(frame,text=text,variable=var,bg=self.palette["panel"],fg=self.palette["text"],activebackground=self.palette["panel"],activeforeground=self.palette["text"],selectcolor=self.palette["input"],font=self._font()).grid(row=row,column=1,sticky="w",pady=4)
        buttons=tk.Frame(frame,bg=self.palette["panel"]); buttons.grid(row=10,column=0,columnspan=2,sticky="e",pady=(14,0)); ttk.Button(buttons,text="Cancel",command=win.destroy).grid(row=0,column=0,padx=(0,8))
        def save_settings():
            name=name_var.get().strip()
            if name:self._chat().ai_name=name[:40]
            self.settings.provider=provider_var.get(); self.settings.model=model_var.get(); self.settings.appearance=appearance_var.get(); self.settings.theme=theme_var.get(); self.settings.stream=stream_var.get(); self.settings.auto_memory=memory_var.get(); self.settings.auto_summary=summary_var.get(); self.settings.save(); self._chat().provider=self._provider(); self.store.save(self.current,self._chat()); win.destroy(); self._retheme_widgets()
        ttk.Button(buttons,text="Save",style="Accent.TButton",command=save_settings).grid(row=0,column=1)
        win.bind("<Escape>",lambda _:win.destroy()); win.bind("<Return>",lambda _:save_settings()); win.update_idletasks(); x=self.winfo_rootx()+(self.winfo_width()-win.winfo_width())//2; y=self.winfo_rooty()+(self.winfo_height()-win.winfo_height())//2; win.geometry(f"+{x}+{y}"); win.after_idle(win.grab_set); win.after_idle(win.focus_force)

    def _quit(self):
        try:self.store.save(self.current,self._chat()); self.settings.current_chat=self.current; self.settings.save()
        finally:self.destroy()


def main():VaxxApp().mainloop()
if __name__=="__main__":main()
