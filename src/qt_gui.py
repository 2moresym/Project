from __future__ import annotations

import html
import pathlib
import re
import sys
import threading

from PySide6.QtCore import QEasingCurve, QPropertyAnimation, Qt, Signal, QObject
from PySide6.QtGui import QFont, QIcon, QTextCursor
from PySide6.QtWidgets import (
    QApplication,
    QCheckBox,
    QComboBox,
    QDialog,
    QFormLayout,
    QGraphicsDropShadowEffect,
    QHBoxLayout,
    QInputDialog,
    QLabel,
    QLineEdit,
    QListWidget,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QTextBrowser,
    QTextEdit,
    QToolButton,
    QVBoxLayout,
    QWidget,
)

from .chat import Chat
from .config import MODELS
from .providers import make_provider
from .sessions import SessionStore, safe_name
from .settings import APPEARANCES, THEMES, Settings, effective_appearance
from .terminal_render import render

APP_NAME = "AI Chat"
APP_CLASS = "AIChat"


class ReplyWorker(QObject):
    finished = Signal(str, object)

    def __init__(self, chat: Chat, text: str) -> None:
        super().__init__()
        self.chat = chat
        self.text = text

    def run(self) -> None:
        try:
            self.finished.emit(self.chat.send(self.text), None)
        except Exception as exc:  # provider errors stay inside the UI
            self.finished.emit("", exc)


class SettingsDialog(QDialog):
    saved = Signal(dict)

    def __init__(self, parent: "MainWindow") -> None:
        super().__init__(parent)
        self.setWindowTitle("Settings")
        self.setModal(True)
        self.setMinimumWidth(500)

        s = parent.settings
        form = QFormLayout(self)
        form.setContentsMargins(24, 20, 24, 20)
        form.setSpacing(12)

        self.name = QLineEdit(parent.chat.ai_name)
        self.provider = QComboBox()
        self.provider.addItems(["huggingface", "openai"])
        self.provider.setCurrentText(s.provider)

        self.model = QComboBox()
        models = list(MODELS)
        if s.model not in models:
            models.append(s.model)
        self.model.addItems(models)
        self.model.setCurrentText(s.model)

        self.appearance = QComboBox()
        self.appearance.addItems(list(APPEARANCES))
        self.appearance.setCurrentText(s.appearance)

        self.theme = QComboBox()
        self.theme.addItems(list(THEMES))
        self.theme.setCurrentText(s.theme)

        self.stream = QCheckBox("Enable streaming responses")
        self.stream.setChecked(s.stream)
        self.memory = QCheckBox("Automatically remember simple personal facts")
        self.memory.setChecked(s.auto_memory)
        self.summary = QCheckBox("Automatically summarize long conversations")
        self.summary.setChecked(s.auto_summary)

        form.addRow("AI name", self.name)
        form.addRow("API provider", self.provider)
        form.addRow("Model", self.model)
        form.addRow("Appearance", self.appearance)
        form.addRow("Accent theme", self.theme)
        form.addRow(self.stream)
        form.addRow(self.memory)
        form.addRow(self.summary)

        buttons = QHBoxLayout()
        buttons.addStretch()
        cancel = QPushButton("Cancel")
        cancel.clicked.connect(self.reject)
        save = QPushButton("Save")
        save.setObjectName("primary")
        save.clicked.connect(self._save)
        buttons.addWidget(cancel)
        buttons.addWidget(save)
        form.addRow(buttons)

    def _save(self) -> None:
        self.saved.emit(
            {
                "ai_name": self.name.text().strip(),
                "provider": self.provider.currentText(),
                "model": self.model.currentText(),
                "appearance": self.appearance.currentText(),
                "theme": self.theme.currentText(),
                "stream": self.stream.isChecked(),
                "auto_memory": self.memory.isChecked(),
                "auto_summary": self.summary.isChecked(),
            }
        )
        self.accept()


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.settings = Settings.load()
        self.store = SessionStore.load(
            lambda _: make_provider(self.settings.model, self.settings.provider)
        )
        self.current = (
            self.settings.current_chat
            if self.settings.current_chat in self.store.chats
            else next(iter(self.store.chats))
        )
        self.chat = self.store.chats[self.current]
        self.sidebar_width = 270
        self._worker_thread: threading.Thread | None = None
        self._reply_worker: ReplyWorker | None = None
        self._sidebar_animation: QPropertyAnimation | None = None

        icon = pathlib.Path(__file__).resolve().parent.parent / "icons" / "Temp app icon.png"
        if icon.exists():
            self.setWindowIcon(QIcon(str(icon)))

        self.setWindowTitle(f"{APP_NAME} — {self.chat.ai_name}")
        self.resize(1120, 740)
        self.setMinimumSize(820, 580)
        self._build()
        self._apply_theme()
        self._refresh_chats()
        self._show_chat()

    def _build(self) -> None:
        root = QWidget()
        layout = QHBoxLayout(root)
        layout.setContentsMargins(10, 10, 10, 10)
        layout.setSpacing(10)

        self.sidebar = QWidget()
        self.sidebar.setObjectName("sidebar")
        sidebar_layout = QVBoxLayout(self.sidebar)
        sidebar_layout.setContentsMargins(14, 14, 14, 14)
        sidebar_layout.setSpacing(8)

        head = QHBoxLayout()
        title = QLabel(APP_NAME)
        title.setObjectName("appTitle")
        subtitle = QLabel("Lightweight • private local state")
        subtitle.setObjectName("muted")
        self.collapse = QToolButton()
        self.collapse.setText("‹")
        self.collapse.setToolTip("Collapse sidebar")
        self.collapse.clicked.connect(self.toggle_sidebar)
        head.addWidget(title, 1)
        head.addWidget(self.collapse)
        sidebar_layout.addLayout(head)
        sidebar_layout.addWidget(subtitle)

        self.new_button = QPushButton("＋  New chat")
        self.new_button.setObjectName("primary")
        self.new_button.clicked.connect(self.new_chat)
        sidebar_layout.addWidget(self.new_button)

        self.chat_list = QListWidget()
        self.chat_list.currentRowChanged.connect(self.select_chat)
        sidebar_layout.addWidget(self.chat_list, 1)

        for text, callback in (
            ("Rename chat", self.rename_chat),
            ("Delete chat", self.delete_chat),
            ("Memory", self.show_memory),
            ("Settings", self.settings_dialog),
        ):
            button = QPushButton(text)
            button.clicked.connect(callback)
            sidebar_layout.addWidget(button)

        layout.addWidget(self.sidebar)

        main = QWidget()
        main_layout = QVBoxLayout(main)
        main_layout.setContentsMargins(4, 4, 4, 4)
        main_layout.setSpacing(10)

        top = QHBoxLayout()
        self.header = QLabel()
        self.header.setObjectName("header")
        menu = QToolButton()
        menu.setText("☰")
        menu.setToolTip("Show/hide sidebar")
        menu.clicked.connect(self.toggle_sidebar)
        top.addWidget(self.header, 1)
        top.addWidget(menu)
        main_layout.addLayout(top)

        self.output = QTextBrowser()
        self.output.setOpenExternalLinks(True)
        self.output.setReadOnly(True)
        main_layout.addWidget(self.output, 1)

        bottom = QHBoxLayout()
        self.entry = QTextEdit()
        self.entry.setPlaceholderText("Message Vaxx…")
        self.entry.setFixedHeight(92)
        self.entry.installEventFilter(self)
        bottom.addWidget(self.entry, 1)

        self.send = QPushButton("Send  ↑")
        self.send.setObjectName("primary")
        self.send.clicked.connect(self.send_message)
        bottom.addWidget(self.send)
        main_layout.addLayout(bottom)

        layout.addWidget(main, 1)
        self.setCentralWidget(root)

    def _apply_theme(self) -> None:
        dark = effective_appearance(self.settings.appearance) == "dark"
        bg = "#111318" if dark else "#f5f7fb"
        panel = "#181b22" if dark else "#ffffff"
        input_bg = "#20242d" if dark else "#ffffff"
        text = "#f2f4f7" if dark else "#1f2937"
        muted = "#9aa3b2" if dark else "#6b7280"
        border = "#2a303b" if dark else "#dbe2ea"
        hover = "#20242d" if dark else "#f1f5f9"
        accents = {
            "default": "#5b8cff",
            "cyan": "#22d3ee",
            "green": "#22c55e",
            "magenta": "#e879f9",
        }
        accent = accents.get(self.settings.theme, accents["default"])

        self.setStyleSheet(
            f"""
            QMainWindow, QWidget {{
                background: {bg};
                color: {text};
                font-family: 'Noto Sans';
                font-size: 11pt;
            }}
            #sidebar {{
                background: {panel};
                border: 1px solid {border};
                border-radius: 18px;
            }}
            #appTitle {{ font-size: 18pt; font-weight: 700; }}
            #header {{ font-size: 14pt; font-weight: 650; padding: 6px; }}
            #muted {{ color: {muted}; font-size: 9pt; }}
            QPushButton, QToolButton {{
                background: {panel};
                color: {text};
                border: 1px solid {border};
                border-radius: 11px;
                padding: 9px 13px;
            }}
            QPushButton:hover, QToolButton:hover {{
                border-color: {accent};
                background: {hover};
            }}
            QPushButton:pressed, QToolButton:pressed {{ padding-top: 10px; padding-bottom: 8px; }}
            QPushButton#primary {{
                background: {accent};
                color: white;
                border: 0;
                font-weight: 650;
            }}
            QPushButton#primary:hover {{ background: {accent}; }}
            QListWidget, QTextBrowser, QTextEdit, QLineEdit, QComboBox {{
                background: {input_bg};
                color: {text};
                border: 1px solid {border};
                border-radius: 15px;
                padding: 10px;
                selection-background-color: {accent};
                selection-color: white;
            }}
            QListWidget::item {{ padding: 8px; border-radius: 8px; }}
            QListWidget::item:selected {{ background: {accent}; color: white; }}
            QScrollBar:vertical {{ width: 9px; background: transparent; margin: 4px; }}
            QScrollBar::handle:vertical {{ background: {border}; border-radius: 4px; min-height: 28px; }}
            QScrollBar::add-line, QScrollBar::sub-line {{ height: 0px; }}
            QCheckBox {{ spacing: 8px; }}
            """
        )

        shadow = QGraphicsDropShadowEffect(self)
        shadow.setBlurRadius(22)
        shadow.setOffset(0, 5)
        shadow.setColor(Qt.GlobalColor.black if not dark else Qt.GlobalColor.black)
        self.sidebar.setGraphicsEffect(shadow)
        self._set_sidebar_width(self.sidebar_width if self.sidebar.isVisible() else 0)

    @staticmethod
    def _html_response(text: str) -> str:
        rendered = render(text)
        lines = rendered.splitlines()
        out: list[str] = []
        in_code = False
        for raw in lines:
            line = html.escape(raw)
            if raw.strip().startswith("```") or raw.strip().startswith("~~~"):
                in_code = not in_code
                if in_code:
                    out.append("<pre>")
                else:
                    out.append("</pre>")
                continue
            if in_code:
                out.append(line)
                continue
            line = re.sub(r"^\s*#{1,6}\s+(.+)$", r"<h3>\1</h3>", line)
            line = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", line)
            line = re.sub(r"(?<!\*)\*([^*\n]+)\*(?!\*)", r"<i>\1</i>", line)
            line = re.sub(r"`([^`]+)`", r"<code>\1</code>", line)
            if line.startswith("• "):
                line = "&bull; " + line[2:]
            out.append(line + "<br>")
        return "".join(out)

    def _refresh_chats(self) -> None:
        self.chat_list.blockSignals(True)
        self.chat_list.clear()
        self.chat_list.addItems(list(self.store.chats))
        names = list(self.store.chats)
        self.chat_list.setCurrentRow(names.index(self.current))
        self.chat_list.blockSignals(False)

    def _show_chat(self) -> None:
        self.chat = self.store.chats[self.current]
        self.setWindowTitle(f"{APP_NAME} — {self.chat.ai_name}")
        self.header.setText(f"{self.chat.ai_name}  ·  {self.current}  ·  {self.settings.model}")
        blocks: list[str] = []
        for message in self.chat.messages:
            if message.get("role") == "system":
                continue
            who = "You" if message["role"] == "user" else self.chat.ai_name
            blocks.append(
                f"<div style='margin: 12px 4px'><div><b>{html.escape(who)}</b></div>"
                f"<div style='margin-top: 6px'>{self._html_response(message['content'])}</div></div>"
            )
        self.output.setHtml("".join(blocks) or "<p>Start a conversation with Vaxx.</p>")
        self.output.moveCursor(QTextCursor.End)

    def select_chat(self, index: int) -> None:
        if index < 0:
            return
        names = list(self.store.chats)
        if index >= len(names):
            return
        self.current = names[index]
        self.settings.current_chat = self.current
        self.settings.save()
        self._show_chat()

    def new_chat(self) -> None:
        name, ok = QInputDialog.getText(self, "New chat", "Chat name:")
        key = safe_name(name if ok else "")
        if key and key not in self.store.chats:
            self.store.chats[key] = Chat(self._provider())
            self.store.save(key, self.store.chats[key])
            self.current = key
            self.settings.current_chat = key
            self.settings.save()
            self._refresh_chats()
            self._show_chat()

    def rename_chat(self) -> None:
        name, ok = QInputDialog.getText(self, "Rename chat", "New name:", text=self.current)
        key = safe_name(name if ok else "")
        if key and key != self.current and key not in self.store.chats:
            self.store.rename(self.current, key)
            self.current = key
            self.settings.current_chat = key
            self.settings.save()
            self._refresh_chats()
            self._show_chat()

    def delete_chat(self) -> None:
        if len(self.store.chats) <= 1:
            return
        if QMessageBox.question(self, "Delete chat", f"Delete '{self.current}'?") != QMessageBox.StandardButton.Yes:
            return
        self.store.delete(self.current)
        self.current = next(iter(self.store.chats))
        self.settings.current_chat = self.current
        self.settings.save()
        self._refresh_chats()
        self._show_chat()

    def show_memory(self) -> None:
        text = "\n".join(f"{i}. {m}" for i, m in enumerate(self.chat.memories, 1))
        QMessageBox.information(self, "Memory", text or "No saved memories.")

    def settings_dialog(self) -> None:
        dialog = SettingsDialog(self)
        dialog.saved.connect(self.save_settings)
        dialog.exec()

    def save_settings(self, data: dict) -> None:
        for key, value in data.items():
            if hasattr(self.settings, key):
                setattr(self.settings, key, value)
        if data.get("ai_name"):
            self.chat.ai_name = data["ai_name"][:40]
        self.settings.save()
        self.chat.provider = self._provider()
        self.store.save(self.current, self.chat)
        self._apply_theme()
        self._show_chat()

    def _provider(self):
        return make_provider(self.settings.model, self.settings.provider)

    def send_message(self) -> None:
        if not self.entry.isEnabled():
            return
        text = self.entry.toPlainText().strip()
        if not text:
            return
        self.entry.clear()
        self.send.setEnabled(False)
        self.entry.setEnabled(False)
        self.output.append(f"<div><b>You</b></div><div>{self._html_response(text)}</div><br>")

        self.chat.provider = self._provider()
        self._reply_worker = ReplyWorker(self.chat, text)
        self._worker_thread = threading.Thread(target=self._reply_worker.run, daemon=True)
        self._reply_worker.finished.connect(self._finish_reply)
        self._worker_thread.start()

    def _finish_reply(self, answer: str, error: object) -> None:
        if error:
            body = f"<b>{html.escape(self.chat.ai_name)}</b><div>Error: {html.escape(str(error))}</div>"
        else:
            if self.settings.auto_memory:
                self.chat.auto_remember(self.chat.messages[-2]["content"])
            if self.settings.auto_summary:
                self.chat.maybe_summarize()
            body = f"<b>{html.escape(self.chat.ai_name)}</b><div>{self._html_response(answer)}</div>"
            self.store.save(self.current, self.chat)
        self.output.append(body + "<br>")
        self.send.setEnabled(True)
        self.entry.setEnabled(True)
        self.entry.setFocus()
        self.output.moveCursor(QTextCursor.End)

    def _set_sidebar_width(self, width: int) -> None:
        self.sidebar.setMinimumWidth(0)
        self.sidebar.setMaximumWidth(width)
        self.sidebar.setVisible(width > 0)

    def toggle_sidebar(self) -> None:
        start = self.sidebar.maximumWidth()
        end = 0 if start > 0 else self.sidebar_width
        animation = QPropertyAnimation(self.sidebar, b"maximumWidth", self)
        animation.setDuration(190)
        animation.setStartValue(start)
        animation.setEndValue(end)
        animation.setEasingCurve(QEasingCurve.Type.OutCubic)
        animation.finished.connect(lambda: self._set_sidebar_width(end))
        animation.start()
        self._sidebar_animation = animation

    def eventFilter(self, obj, event):
        if obj is self.entry and event.type() == event.Type.KeyPress:
            if event.key() in (Qt.Key_Return, Qt.Key_Enter) and event.modifiers() & Qt.KeyboardModifier.ControlModifier:
                self.send_message()
                return True
        return super().eventFilter(obj, event)

    def closeEvent(self, event) -> None:
        self.store.save(self.current, self.chat)
        self.settings.current_chat = self.current
        self.settings.save()
        event.accept()


def main() -> int:
    app = QApplication(sys.argv)
    app.setApplicationName(APP_NAME)
    app.setApplicationDisplayName(APP_NAME)
    app.setDesktopFileName(APP_CLASS)
    window = MainWindow()
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
