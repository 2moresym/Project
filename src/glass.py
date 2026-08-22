"""Lightweight GPU liquid-glass surface for the Qt UI."""
from __future__ import annotations

from PySide6.QtCore import QEvent, QTimer, Qt
from PySide6.QtGui import QOpenGLShader, QOpenGLShaderProgram
from PySide6.QtOpenGLWidgets import QOpenGLWidget
from PySide6.QtWidgets import QWidget


class LiquidGlass(QOpenGLWidget):
    """Procedural glass layer with a tiny shader and no texture uploads."""

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setAttribute(Qt.WidgetAttribute.WA_TransparentForMouseEvents, True)
        self.setAttribute(Qt.WidgetAttribute.WA_NoSystemBackground, True)
        self._program: QOpenGLShaderProgram | None = None
        self._time = 0.0
        self._timer = QTimer(self)
        self._timer.setInterval(33)
        self._timer.timeout.connect(self._tick)
        if parent is not None:
            parent.installEventFilter(self)

    def eventFilter(self, watched: QObject, event: QEvent) -> bool:
        if watched is self.parentWidget() and event.type() == QEvent.Type.Resize:
            self.setGeometry(watched.rect())
        return super().eventFilter(watched, event)

    def _tick(self) -> None:
        self._time += 0.033
        self.update()

    def initializeGL(self) -> None:
        vertex = """
        attribute vec2 position;
        varying vec2 uv;
        void main() {
            uv = position * 0.5 + 0.5;
            gl_Position = vec4(position, 0.0, 1.0);
        }
        """
        fragment = """
        varying vec2 uv;
        uniform float time;
        uniform float strength;
        void main() {
            vec2 p = uv - 0.5;
            float r = length(p);
            float edge = smoothstep(0.72, 0.38, r);
            float wave = sin((uv.x + time * 0.035) * 8.0 + sin(uv.y * 7.0)) * 0.5 + 0.5;
            float sheen = pow(max(0.0, 1.0 - abs(uv.y - 0.18) * 3.0), 5.0);
            float glow = smoothstep(0.55, 0.0, r);
            vec3 tint = vec3(0.34, 0.55, 1.0);
            vec3 light = mix(tint, vec3(0.78, 0.91, 1.0), wave * 0.10);
            light += vec3(0.10, 0.16, 0.28) * sheen;
            float alpha = strength * edge * (0.42 + glow * 0.16 + sheen * 0.12);
            gl_FragColor = vec4(light, alpha);
        }
        """
        program = QOpenGLShaderProgram(self)
        if not program.addShaderFromSourceCode(QOpenGLShader.ShaderTypeBit.Vertex, vertex):
            return
        if not program.addShaderFromSourceCode(QOpenGLShader.ShaderTypeBit.Fragment, fragment):
            return
        if not program.link():
            return
        self._program = program
        profile = self._profile()
        if profile != "Low GPU":
            self._timer.start()

    def paintGL(self) -> None:
        if self._program is None:
            return
        self._program.bind()
        self._program.setUniformValue("time", self._time)
        self._program.setUniformValue("strength", self._strength())
        vertices = (-1.0, -1.0, 1.0, -1.0, -1.0, 1.0, 1.0, 1.0)
        self._program.enableAttributeArray("position")
        self._program.setAttributeArray("position", vertices, 2)
        self.context().functions().glDrawArrays(0x0005, 0, 4)  # GL_TRIANGLE_STRIP
        self._program.disableAttributeArray("position")
        self._program.release()

    def _profile(self) -> str:
        window = self.window()
        return getattr(getattr(window, "settings", None), "ui_performance", "Balanced")

    def _strength(self) -> float:
        return {"Low GPU": 0.30, "Balanced": 0.58, "Smooth": 0.82}.get(self._profile(), 0.58)


def install_glass() -> None:
    """Attach the OpenGL glass layer automatically to the existing sidebar."""
    if getattr(QWidget, "_liquid_glass_installed", False):
        return
    original_show = QWidget.showEvent

    def show_event(widget: QWidget, event: QEvent) -> None:
        original_show(widget, event)
        if widget.objectName() != "sidebar" or hasattr(widget, "_liquid_glass"):
            return
        glass = LiquidGlass(widget)
        glass.setGeometry(widget.rect())
        glass.lower()
        widget._liquid_glass = glass
        widget._glass_settings = getattr(widget.window(), "settings", None)
        glass.show()

    QWidget.showEvent = show_event
    QWidget._liquid_glass_installed = True
