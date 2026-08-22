#pragma once

#include <QDialog>
#include <QString>

class QTextEdit;

class DebugPanel final : public QDialog {
    Q_OBJECT
public:
    explicit DebugPanel(QWidget* parent = nullptr);

    void log(const QString& message);
    void setStatus(const QString& status);

private:
    QTextEdit* m_log = nullptr;
};
