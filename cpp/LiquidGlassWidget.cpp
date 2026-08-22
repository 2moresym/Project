#include "LiquidGlassWidget.hpp"

#include <QOpenGLFunctions>

namespace {
constexpr char kVertexShader[] = R"GLSL(
attribute vec2 position;
varying vec2 uv;
void main() {
    uv = position * 0.5 + 0.5;
    gl_Position = vec4(position, 0.0, 1.0);
}
)GLSL";

constexpr char kFragmentShader[] = R"GLSL(
varying vec2 uv;
uniform float time;
uniform float strength;
void main() {
    vec2 p = uv - 0.5;
    float r = length(p);
    float edge = smoothstep(0.74, 0.36, r);
    float wave = sin((uv.x + time * 0.035) * 8.0 + sin(uv.y * 7.0)) * 0.5 + 0.5;
    float sheen = pow(max(0.0, 1.0 - abs(uv.y - 0.18) * 3.0), 5.0);
    float glow = smoothstep(0.55, 0.0, r);
    vec3 tint = vec3(0.25, 0.45, 0.92);
    vec3 light = mix(tint, vec3(0.84, 0.94, 1.0), wave * 0.12);
    light += vec3(0.10, 0.16, 0.30) * sheen;
    float alpha = strength * edge * (0.34 + glow * 0.17 + sheen * 0.12);
    gl_FragColor = vec4(light, alpha);
}
)GLSL";
}

LiquidGlassWidget::LiquidGlassWidget(QWidget* parent)
    : QOpenGLWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    connect(&m_timer, &QTimer::timeout, this, &LiquidGlassWidget::tick);
}

void LiquidGlassWidget::setPerformanceProfile(const QString& profile) {
    if (m_profile == profile) return;
    m_profile = profile;
    if (m_profile == QStringLiteral("Low GPU")) {
        m_timer.stop();
    } else {
        m_timer.start(intervalMs());
    }
    update();
}

void LiquidGlassWidget::initializeGL() {
    m_program = new QOpenGLShaderProgram(this);
    if (!m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader) ||
        !m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, kFragmentShader) ||
        !m_program->link()) {
        delete m_program;
        m_program = nullptr;
        return;
    }

    m_timer.setInterval(intervalMs());
    if (m_profile != QStringLiteral("Low GPU")) m_timer.start();
}

void LiquidGlassWidget::paintGL() {
    if (!m_program) return;

    auto* f = context()->functions();
    f->glClearColor(0.f, 0.f, 0.f, 0.f);
    f->glClear(GL_COLOR_BUFFER_BIT);

    const GLfloat vertices[] = {
        -1.f, -1.f,
         1.f, -1.f,
        -1.f,  1.f,
         1.f,  1.f
    };

    m_program->bind();
    m_program->setUniformValue("time", m_time);
    m_program->setUniformValue("strength", strength());
    m_program->enableAttributeArray("position");
    m_program->setAttributeArray("position", GL_FLOAT, vertices, 2);
    f->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    m_program->disableAttributeArray("position");
    m_program->release();
}

void LiquidGlassWidget::resizeGL(int width, int height) {
    Q_UNUSED(width);
    Q_UNUSED(height);
}

void LiquidGlassWidget::tick() {
    m_time += 0.033f;
    update();
}

float LiquidGlassWidget::strength() const {
    if (m_profile == QStringLiteral("Low GPU")) return 0.25f;
    if (m_profile == QStringLiteral("Smooth")) return 0.82f;
    return 0.58f;
}

int LiquidGlassWidget::intervalMs() const {
    return m_profile == QStringLiteral("Smooth") ? 16 : 33;
}
