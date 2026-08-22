#include "ChatBubble.hpp"

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

class ProviderDialog final : public QDialog {
public:
    explicit ProviderDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle(QStringLiteral("AI provider setup"));
        setMinimumWidth(520);
        auto* form = new QFormLayout(this);
        form->setContentsMargins(24, 24, 24, 24);
        form->setSpacing(12);

        m_provider = new QComboBox(this);
        m_provider->addItems({QStringLiteral("Hugging Face"), QStringLiteral("OpenAI-compatible")});
        m_hfToken = new QLineEdit(this);
        m_openaiKey = new QLineEdit(this);
        m_baseUrl = new QLineEdit(this);
        m_model = new QLineEdit(this);
        m_hfToken->setEchoMode(QLineEdit::Password);
        m_openaiKey->setEchoMode(QLineEdit::Password);
        m_baseUrl->setPlaceholderText(QStringLiteral("https://api.openai.com/v1/chat/completions"));
        m_model->setPlaceholderText(QStringLiteral("openai/gpt-oss-120b:groq"));

        QSettings s;
        m_provider->setCurrentIndex(s.value(QStringLiteral("provider"), QStringLiteral("huggingface")).toString() == QStringLiteral("openai") ? 1 : 0);
        m_hfToken->setText(s.value(QStringLiteral("hf_token")).toString());
        m_openaiKey->setText(s.value(QStringLiteral("openai_key")).toString());
        m_baseUrl->setText(s.value(QStringLiteral("openai_base_url"), QStringLiteral("https://api.openai.com/v1/chat/completions")).toString());
        m_model->setText(s.value(QStringLiteral("model"), QStringLiteral("openai/gpt-oss-120b:groq")).toString());

        form->addRow(QStringLiteral("Provider"), m_provider);
        form->addRow(QStringLiteral("Hugging Face token"), m_hfToken);
        form->addRow(QStringLiteral("OpenAI API key"), m_openaiKey);
        form->addRow(QStringLiteral("OpenAI-compatible endpoint"), m_baseUrl);
        form->addRow(QStringLiteral("Model"), m_model);

        auto* note = new QLabel(QStringLiteral("Credentials are saved in your local desktop settings so you do not need a terminal. They are not committed to the project."), this);
        note->setWordWrap(true);
        note->setObjectName(QStringLiteral("settingsNote"));
        form->addRow(note);

        auto* buttons = new QHBoxLayout;
        buttons->addStretch();
        auto* cancel = new QPushButton(QStringLiteral("Cancel"), this);
        auto* save = new QPushButton(QStringLiteral("Save & connect"), this);
        save->setObjectName(QStringLiteral("primaryButton"));
        buttons->addWidget(cancel);
        buttons->addWidget(save);
        form->addRow(buttons);
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
        connect(save, &QPushButton::clicked, this, &ProviderDialog::save);
    }

private:
    void save() {
        QSettings s;
        const bool openai = m_provider->currentIndex() == 1;
        s.setValue(QStringLiteral("provider"), openai ? QStringLiteral("openai") : QStringLiteral("huggingface"));
        s.setValue(QStringLiteral("hf_token"), m_hfToken->text().trimmed());
        s.setValue(QStringLiteral("openai_key"), m_openaiKey->text().trimmed());
        s.setValue(QStringLiteral("openai_base_url"), m_baseUrl->text().trimmed());
        s.setValue(QStringLiteral("model"), m_model->text().trimmed());
        s.sync();
        accept();
    }

    QComboBox* m_provider = nullptr;
    QLineEdit* m_hfToken = nullptr;
    QLineEdit* m_openaiKey = nullptr;
    QLineEdit* m_baseUrl = nullptr;
    QLineEdit* m_model = nullptr;
};

class DebugDialog final : public QDialog {
public:
    explicit DebugDialog(const QStringList& logs, QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle(QStringLiteral("Debug mode"));
        resize(820, 520);
        auto* layout = new QVBoxLayout(this);
        auto* output = new QPlainTextEdit(this);
        output->setReadOnly(true);
        output->setPlainText(logs.join(QStringLiteral("\n")));
        layout->addWidget(output);
        auto* close = new QPushButton(QStringLiteral("Close"), this);
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
        QSettings s;
        if (s.value(QStringLiteral("first_run"), true).toBool()) {
            s.setValue(QStringLiteral("first_run"), false);
            QTimer::singleShot(350, this, &NativeWindow::showProviderSetup);
        }
    }

    ~NativeWindow() override {
        m_typeTimer.stop();
        stopBackend();
    }

private:
    void buildUi() {
        auto* root = new QWidget(this);
        auto* rootLayout = new QHBoxLayout(root);
        rootLayout->setContentsMargins(12, 12, 12, 12);
        rootLayout->setSpacing(12);

        auto* sidebar = new QFrame(this);
        sidebar->setObjectName(QStringLiteral("sidebar"));
        sidebar->setFixedWidth(252);
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
        auto* chatsLabel = new QLabel(QStringLiteral("Chats"), sidebar);
        chatsLabel->setObjectName(QStringLiteral("sectionLabel"));
        side->addWidget(chatsLabel);
        m_chatList = new QListWidget(sidebar);
        m_chatList->addItem(QStringLiteral("Main chat"));
        m_chatList->setCurrentRow(0);
        side->addWidget(m_chatList, 1);
        auto* provider = new QPushButton(QStringLiteral("AI provider & API key"), sidebar);
        auto* memory = new QPushButton(QStringLiteral("Memory"), sidebar);
        auto* settings = new QPushButton(QStringLiteral("Settings"), sidebar);
        auto* debug = new QPushButton(QStringLiteral("Debug"), sidebar);
        side->addWidget(provider);
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
        connect(m_entry, &QTextEdit::textChanged, this, [this]() { if (!m_waiting && !m_typing) m_send->setEnabled(!m_entry->toPlainText().trimmed().isEmpty()); });
        connect(newChat, &QPushButton::clicked, this, &NativeWindow::newChat);
        connect(provider, &QPushButton::clicked, this, &NativeWindow::showProviderSetup);
        connect(memory, &QPushButton::clicked, this, &NativeWindow::showMemory);
        connect(settings, &QPushButton::clicked, this, &NativeWindow::showSettings);
        connect(debug, &QPushButton::clicked, this, &NativeWindow::showDebug);
        connect(m_backend, &QProcess::readyReadStandardOutput, this, &NativeWindow::readBackend);
        connect(m_backend, &QProcess::readyReadStandardError, this, [this]() { const auto text = m_backend->readAllStandardError().trimmed(); if (!text.isEmpty()) log(QStringLiteral("backend stderr: %1").arg(QString::fromUtf8(text))); });
        connect(m_backend, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) { log(QStringLiteral("backend error: %1").arg(m_backend->errorString())); setStatus(QStringLiteral("●  Backend error")); m_waiting = false; m_send->setEnabled(!m_entry->toPlainText().trimmed().isEmpty()); });
        m_typeTimer.setInterval(14);
        connect(&m_typeTimer, &QTimer::timeout, this, &NativeWindow::typeNextCharacter);
        applyStyle();
    }

    void applyStyle() {
        setStyleSheet(QStringLiteral(
            "QMainWindow,QWidget{background:#0e1117;color:#eef2f7;font-family:'Noto Sans';font-size:11pt;}"
            "#sidebar{background:#151922;border:1px solid #242b38;border-radius:16px;}"
            "#brand{font-size:23pt;font-weight:750;color:#fff;}"
            "#subtitle,#settingsNote{color:#8791a1;font-size:9.5pt;}"
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
        ));
    }

    QProcessEnvironment backendEnvironment() const {
        QSettings s;
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("PROJECT_PROVIDER"), s.value(QStringLiteral("provider"), QStringLiteral("huggingface")).toString());
        env.insert(QStringLiteral("HF_TOKEN"), s.value(QStringLiteral("hf_token")).toString());
        env.insert(QStringLiteral("OPENAI_API_KEY"), s.value(QStringLiteral("openai_key")).toString());
        env.insert(QStringLiteral("OPENAI_BASE_URL"), s.value(QStringLiteral("openai_base_url"), QStringLiteral("https://api.openai.com/v1/chat/completions")).toString());
        env.insert(QStringLiteral("OPENAI_MODEL"), s.value(QStringLiteral("model"), QStringLiteral("openai/gpt-oss-120b:groq")).toString());
        return env;
    }

    void startBackend() {
        if (m_backend->state() != QProcess::NotRunning) return;
        m_backend->setProcessEnvironment(backendEnvironment());
        m_backend->setProgram(QStringLiteral("python3"));
        m_backend->setArguments({QStringLiteral("-m"), QStringLiteral("src.backend_bridge")});
        m_backend->setWorkingDirectory(QCoreApplication::applicationDirPath() + QStringLiteral("/.."));
        m_backend->start();
    }

    void stopBackend() { if (m_backend->state() != QProcess::NotRunning) { m_backend->terminate(); if (!m_backend->waitForFinished(500)) m_backend->kill(); } }
    void restartBackend() { stopBackend(); startBackend(); }

    void sendMessage() {
        const auto text = m_entry->toPlainText().trimmed();
        if (text.isEmpty() || m_waiting || m_typing) return;
        startBackend();
        if (!m_backend->waitForStarted(1000)) return;
        addBubble(QStringLiteral("You"), text, false);
        m_entry->clear(); m_waiting = true; m_send->setEnabled(false); setStatus(QStringLiteral("●  Thinking…"));
        QJsonObject object; object.insert(QStringLiteral("action"), QStringLiteral("reply")); object.insert(QStringLiteral("text"), text);
        m_backend->write(QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n');
    }

    void readBackend() {
        while (m_backend->canReadLine()) {
            const auto line = m_backend->readLine().trimmed();
            const auto doc = QJsonDocument::fromJson(line);
            if (!doc.isObject()) continue;
            const auto object = doc.object();
            if (m_memoryRequest) {
                m_memoryRequest = false;
                QStringList values; for (const auto& value : object.value(QStringLiteral("memories")).toArray()) values << value.toString();
                QMessageBox::information(this, QStringLiteral("Memory"), values.isEmpty() ? QStringLiteral("No saved memories.") : values.join(QStringLiteral("\n\n")));
                continue;
            }
            if (!object.value(QStringLiteral("ok")).toBool()) { addBubble(QStringLiteral("Vaxx"), QStringLiteral("Something went wrong: %1").arg(object.value(QStringLiteral("error")).toString()), true); finishTyping(); continue; }
            m_typingBubble = addBubble(QStringLiteral("Vaxx"), QStringLiteral("▌"), true);
            m_typedText = object.value(QStringLiteral("answer")).toString(); m_typeIndex = 0; m_waiting = false; m_typing = true; m_typeTimer.start(); setStatus(QStringLiteral("●  Typing…"));
        }
    }

    ChatBubble* addBubble(const QString& sender, const QString& text, bool assistant) {
        auto* row = new QWidget(m_conversation); auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        auto* bubble = new ChatBubble(sender, text, assistant, row);
        if (assistant) { rowLayout->addStretch(); rowLayout->addWidget(bubble, 0, Qt::AlignRight); }
        else { rowLayout->addWidget(bubble, 0, Qt::AlignLeft); rowLayout->addStretch(); }
        m_conversationLayout->insertWidget(m_conversationLayout->count() - 1, row);
        scrollToBottom();
        return bubble;
    }

    void typeNextCharacter() {
        if (!m_typingBubble) return;
        if (m_typeIndex >= m_typedText.size()) { finishTyping(); return; }
        ++m_typeIndex;
        m_typingBubble->setText(m_typedText.left(m_typeIndex) + QStringLiteral("▌"));
        scrollToBottom();
    }

    void finishTyping() {
        m_typeTimer.stop();
        if (m_typingBubble) m_typingBubble->setText(m_typedText);
        m_typingBubble = nullptr; m_typedText.clear(); m_typeIndex = 0; m_typing = false; m_waiting = false;
        setStatus(QStringLiteral("●  Ready"));
        m_send->setEnabled(!m_entry->toPlainText().trimmed().isEmpty());
    }

    void newChat() {
        if (m_waiting || m_typing) return;
        while (m_conversationLayout->count() > 1) { auto* item = m_conversationLayout->takeAt(0); if (auto* widget = item->widget()) widget->deleteLater(); delete item; }
        setStatus(QStringLiteral("●  New chat"));
        log(QStringLiteral("native conversation view cleared"));
    }

    void showProviderSetup() {
        if (ProviderDialog(this).exec() == QDialog::Accepted) { restartBackend(); setStatus(QStringLiteral("●  Provider settings saved")); log(QStringLiteral("provider configuration saved")); }
    }

    void showMemory() {
        if (m_backend->state() == QProcess::NotRunning) startBackend();
        if (!m_backend->waitForStarted(1000)) return;
        m_memoryRequest = true;
        QJsonObject object; object.insert(QStringLiteral("action"), QStringLiteral("memory"));
        m_backend->write(QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n');
    }

    void showSettings() {
        QMessageBox::information(this, QStringLiteral("Settings"), QStringLiteral("Provider and API credentials are available from 'AI provider & API key'. More UI preferences will be added here as the native frontend grows."));
    }

    void showDebug() {
        m_logs << QStringLiteral("[%1] backend state=%2").arg(QDateTime::currentDateTime().toString(Qt::ISODate), QString::number(m_backend->state()));
        DebugDialog(m_logs, this).exec();
    }

    void scrollToBottom() { QTimer::singleShot(0, this, [this]() { auto* bar = m_scroll->verticalScrollBar(); bar->setValue(bar->maximum()); }); }
    void setStatus(const QString& text) { m_status->setText(text); }
    void log(const QString& message) { m_logs << QStringLiteral("[%1] %2").arg(QDateTime::currentDateTime().toString(Qt::ISODate), message); }

    QProcess* m_backend = nullptr;
    QListWidget* m_chatList = nullptr;
    QLabel* m_status = nullptr;
    QScrollArea* m_scroll = nullptr;
    QWidget* m_conversation = nullptr;
    QVBoxLayout* m_conversationLayout = nullptr;
    QTextEdit* m_entry = nullptr;
    QPushButton* m_send = nullptr;
    QTimer m_typeTimer;
    ChatBubble* m_typingBubble = nullptr;
    QString m_typedText;
    int m_typeIndex = 0;
    bool m_waiting = false;
    bool m_typing = false;
    bool m_memoryRequest = false;
    QStringList m_logs;
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("Vaxx"));
    app.setApplicationName(QStringLiteral("AI Chat"));
    NativeWindow window;
    window.show();
    return app.exec();
}
