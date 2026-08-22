"""Lightweight GPU liquid-glass surface for the Qt UI.

Uses QOpenGLWidget with the OpenGL functions exposed by Qt itself. This avoids
PyOpenGL and the QOpenGLShader Python binding, both of which vary across
PySide6 installations.
"""
from __future__ import annotations

from PySide6.QtCore import QEvent, QObject, QTimer, Qt
from PySide6.QtOpenGLWidgets import QOpenGLWidget
from PySide6.QtWidgets import QWidget


class LiquidGlass(QOpenGLWidget):
    """Procedural glass layer with a tiny GLSL program."""

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setAttribute(Qt.WidgetAttribute.WA_TransparentForMouseEvents, True)
        self.setAttribute(Qt.WidgetAttribute.WA_NoSystemBackground, True)
        self._program = None
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
        # Qt exposes the OpenGL API through QOpenGLContext.functions().
        # We compile the tiny shader ourselves so this works without PyOpenGL.
        funcs = self.context().functions()
        version = funcs.glGetString(0x1F02)  # GL_VERSION
        if version is None:
            return

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

        # Use the fixed-function-compatible Qt OpenGL functions for shader
        # creation. These enums are part of the OpenGL API and avoid importing
        # optional Python OpenGL packages.
        GL_VERTEX_SHADER = 0x8B31
        GL_FRAGMENT_SHADER = 0x8B30
        GL_COMPILE_STATUS = 0x8B81
        GL_LINK_STATUS = 0x8B82

        create_shader = getattr(funcs, "glCreateShader", None)
        create_program = getattr(funcs, "glCreateProgram", None)
        if create_shader is None or create_program is None:
            return

        def compile_shader(shader_type: int, source: str):
            shader = funcs.glCreateShader(shader_type)
            funcs.glShaderSource(shader, source)
            funcs.glCompileShader(shader)
            if not funcs.glGetShaderiv(shader, GL_COMPILE_STATUS):
                funcs.glDeleteShader(shader)
                return None
            return shader

        vs = compile_shader(GL_VERTEX_SHADER, vertex)
        fs = compile_shader(GL_FRAGMENT_SHADER, fragment)
        if vs is None or fs is None:
            if vs is not None:
                funcs.glDeleteShader(vs)
            if fs is not None:
                funcs.glDeleteShader(fs)
            return

        program = funcs.glCreateProgram()
        funcs.glAttachShader(program, vs)
        funcs.glAttachShader(program, fs)
        funcs.glLinkProgram(program)
        funcs.glDeleteShader(vs)
        funcs.glDeleteShader(fs)
        if not funcs.glGetProgramiv(program, GL_LINK_STATUS):
            funcs.glDeleteProgram(program)
            return

        self._program = program
        self._position = funcs.glGetAttribLocation(program, "position")
        self._time_uniform = funcs.glGetUniformLocation(program, "time")
        self._strength_uniform = funcs.glGetUniformLocation(program, "strength")
        if self._profile() != "Low GPU":
            self._timer.start()

    def paintGL(self) -> None:
        if self._program is None:
            return
        funcs = self.context().functions()
        funcs.glUseProgram(self._program)
        funcs.glUniform1f(self._time_uniform, self._time)
        funcs.glUniform1f(self._strength_uniform, self._strength())

        # A tiny CPU-side array is enough for one fullscreen triangle strip;
        # no VBO allocation or texture upload is needed.
        vertices = (-1.0, -1.0, 1.0, -1.0, -1.0, 1.0, 1.0, 1.0)
        funcs.glEnableVertexAttribArray(self._position)
        funcs.glVertexAttribPointer(self._position, 2, 0x1406, False, 0, vertices)
        funcs.glDrawArrays(0x0005, 0, 4)  # GL_TRIANGLE_STRIP
        funcs.glDisableVertexAttribArray(self._position)
        funcs.glUseProgram(0)

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
