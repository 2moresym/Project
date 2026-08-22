#include "ChatBubble.hpp"

#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

class SettingsDialog final : public QDialog {
public:
    SettingsDialog(const QString& profile, QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle(QStringLiteral("Settings"));
        setModal(true);
        setMinimumWidth(420);
        auto* form = new QFormLayout(this);
        form->setContentsMargins(24, 24, 24, 24);
        form->setSpacing(14);
        m_name = new QLineEdit(QStringLiteral("Vaxx"), this);
        m_profile = new QComboBox(this);
        m_profile->addItems({QStringLiteral("Low GPU"), QStringLiteral("Balanced"), QStringLiteral("Smooth")});
        m_profile->setCurrentText(profile);
        m_theme = new QComboBox(this);
        m_theme->addItems({QStringLiteral("Dark"), QStringLiteral("Light")});
        m_theme->setCurrentText(QStringLiteral("Dark"));
        form->addRow(QStringLiteral("AI name"), m_name);
        form->addRow(QStringLiteral("UI performance"), m_profile);
        form->addRow(QStringLiteral("Appearance"), m_theme);
        auto* buttons = new QHBoxLayout;
        buttons->addStretch();
        auto* cancel = new QPushButton(QStringLiteral("Cancel"), this);
        auto* save = new QPushButton(QStringLiteral("Save"), this);
        save->setObjectName(QStringLiteral("primaryButton"));
        buttons->addWidget(cancel);
        buttons->addWidget(save);
        form->addRow(buttons);
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
        connect(save, &QPushButton::clicked, this, &QDialog::accept);
    }
    QString profile() const { return m_profile->currentText(); }
private:
    QLineEdit* m_name = nullptr;
    QComboBox* m_profile = nullptr;
    QComboBox* m_theme = nullptr;
};

class DebugDialog final : public QDialog {
public:
    DebugDialog(const QStringList& logs, QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle(QStringLiteral("Debug mode"));
        resize(760, 500);
        auto* layout = new QVBoxLayout(this);
        auto* output = new QPlainTextEdit(this);
        output->setReadOnly(true);
        output->setPlainText(logs.join(QStringLiteral("\n")));
        layout->addWidget(output);
        auto* close = new QPushButton(QStringLiteral("Close"), this);
        close->setObjectName(QStringLiteral("primaryButton"));
        layout->addWidget(close, 0, Qt::AlignRight);
        connect(close, &QPushButton::clicked, this, &QDialog::accept);
    }
};

class NativeWindow final : public QMainWindow {
public:
    NativeWindow() : m_backend(new QProcess(this)) {
        setWindowTitle(QStringLiteral("AI Chat — Vaxx"));
        resize(1180, 780);
        setMinimumSize(900, 620);
        buildUi();
        startBackend();
        log(QStringLiteral("native C++/Qt UI ready"));
    }
    ~NativeWindow() override {
        m_typeTimer.stop();
        if (m_backend) {
            m_backend->closeWriteChannel();
            m_backend->terminate();
            if (!m_backend->waitForFinished(500)) m_backend->kill();
        }
    }

private:
    void buildUi() {
        auto* root = new QWidget(this);
        auto* rootLayout = new QHBoxLayout(root);
        rootLayout->setContentsMargins(12, 12, 12, 12);
        rootLayout->setSpacing(12);

        auto* sidebar = new QFrame(this);
        sidebar->setObjectName(QStringLiteral("sidebar"));
        sidebar->setFixedWidth(250);
        auto* side = new QVBoxLayout(sidebar);
        side->setContentsMargins(14, 14, 14, 14);
        side->setSpacing(9);
        auto* brand = new QLabel(QStringLiteral("Vaxx"), sidebar);
        brand->setObjectName(QStringLiteral("brand"));
        side->addWidget(brand);
        auto* subtitle = new QLabel(QStringLiteral("AI playground"), sidebar);
        subtitle->setObjectName(QStringLiteral("subtitle"));
        side->addWidget(subtitle);
        auto* newChat = new QPushButton(QStringLiteral("＋  New chat"), sidebar);
        newChat->setObjectName(QStringLiteral("primaryButton"));
        side->addWidget(newChat);
        auto* label = new QLabel(QStringLiteral("Chats"), sidebar);
        label->setObjectName(QStringLiteral("sectionLabel"));
        side->addWidget(label);
        m_chatList = new QListWidget(sidebar);
        m_chatList->addItem(QStringLiteral("Main chat"));
        m_chatList->setCurrentRow(0);
        side->addWidget(m_chatList, 1);
        auto* memory = new QPushButton(QStringLiteral("Memory"), sidebar);
        auto* settings = new QPushButton(QStringLiteral("Settings"), sidebar);
        auto* debug = new QPushButton(QStringLiteral("Debug"), sidebar);
        side->addWidget(memory);
        side->addWidget(settings);
        side->addWidget(debug);
        rootLayout->addWidget(sidebar);

        auto* main = new QWidget(this);
        auto* mainLayout = new QVBoxLayout(main);
        mainLayout->setContentsMargins(6, 2, 6, 2);
        mainLayout->setSpacing(12);
        auto* top = new QHBoxLayout;
        auto* title = new QLabel(QStringLiteral("Main chat"), main);
        title->setObjectName(QStringLiteral("header"));
        top->addWidget(title);
        top->addStretch();
        m_status = new QLabel(QStringLiteral("●  Ready"), main);
        m_status->setObjectName(QStringLiteral("status"));
        top->addWidget(m_status);
        mainLayout->addLayout(top);

        m_scroll = new QScrollArea(main);
        m_scroll->setWidgetResizable(true);
        m_scroll->setFrameShape(QFrame::NoFrame);
        m_conversation = new QWidget;
        m_conversationLayout = new QVBoxLayout(m_conversation);
        m_conversationLayout->setContentsMargins(10, 10, 10, 10);
        m_conversationLayout->setSpacing(12);
        m_conversationLayout->addStretch();
        m_scroll->setWidget(m_conversation);
        mainLayout->addWidget(m_scroll, 1);

        auto* composer = new QFrame(main);
        composer->setObjectName(QStringLiteral("composer"));
        auto* composerLayout = new QHBoxLayout(composer);
        composerLayout->setContentsMargins(10, 8, 10, 8);
        m_entry = new QTextEdit(composer);
        m_entry->setPlaceholderText(QStringLiteral("Message Vaxx…"));
        m_entry->setFixedHeight(76);
        composerLayout->addWidget(m_entry, 1);
        m_send = new QPushButton(QStringLiteral("Send  ↑"), composer);
        m_send->setObjectName(QStringLiteral("primaryButton"));
        m_send->setFixedWidth(100);
        m_send->setEnabled(false);
        composerLayout->addWidget(m_send);
        mainLayout->addWidget(composer);
        rootLayout->addWidget(main, 1);
        setCentralWidget(root);

        connect(m_send, &QPushButton::clicked, this, &NativeWindow::sendMessage);
        connect(m_entry, &QTextEdit::textChanged, this, [this]() {
            if (!m_waiting && !m_typing) m_send->setEnabled(!m_entry->toPlainText().trimmed().isEmpty());
        });
        connect(newChat, &QPushButton::clicked, this, &NativeWindow::newChat);
        connect(settings, &QPushButton::clicked, this, &NativeWindow::showSettings);
        connect(memory, &QPushButton::clicked, this, &NativeWindow::showMemory);
        connect(debug, &QPushButton::clicked, this, &NativeWindow::showDebug);
        connect(m_backend, &QProcess::readyReadStandardOutput, this, &NativeWindow::readBackend);
        connect(m_backend, &QProcess::readyReadStandardError, this, [this]() {
            const auto text = m_backend->readAllStandardError().trimmed();
            if (!text.isEmpty()) log(QStringLiteral("backend stderr: %1").arg(QString::fromUtf8(text)));
        });
        connect(m_backend, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
            log(QStringLiteral("backend error: %1").arg(m_backend->errorString()));
            setStatus(QStringLiteral("●  Backend error"));
            m_waiting = false;
            m_send->setEnabled(true);
        });
        m_typeTimer.setInterval(14);
        connect(&m_typeTimer, &QTimer::timeout, this, &NativeWindow::typeNextCharacter);
        applyStyle();
    }

    void applyStyle() {
        setStyleSheet(QStringLiteral(
            "QMainWindow,QWidget{background:#0e1117;color:#eef2f7;font-family:'Noto Sans';font-size:11pt;}"
            "#sidebar{background:#151922;border:1px solid #242b38;border-radius:16px;}"
            "#brand{font-size:23pt;font-weight:750;color:#fff;}"
            "#subtitle{color:#8791a1;font-size:9.5pt;}"
            "#sectionLabel{color:#8f9aaa;font-weight:650;padding-top:8px;}"
            "#header{font-size:17pt;font-weight:700;padding:5px;}"
            "#status{color:#76d69a;font-size:9.5pt;padding:6px;}"
            "QPushButton,QComboBox{background:#191e28;color:#eef2f7;border:1px solid #2a3240;border-radius:10px;padding:9px 12px;}"
            "QPushButton:hover{border-color:#5b8cff;background:#1d2430;}"
            "#primaryButton{background:#5b8cff;color:#fff;border:0;font-weight:700;}"
            "#primaryButton:hover{background:#6b99ff;}"
            "QListWidget{background:#11151d;border:1px solid #242b38;border-radius:10px;padding:5px;}"
            "QListWidget::item{padding:10px;border-radius:8px;}"
            "QListWidget::item:selected{background:#283757;color:#fff;}"
            "#composer{background:#151922;border:1px solid #242b38;border-radius:15px;}"
            "QTextEdit,QPlainTextEdit,QLineEdit{background:#10141b;color:#eef2f7;border:1px solid #2a3240;border-radius:11px;padding:10px;}"
            "#userBubble{background:#202b40;border-radius:14px;}"
            "#assistantBubble{background:#171d27;border:1px solid #2a3342;border-radius:14px;}"
            "#bubbleSender{font-size:9pt;font-weight:700;color:#8fb3ff;}"
            "QScrollBar:vertical{width:9px;background:transparent;margin:2px;}"
            "QScrollBar::handle:vertical{background:#2d3544;border-radius:4px;min-height:28px;}"
            "QScrollBar::add-line,QScrollBar::sub-line{height:0px;}"
        ));
    }

    void startBackend() {
        if (m_backend->state() != QProcess::NotRunning) return;
        m_backend->setProgram(QStringLiteral("python3"));
        m_backend->setArguments({QStringLiteral("-m"), QStringLiteral("src.backend_bridge")});
        m_backend->setWorkingDirectory(QCoreApplication::applicationDirPath() + QStringLiteral("/.."));
        m_backend->start();
        log(QStringLiteral("starting Python backend"));
    }

    void sendMessage() {
        const QString text = m_entry->toPlainText().trimmed();
        if (text.isEmpty() || m_waiting || m_typing) return;
        startBackend();
        if (!m_backend->waitForStarted(1000)) return;
        addBubble(QStringLiteral("You"), text, false);
        m_entry->clear();
        m_waiting = true;
        m_send->setEnabled(false);
        setStatus(QStringLiteral("●  Thinking…"));
        QJsonObject object;
        object.insert(QStringLiteral("action"), QStringLiteral("reply"));
        object.insert(QStringLiteral("text"), text);
        m_backend->write(QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n');
        log(QStringLiteral("request sent: %1 chars").arg(text.size()));
    }

    void readBackend() {
        while (m_backend->canReadLine()) {
            const auto line = m_backend->readLine().trimmed();
            const auto doc = QJsonDocument::fromJson(line);
            if (!doc.isObject()) {
                log(QStringLiteral("ignored non-JSON: %1").arg(QString::fromUtf8(line)));
                continue;
            }
            const auto object = doc.object();
            if (m_memoryRequest) {
                m_memoryRequest = false;
                QStringList values;
                for (const auto& value : object.value(QStringLiteral("memories")).toArray()) values << value.toString();
                QMessageBox::information(this, QStringLiteral("Memory"), values.isEmpty() ? QStringLiteral("No saved memories.") : values.join(QStringLiteral("\n\n")));
                continue;
            }
            if (!object.value(QStringLiteral("ok")).toBool()) {
                addBubble(QStringLiteral("Vaxx"), QStringLiteral("Something went wrong: %1").arg(object.value(QStringLiteral("error")).toString()), true);
                finishTyping();
                continue;
            }
            m_typingBubble = addBubble(QStringLiteral("Vaxx"), QStringLiteral("▌"), true);
            m_typedText = object.value(QStringLiteral("answer")).toString();
            m_typeIndex = 0;
            m_waiting = false;
            m_typing = true;
            m_typeTimer.start();
            setStatus(QStringLiteral("●  Typing…"));
        }
    }

    ChatBubble* addBubble(const QString& sender, const QString& text, bool assistant) {
        auto* row = new QWidget(m_conversation);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        auto* bubble = new ChatBubble(sender, text, assistant, row);
        if (assistant) {
            rowLayout->addStretch();
            rowLayout->addWidget(bubble, 0, Qt::AlignRight);
        } else {
            rowLayout->addWidget(bubble, 0, Qt::AlignLeft);
            rowLayout->addStretch();
        }
        m_conversationLayout->insertWidget(m_conversationLayout->count() - 1, row);
        scrollToBottom();
        return bubble;
    }

    void typeNextCharacter() {
        if (!m_typingBubble) return;
        if (m_typeIndex >= m_typedText.size()) {
            finishTyping();
            return;
        }
        ++m_typeIndex;
        m_typingBubble->setText(m_typedText.left(m_typeIndex) + QStringLiteral("▌"));
        scrollToBottom();
    }

    void finishTyping() {
        m_typeTimer.stop();
        if (m_typingBubble) m_typingBubble->setText(m_typedText);
        m_typingBubble = nullptr;
        m_typedText.clear();
        m_typeIndex = 0;
        m_typing = false;
        m_waiting = false;
        setStatus(QStringLiteral("●  Ready"));
        m_send->setEnabled(!m_entry->toPlainText().trimmed().isEmpty());
    }

    void newChat() {
        if (m_waiting || m_typing) return;
        while (m_conversationLayout->count() > 1) {
            auto* item = m_conversationLayout->takeAt(0);
            if (auto* widget = item->widget()) widget->deleteLater();
            delete item;
        }
        setStatus(QStringLiteral("●  New chat"));
        log(QStringLiteral("native conversation view cleared"));
    }

    void showSettings() {
        SettingsDialog dialog(m_profile, this);
        if (dialog.exec() == QDialog::Accepted) {
            m_profile = dialog.profile();
            log(QStringLiteral("performance profile: %1").arg(m_profile));
            setStatus(QStringLiteral("●  Settings saved"));
        }
    }

    void showMemory() {
        startBackend();
        if (!m_backend->waitForStarted(1000)) return;
        m_memoryRequest = true;
        QJsonObject object;
        object.insert(QStringLiteral("action"), QStringLiteral("memory"));
        m_backend->write(QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n');
        log(QStringLiteral("memory requested"));
    }

    void showDebug() {
        QStringList lines = m_logs;
        lines << QStringLiteral("--- snapshot ---");
        lines << QStringLiteral("Qt: %1").arg(QT_VERSION_STR);
        lines << QStringLiteral("Backend state: %1").arg(static_cast<int>(m_backend->state()));
        lines << QStringLiteral("Backend error: %1").arg(m_backend->errorString());
        lines << QStringLiteral("Profile: %1").arg(m_profile);
        lines << QStringLiteral("Waiting: %1").arg(m_waiting ? QStringLiteral("yes") : QStringLiteral("no"));
        lines << QStringLiteral("Typing: %1").arg(m_typing ? QStringLiteral("yes") : QStringLiteral("no"));
        DebugDialog dialog(lines, this);
        dialog.exec();
    }

    void scrollToBottom() {
        QTimer::singleShot(0, this, [this]() {
            m_scroll->verticalScrollBar()->setValue(m_scroll->verticalScrollBar()->maximum());
        });
    }

    void setStatus(const QString& value) { m_status->setText(value); }

    void log(const QString& message) {
        m_logs << QStringLiteral("[%1] %2").arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")), message);
    }

    QProcess* m_backend = nullptr;
    QListWidget* m_chatList = nullptr;
    QLabel* m_status = nullptr;
    QScrollArea* m_scroll = nullptr;
    QWidget* m_conversation = nullptr;
    QVBoxLayout* m_conversationLayout = nullptr;
    QTextEdit* m_entry = nullptr;
    QPushButton* m_send = nullptr;
    QString m_profile = QStringLiteral("Balanced");
    QStringList m_logs;
    QString m_typedText;
    ChatBubble* m_typingBubble = nullptr;
    QTimer m_typeTimer;
    int m_typeIndex = 0;
    bool m_waiting = false;
    bool m_typing = false;
    bool m_memoryRequest = false;
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("AI Chat"));
    app.setApplicationDisplayName(QStringLiteral("AI Chat"));
    NativeWindow window;
    window.show();
    return app.exec();
}
