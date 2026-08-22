#pragma once

#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>

class ChatBubble final : public QFrame {
public:
    explicit ChatBubble(const QString& sender, const QString& text, bool assistant, QWidget* parent = nullptr)
        : QFrame(parent) {
        setObjectName(assistant ? QStringLiteral("assistantBubble") : QStringLiteral("userBubble"));
        setMaximumWidth(760);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 11, 16, 11);
        layout->setSpacing(5);
        auto* who = new QLabel(sender, this);
        who->setObjectName(QStringLiteral("bubbleSender"));
        layout->addWidget(who);
        m_text = new QLabel(text, this);
        m_text->setWordWrap(true);
        m_text->setTextFormat(Qt::PlainText);
        m_text->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        m_text->setStyleSheet(QStringLiteral("background:transparent;border:0;"));
        layout->addWidget(m_text);
    }

    void setText(const QString& text) { m_text->setText(text); }

private:
    QLabel* m_text = nullptr;
};
