"""Lightweight GPU glass surface used by the Qt UI.

This intentionally uses Qt's OpenGL widget rather than forcing a global OpenGL
backend. The effect is isolated to the glass surface, which is much safer on
older Mesa drivers and lets the rest of the Qt Widgets UI stay conventional.
"""
from __future__ import annotations

from PySide6.QtCore import QTimer
from PySide6.QtGui import QOpenGLShader, QOpenGLShaderProgram
from PySide6.QtOpenGLWidgets import QOpenGLWidget
from PySide6.QtWidgets import QWidget


class LiquidGlass(QOpenGLWidget):
    """Small procedural glass layer; no textures and no expensive blur passes."""

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setAttribute(Qt.WidgetAttribute.WA_TransparentForMouseEvents, True)
        self.setAttribute(Qt.WidgetAttribute.WA_NoSystemBackground, True)
        self._program: QOpenGLShaderProgram | None = None
        self._time = 0.0
        self._timer = QTimer(self)
        self._timer.setInterval(33)  # ~30 FPS: plenty for a UI accent on HD 4000
        self._timer.timeout.connect(self._tick)

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
            float edge = smoothstep(0.70, 0.42, r);
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
        self._program = QOpenGLShaderProgram(self)
        self._program.addShaderFromSourceCode(QOpenGLShader.ShaderTypeBit.Vertex, vertex)
        self._program.addShaderFromSourceCode(QOpenGLShader.ShaderTypeBit.Fragment, fragment)
        self._program.link()
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
        from OpenGL.GL import glDrawArrays, GL_TRIANGLE_STRIP  # optional PyOpenGL fast path
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4)
        self._program.disableAttributeArray("position")
        self._program.release()

    def _strength(self) -> float:
        owner = self.parentWidget()
        profile = getattr(getattr(owner, "_glass_settings", None), "ui_performance", "Balanced")
        return {"Low GPU": 0.35, "Balanced": 0.62, "Smooth": 0.82}.get(profile, 0.62)
