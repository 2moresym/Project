#pragma once

#include <QFrame>
#include <QFont>
#include <QLabel>
#include <QVBoxLayout>

class ChatBubble final : public QFrame {
public:
    explicit ChatBubble(const QString& sender, const QString& text, bool assistant, QWidget* parent = nullptr)
        : QFrame(parent) {
        setObjectName(assistant ? QStringLiteral("assistantBubble") : QStringLiteral("userBubble"));
        setAutoFillBackground(false);
        setMaximumWidth(760);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 11, 16, 11);
        layout->setSpacing(5);

        QFont emojiFont(QStringLiteral("Sans Serif"));
        emojiFont.setFamilies({
            QStringLiteral("Noto Sans"),
            QStringLiteral("Noto Color Emoji"),
            QStringLiteral("Segoe UI Emoji"),
            QStringLiteral("Apple Color Emoji"),
            QStringLiteral("Sans Serif")
        });
        emojiFont.setStyleStrategy(QFont::PreferMatch);

        auto* who = new QLabel(sender, this);
        who->setObjectName(QStringLiteral("bubbleSender"));
        who->setAttribute(Qt::WA_TranslucentBackground, true);
        who->setFont(emojiFont);
        who->setStyleSheet(QStringLiteral("background:transparent;border:0;"));
        layout->addWidget(who);

        m_text = new QLabel(text, this);
        m_text->setWordWrap(true);
        m_text->setTextFormat(Qt::PlainText);
        m_text->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        m_text->setAttribute(Qt::WA_TranslucentBackground, true);
        m_text->setFont(emojiFont);
        m_text->setStyleSheet(QStringLiteral("background:transparent;border:0;color:palette(text);"));
        layout->addWidget(m_text);
    }

    void setText(const QString& text) { m_text->setText(text); }

private:
    QLabel* m_text = nullptr;
};
