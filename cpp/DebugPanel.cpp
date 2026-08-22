#include "DebugPanel.hpp"

#include <QDateTime>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

DebugPanel::DebugPanel(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("AI Chat Debug"));
    resize(760, 480);

    auto* layout = new QVBoxLayout(this);
    m_log = new QTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setAcceptRichText(false);
    m_log->setFontFamily(QStringLiteral("monospace"));
    layout->addWidget(m_log, 1);

    auto* close = new QPushButton(QStringLiteral("Close"), this);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(close);
}

void DebugPanel::log(const QString& message) {
    const auto stamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    m_log->append(QStringLiteral("[%1] %2").arg(stamp, message));
}

void DebugPanel::setStatus(const QString& status) {
    log(QStringLiteral("STATUS: %1").arg(status));
}
