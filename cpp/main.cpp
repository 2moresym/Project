#include "ChatBubble.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
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
#include <QStackedWidget>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

namespace {

const QStringList kHfModels = {
    QStringLiteral("openai/gpt-oss-120b:groq"),
    QStringLiteral("openai/gpt-oss-120b:nscale"),
    QStringLiteral("openai/gpt-oss-120b:together"),
    QStringLiteral("openai/gpt-oss-20b:groq"),
    QStringLiteral("openai/gpt-oss-20b:nscale"),
    QStringLiteral("openai/gpt-oss-20b:together")
};

const QStringList kOpenAiModels = {
    QStringLiteral("gpt-4o-mini"),
    QStringLiteral("gpt-4.1-mini"),
    QStringLiteral("gpt-4.1"),
    QStringLiteral("gpt-5-mini")
};

QString providerName() {
    QSettings settings;
    const QString saved = settings.value(QStringLiteral("provider")).toString().trimmed();
    if (saved == QStringLiteral("openai") || saved == QStringLiteral("huggingface")) {
        return saved;
    }
    return qEnvironmentVariable("OPENAI_API_KEY").trimmed().isEmpty()
        ? QStringLiteral("huggingface")
        : QStringLiteral("openai");
}

bool hasCredential(const QString& provider) {
    QSettings settings;
    const QString saved = provider == QStringLiteral("openai")
        ? settings.value(QStringLiteral("openai_key")).toString().trimmed()
        : settings.value(QStringLiteral("hf_token")).toString().trimmed();
    if (!saved.isEmpty()) return true;
    return provider == QStringLiteral("openai")
        ? !qEnvironmentVariable("OPENAI_API_KEY").trimmed().isEmpty()
        : !qEnvironmentVariable("HF_TOKEN").trimmed().isEmpty();
}

QFont applicationFont() {
    const QStringList installed = QFontDatabase::families();
    QStringList families;
    for (const QString& name : {
        QStringLiteral("Noto Sans"), QStringLiteral("Noto Color Emoji"),
        QStringLiteral("Segoe UI Emoji"), QStringLiteral("Apple Color Emoji"),
        QStringLiteral("Sans Serif")
    }) {
        if (installed.contains(name)) families << name;
    }
    QFont font(families.isEmpty() ? QStringLiteral("Sans Serif") : families.first());
    if (!families.isEmpty()) font.setFamilies(families);
    font.setStyleStrategy(QFont::PreferMatch);
    return font;
}

class SettingsDialog final : public QDialog {
public:
    explicit SettingsDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle(QStringLiteral("Settings"));
        resize(620, 520);

        auto* root = new QVBoxLayout(this);
        auto* tabs = new QTabWidget(this);
        root->addWidget(tabs, 1);

        auto* general = new QWidget(this);
        auto* generalForm = new QFormLayout(general);
        m_name = new QLineEdit(general);
        m_appearance = new QComboBox(general);
        m_appearance->addItems({QStringLiteral("Dark"), QStringLiteral("Light"), QStringLiteral("System")});
        m_performance = new QComboBox(general);
        m_performance->addItems({QStringLiteral("Low GPU"), QStringLiteral("Balanced"), QStringLiteral("Smooth")});
        generalForm->addRow(QStringLiteral("AI name"), m_name);
        generalForm->addRow(QStringLiteral("Appearance"), m_appearance);
        generalForm->addRow(QStringLiteral("UI performance"), m_performance);
        tabs->addTab(general, QStringLiteral("General"));

        auto* provider = new QWidget(this);
        auto* providerForm = new QFormLayout(provider);
        m_provider = new QComboBox(provider);
        m_provider->addItems({QStringLiteral("Hugging Face"), QStringLiteral("OpenAI-compatible")});
        providerForm->addRow(QStringLiteral("Provider"), m_provider);
        m_pages = new QStackedWidget(provider);
        providerForm->addRow(m_pages);

        auto* hfPage = new QWidget(m_pages);
        auto* hfForm = new QFormLayout(hfPage);
        m_hfToken = new QLineEdit(hfPage);
        m_hfToken->setEchoMode(QLineEdit::Password);
        m_hfModel = new QComboBox(hfPage);
        m_hfModel->addItems(kHfModels);
        hfForm->addRow(QStringLiteral("HF token"), m_hfToken);
        hfForm->addRow(QStringLiteral("Model"), m_hfModel);
        m_pages->addWidget(hfPage);

        auto* openAiPage = new QWidget(m_pages);
        auto* openAiForm = new QFormLayout(openAiPage);
        m_openAiKey = new QLineEdit(openAiPage);
        m_openAiKey->setEchoMode(QLineEdit::Password);
        m_endpoint = new QLineEdit(openAiPage);
        m_openAiModel = new QComboBox(openAiPage);
        m_openAiModel->addItems(kOpenAiModels);
        openAiForm->addRow(QStringLiteral("API key"), m_openAiKey);
        openAiForm->addRow(QStringLiteral("Endpoint"), m_endpoint);
        openAiForm->addRow(QStringLiteral("Model"), m_openAiModel);
        m_pages->addWidget(openAiPage);

        tabs->addTab(provider, QStringLiteral("AI provider"));

        auto* buttons = new QHBoxLayout;
        buttons->addStretch();
        auto* cancel = new QPushButton(QStringLiteral("Cancel"), this);
        auto* save = new QPushButton(QStringLiteral("Save"), this);
        save->setObjectName(QStringLiteral("primaryButton"));
        buttons->addWidget(cancel);
        buttons->addWidget(save);
        root->addLayout(buttons);

        load();
        connect(m_provider, &QComboBox::currentTextChanged, this, [this](const QString& text) {
            m_pages->setCurrentIndex(text == QStringLiteral("OpenAI-compatible") ? 1 : 0);
        });
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
        connect(save, &QPushButton::clicked, this, &QDialog::accept);
    }

    void saveSettings() {
        QSettings settings;
        const bool openAi = m_provider->currentIndex() == 1;
        settings.setValue(QStringLiteral("provider"), openAi ? QStringLiteral("openai") : QStringLiteral("huggingface"));
        settings.setValue(QStringLiteral("ai_name"), m_name->text().trimmed().left(40));
        settings.setValue(QStringLiteral("appearance"), m_appearance->currentText());
        settings.setValue(QStringLiteral("ui_performance"), m_performance->currentText());
        settings.setValue(QStringLiteral("hf_token"), m_hfToken->text().trimmed());
        settings.setValue(QStringLiteral("hf_model"), m_hfModel->currentText());
        settings.setValue(QStringLiteral("openai_key"), m_openAiKey->text().trimmed());
        settings.setValue(QStringLiteral("openai_base_url"), m_endpoint->text().trimmed());
        settings.setValue(QStringLiteral("openai_model"), m_openAiModel->currentText());
        settings.sync();
    }

private:
    void load() {
        QSettings settings;
        const QString provider = providerName();
        m_provider->setCurrentIndex(provider == QStringLiteral("openai") ? 1 : 0);
        m_pages->setCurrentIndex(provider == QStringLiteral("openai") ? 1 : 0);
        m_name->setText(settings.value(QStringLiteral("ai_name"), QStringLiteral("Vaxx")).toString());
        m_appearance->setCurrentText(settings.value(QStringLiteral("appearance"), QStringLiteral("Dark")).toString());
        m_performance->setCurrentText(settings.value(QStringLiteral("ui_performance"), QStringLiteral("Balanced")).toString());

        QString hf = settings.value(QStringLiteral("hf_token")).toString();
        if (hf.isEmpty()) hf = qEnvironmentVariable("HF_TOKEN");
        m_hfToken->setText(hf);
        QString openAi = settings.value(QStringLiteral("openai_key")).toString();
        if (openAi.isEmpty()) openAi = qEnvironmentVariable("OPENAI_API_KEY");
        m_openAiKey->setText(openAi);
        m_endpoint->setText(settings.value(QStringLiteral("openai_base_url"), QStringLiteral("https://api.openai.com/v1/chat/completions")).toString());
        m_hfModel->setCurrentText(settings.value(QStringLiteral("hf_model"), kHfModels.first()).toString());
        m_openAiModel->setCurrentText(settings.value(QStringLiteral("openai_model"), kOpenAiModels.first()).toString());
    }

    QLineEdit* m_name = nullptr;
    QComboBox* m_appearance = nullptr;
    QComboBox* m_performance = nullptr;
    QComboBox* m_provider = nullptr;
    QStackedWidget* m_pages = nullptr;
    QLineEdit* m_hfToken = nullptr;
    QComboBox* m_hfModel = nullptr;
    QLineEdit* m_openAiKey = nullptr;
    QLineEdit* m_endpoint = nullptr;
    QComboBox* m_openAiModel = nullptr;
};

class NativeWindow final : public QMainWindow {
public:
    NativeWindow() : m_backend(new QProcess(this)) {
        setWindowTitle(QStringLiteral("AI Chat — Vaxx"));
        resize(1180, 780);
        setMinimumSize(900, 620);
        buildUi();
        connectBackend();
        startBackend();
        QTimer::singleShot(150, this, &NativeWindow::requestChats);
        if (!hasCredential(providerName())) {
            QTimer::singleShot(250, this, &NativeWindow::showSettings);
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
        auto* newChat = new QPushButton(QStringLiteral("New chat"), sidebar);
        newChat->setObjectName(QStringLiteral("primaryButton"));
        side->addWidget(newChat);
        auto* chatLabel = new QLabel(QStringLiteral("Chats"), sidebar);
        chatLabel->setObjectName(QStringLiteral("sectionLabel"));
        side->addWidget(chatLabel);
        m_chatList = new QListWidget(sidebar);
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
        m_title = new QLabel(QStringLiteral("Chat"), main);
        m_title->setObjectName(QStringLiteral("header"));
        top->addWidget(m_title);
        top->addStretch();
        m_status = new QLabel(QStringLiteral("Ready"), main);
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
        m_entry->setFont(applicationFont());
        m_entry->setPlaceholderText(QStringLiteral("Message Vaxx…"));
        m_entry->setFixedHeight(76);
        composerLayout->addWidget(m_entry, 1);
        m_send = new QPushButton(QStringLiteral("Send"), composer);
        m_send->setObjectName(QStringLiteral("primaryButton"));
        m_send->setFixedWidth(100);
        m_send->setEnabled(false);
        composerLayout->addWidget(m_send);
        mainLayout->addWidget(composer);
        rootLayout->addWidget(main, 1);
        setCentralWidget(root);

        connect(newChat, &QPushButton::clicked, this, &NativeWindow::newChat);
        connect(memory, &QPushButton::clicked, this, &NativeWindow::showMemory);
        connect(settings, &QPushButton::clicked, this, &NativeWindow::showSettings);
        connect(debug, &QPushButton::clicked, this, &NativeWindow::showDebug);
        connect(m_send, &QPushButton::clicked, this, &NativeWindow::sendMessage);
        connect(m_entry, &QTextEdit::textChanged, this, [this]() {
            if (!m_waiting && !m_typing) m_send->setEnabled(!m_entry->toPlainText().trimmed().isEmpty());
        });
        connect(m_chatList, &QListWidget::currentRowChanged, this, &NativeWindow::selectChat);
        applyStyle();
    }

    void connectBackend() {
        connect(m_backend, &QProcess::readyReadStandardOutput, this, &NativeWindow::readBackend);
        connect(m_backend, &QProcess::readyReadStandardError, this, [this]() {
            const QString text = QString::fromUtf8(m_backend->readAllStandardError()).trimmed();
            if (!text.isEmpty()) log(QStringLiteral("backend stderr: %1").arg(text));
        });
        connect(m_backend, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
                [this](int code, QProcess::ExitStatus status) {
            log(QStringLiteral("backend finished code=%1 status=%2")
                .arg(code)
                .arg(status == QProcess::NormalExit ? QStringLiteral("normal") : QStringLiteral("crashed")));
            if (status == QProcess::CrashExit) {
                m_status->setText(QStringLiteral("Backend crashed"));
            }
        });
    }

    void applyStyle() {
        setStyleSheet(QStringLiteral(
            "QMainWindow,QWidget{background:#0e1117;color:#eef2f7;font-size:11pt;}"
            "QLabel{background:transparent;}"
            "#sidebar{background:#151922;border:1px solid #242b38;border-radius:16px;}"
            "#brand{font-size:23pt;font-weight:750;}"
            "#subtitle,#settingsNote{color:#8791a1;font-size:9.5pt;}"
            "#sectionLabel{color:#8f9aaa;font-weight:650;padding-top:8px;}"
            "#header{font-size:17pt;font-weight:700;}"
            "#status{color:#76d69a;}"
            "QPushButton,QComboBox{background:#191e28;color:#eef2f7;border:1px solid #2a3240;border-radius:10px;padding:9px 12px;}"
            "QPushButton:hover{border-color:#5b8cff;background:#1d2430;}"
            "QPushButton#primaryButton{background:#5b8cff;color:white;border:0;font-weight:700;}"
            "QListWidget{background:#11151d;color:#eef2f7;border:1px solid #242b38;border-radius:10px;padding:5px;}"
            "QListWidget::item{padding:10px;border-radius:8px;}"
            "QListWidget::item:selected{background:#283757;color:white;}"
            "#composer{background:#151922;border:1px solid #242b38;border-radius:15px;}"
            "QTextEdit,QPlainTextEdit,QLineEdit{background:#10141b;color:#eef2f7;border:1px solid #2a3240;border-radius:11px;padding:10px;}"
            "QMenu{background:#171d27;color:#eef2f7;border:1px solid #2a3342;}"
            "QMenu::item:selected{background:#2b3b5e;color:white;}"
        ));
        qApp->setFont(applicationFont());
    }

    QString pythonProgram() const {
        const QString root = QCoreApplication::applicationDirPath() + QStringLiteral("/..");
        const QString venvPython = root + QStringLiteral("/.venv/bin/python");
        return QFileInfo::exists(venvPython) ? venvPython : QStringLiteral("python3");
    }

    QProcessEnvironment backendEnvironment() const {
        QSettings settings;
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("PROJECT_PROVIDER"), providerName());

        const auto setSavedOrKeepEnvironment = [&env, &settings](const char* key, const QString& settingsKey) {
            const QString value = settings.value(QString::fromUtf8(settingsKey)).toString().trimmed();
            if (!value.isEmpty()) env.insert(QString::fromUtf8(key), value);
        };
        setSavedOrKeepEnvironment("HF_TOKEN", QStringLiteral("hf_token"));
        setSavedOrKeepEnvironment("HF_MODEL", QStringLiteral("hf_model"));
        setSavedOrKeepEnvironment("OPENAI_API_KEY", QStringLiteral("openai_key"));
        setSavedOrKeepEnvironment("OPENAI_BASE_URL", QStringLiteral("openai_base_url"));
        setSavedOrKeepEnvironment("OPENAI_MODEL", QStringLiteral("openai_model"));
        env.insert(QStringLiteral("PYTHONUNBUFFERED"), QStringLiteral("1"));
        return env;
    }

    bool ensureBackend() {
        if (m_backend->state() == QProcess::Running) return true;
        if (m_backend->state() == QProcess::NotRunning) startBackend();
        if (!m_backend->waitForStarted(1500)) {
            m_status->setText(QStringLiteral("Backend unavailable"));
            log(QStringLiteral("backend start failed: %1").arg(m_backend->errorString()));
            return false;
        }
        return true;
    }

    void startBackend() {
        if (m_backend->state() != QProcess::NotRunning) return;
        const QString root = QCoreApplication::applicationDirPath() + QStringLiteral("/..");
        m_backend->setProcessEnvironment(backendEnvironment());
        m_backend->setProgram(pythonProgram());
        m_backend->setArguments({QStringLiteral("-m"), QStringLiteral("src.backend_bridge")});
        m_backend->setWorkingDirectory(root);
        m_backend->start();
        log(QStringLiteral("backend start %1 provider=%2").arg(m_backend->program(), providerName()));
    }

    void stopBackend() {
        if (m_backend->state() == QProcess::NotRunning) return;
        m_backend->terminate();
        if (!m_backend->waitForFinished(700)) m_backend->kill();
    }

    void sendRequest(QJsonObject object) {
        if (!ensureBackend()) return;
        m_backend->write(QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n');
    }

    void requestChats() {
        QJsonObject request;
        request.insert(QStringLiteral("action"), QStringLiteral("list_chats"));
        sendRequest(request);
    }

    void selectChat(int row) {
        if (row < 0 || row >= m_chatNames.size()) return;
        const QString name = m_chatNames.at(row);
        m_activeChat = name;
        ++m_requestSerial;
        const int requestId = m_requestSerial;
        QJsonObject request;
        request.insert(QStringLiteral("action"), QStringLiteral("select_chat"));
        request.insert(QStringLiteral("name"), name);
        request.insert(QStringLiteral("request_id"), requestId);
        m_waiting = false;
        m_typing = false;
        m_typeTimer.stop();
        m_status->setText(QStringLiteral("Loading…"));
        sendRequest(request);
    }

    void newChat() {
        bool ok = false;
        const QString requested = QInputDialog::getText(this, QStringLiteral("New chat"), QStringLiteral("Chat name:"), QLineEdit::Normal, QString(), &ok).trimmed();
        if (!ok || requested.isEmpty()) return;
        QJsonObject request;
        request.insert(QStringLiteral("action"), QStringLiteral("new_chat"));
        request.insert(QStringLiteral("name"), requested);
        sendRequest(request);
    }

    void sendMessage() {
        const QString text = m_entry->toPlainText().trimmed();
        if (text.isEmpty() || m_waiting || m_typing || m_activeChat.isEmpty()) return;
        ++m_requestSerial;
        m_pendingReplyId = m_requestSerial;
        m_pendingReplyChat = m_activeChat;
        QJsonObject request;
        request.insert(QStringLiteral("action"), QStringLiteral("reply"));
        request.insert(QStringLiteral("text"), text);
        request.insert(QStringLiteral("request_id"), m_pendingReplyId);
        request.insert(QStringLiteral("chat"), m_pendingReplyChat);
        addBubble(QStringLiteral("You"), text, false);
        m_entry->clear();
        m_waiting = true;
        m_send->setEnabled(false);
        m_status->setText(QStringLiteral("Thinking…"));
        sendRequest(request);
    }

    void readBackend() {
        while (m_backend->canReadLine()) {
            const QByteArray line = m_backend->readLine().trimmed();
            if (line.isEmpty()) continue;
            log(QString::fromUtf8(line));
            const QJsonDocument document = QJsonDocument::fromJson(line);
            if (!document.isObject()) continue;
            const QJsonObject object = document.object();
            const QString action = object.value(QStringLiteral("action")).toString();

            if (action == QStringLiteral("chat_list")) {
                m_chatNames.clear();
                m_chatList->clear();
                for (const auto& value : object.value(QStringLiteral("chats")).toArray()) {
                    const QJsonObject chat = value.toObject();
                    const QString name = chat.value(QStringLiteral("name")).toString();
                    const QString title = chat.value(QStringLiteral("title")).toString();
                    m_chatNames << name;
                    m_chatList->addItem(title.isEmpty() ? name : title);
                }
                const QString current = object.value(QStringLiteral("current")).toString();
                const int row = m_chatNames.indexOf(current);
                if (row >= 0) {
                    m_activeChat = current;
                    m_chatList->blockSignals(true);
                    m_chatList->setCurrentRow(row);
                    m_chatList->blockSignals(false);
                    loadMessages(object.value(QStringLiteral("messages")).toArray(), current);
                }
                continue;
            }

            if (action == QStringLiteral("history")) {
                const int requestId = object.value(QStringLiteral("request_id")).toInt();
                const QString name = object.value(QStringLiteral("name")).toString();
                if (requestId == m_requestSerial && name == m_activeChat) {
                    loadMessages(object.value(QStringLiteral("messages")).toArray(), name);
                }
                continue;
            }

            if (action == QStringLiteral("created")) {
                m_activeChat = object.value(QStringLiteral("name")).toString();
                requestChats();
                continue;
            }

            if (action == QStringLiteral("memory")) {
                QStringList memories;
                for (const auto& value : object.value(QStringLiteral("memories")).toArray()) memories << value.toString();
                QMessageBox::information(this, QStringLiteral("Memory"), memories.isEmpty() ? QStringLiteral("No saved memories.") : memories.join(QStringLiteral("\n\n")));
                continue;
            }

            if (action == QStringLiteral("reply")) {
                const int requestId = object.value(QStringLiteral("request_id")).toInt();
                const QString name = object.value(QStringLiteral("name")).toString();
                if (requestId != m_pendingReplyId || name != m_activeChat) continue;
                if (!object.value(QStringLiteral("ok")).toBool()) {
                    m_waiting = false;
                    m_send->setEnabled(!m_entry->toPlainText().trimmed().isEmpty());
                    m_status->setText(QStringLiteral("AI error"));
                    QMessageBox::warning(this, QStringLiteral("AI backend"), object.value(QStringLiteral("error")).toString());
                    continue;
                }
                m_typingBubble = addBubble(QStringLiteral("Vaxx"), QStringLiteral("▌"), true);
                m_typedText = object.value(QStringLiteral("answer")).toString();
                m_typeIndex = 0;
                m_waiting = false;
                m_typing = true;
                m_typeTimer.start();
                m_status->setText(QStringLiteral("Typing…"));
                continue;
            }

            if (!object.value(QStringLiteral("ok")).toBool() && !object.value(QStringLiteral("error")).toString().isEmpty()) {
                m_waiting = false;
                m_send->setEnabled(!m_entry->toPlainText().trimmed().isEmpty());
                m_status->setText(QStringLiteral("Error"));
                QMessageBox::warning(this, QStringLiteral("AI backend"), object.value(QStringLiteral("error")).toString());
            }
        }
    }

    void loadMessages(const QJsonArray& messages, const QString& name) {
        clearConversation();
        m_title->setText(name.isEmpty() ? QStringLiteral("Chat") : name);
        for (const auto& value : messages) {
            const QJsonObject message = value.toObject();
            const QString role = message.value(QStringLiteral("role")).toString();
            const QString content = message.value(QStringLiteral("content")).toString();
            if (role == QStringLiteral("user")) addBubble(QStringLiteral("You"), content, false);
            else if (role == QStringLiteral("assistant")) addBubble(QStringLiteral("Vaxx"), content, true);
        }
        m_waiting = false;
        m_typing = false;
        m_status->setText(QStringLiteral("Ready"));
    }

    void clearConversation() {
        while (m_conversationLayout->count() > 1) {
            auto* item = m_conversationLayout->takeAt(0);
            if (auto* widget = item->widget()) widget->deleteLater();
            delete item;
        }
        m_typingBubble = nullptr;
        m_typedText.clear();
        m_typeIndex = 0;
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

    void typeNext() {
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
        m_send->setEnabled(!m_entry->toPlainText().trimmed().isEmpty());
        m_status->setText(QStringLiteral("Ready"));
        requestChats();
    }

    void showMemory() {
        QJsonObject request;
        request.insert(QStringLiteral("action"), QStringLiteral("memory"));
        sendRequest(request);
    }

    void showSettings() {
        if (m_waiting || m_typing) {
            QMessageBox::information(this, QStringLiteral("Settings"), QStringLiteral("Finish the current response before changing provider settings."));
            return;
        }
        SettingsDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted) {
            dialog.saveSettings();
            restartBackend();
            QTimer::singleShot(120, this, &NativeWindow::requestChats);
        }
    }

    void showDebug() {
        auto* output = new QPlainTextEdit;
        output->setReadOnly(true);
        output->setPlainText(m_logs.join(QStringLiteral("\n")));
        output->setWindowTitle(QStringLiteral("Debug mode"));
        output->resize(860, 540);
        output->show();
    }

    void restartBackend() {
        stopBackend();
        startBackend();
    }

    void scrollToBottom() {
        QTimer::singleShot(0, this, [this]() {
            auto* bar = m_scroll->verticalScrollBar();
            bar->setValue(bar->maximum());
        });
    }

    void log(const QString& message) {
        m_logs << QStringLiteral("[%1] %2").arg(QDateTime::currentDateTime().toString(Qt::ISODate), message);
    }

    QProcess* m_backend = nullptr;
    QListWidget* m_chatList = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_status = nullptr;
    QScrollArea* m_scroll = nullptr;
    QWidget* m_conversation = nullptr;
    QVBoxLayout* m_conversationLayout = nullptr;
    QTextEdit* m_entry = nullptr;
    QPushButton* m_send = nullptr;
    QStringList m_chatNames;
    QStringList m_logs;
    QString m_activeChat;
    QString m_pendingReplyChat;
    QTimer m_typeTimer;
    ChatBubble* m_typingBubble = nullptr;
    QString m_typedText;
    int m_typeIndex = 0;
    int m_requestSerial = 0;
    int m_pendingReplyId = 0;
    bool m_waiting = false;
    bool m_typing = false;
};

} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("Vaxx"));
    app.setApplicationName(QStringLiteral("AI Chat"));
    app.setFont(applicationFont());
    NativeWindow window;
    window.show();
    return app.exec();
}
