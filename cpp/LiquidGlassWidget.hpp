#pragma once

#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QTimer>

class LiquidGlassWidget final : public QOpenGLWidget {
    Q_OBJECT

public:
    explicit LiquidGlassWidget(QWidget* parent = nullptr);
    void setPerformanceProfile(const QString& profile);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int width, int height) override;

private:
    void tick();
    float strength() const;
    int intervalMs() const;

    QOpenGLShaderProgram* m_program = nullptr;
    QTimer m_timer;
    float m_time = 0.0f;
    QString m_profile = QStringLiteral("Balanced");
};
