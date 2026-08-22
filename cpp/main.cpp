#include "ChatBubble.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QFileInfo>
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
    QSettings s;
    const QString saved = s.value(QStringLiteral("provider")).toString().trimmed();
    if (saved == QStringLiteral("openai") || saved == QStringLiteral("huggingface")) return saved;
    return qEnvironmentVariable("OPENAI_API_KEY").trimmed().isEmpty()
        ? QStringLiteral("huggingface") : QStringLiteral("openai");
}

bool hasCredential(const QString& provider) {
    QSettings s;
    const QString key = provider == QStringLiteral("openai")
        ? s.value(QStringLiteral("openai_key")).toString().trimmed()
        : s.value(QStringLiteral("hf_token")).toString().trimmed();
    if (!key.isEmpty()) return true;
    return provider == QStringLiteral("openai")
        ? !qEnvironmentVariable("OPENAI_API_KEY").trimmed().isEmpty()
        : !qEnvironmentVariable("HF_TOKEN").trimmed().isEmpty();
}

QFont uiFont() {
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
        auto* gf = new QFormLayout(general);
        m_name = new QLineEdit(general);
        m_appearance = new QComboBox(general);
        m_appearance->addItems({QStringLiteral("Dark"), QStringLiteral("Light"), QStringLiteral("System")});
        m_performance = new QComboBox(general);
        m_performance->addItems({QStringLiteral("Low GPU"), QStringLiteral("Balanced"), QStringLiteral("Smooth")});
        m_stream = new QCheckBox(QStringLiteral("Enable streaming responses"), general);
        m_autoMemory = new QCheckBox(QStringLiteral("Automatically remember simple facts"), general);
        m_autoSummary = new QCheckBox(QStringLiteral("Automatically summarize long chats"), general);
        gf->addRow(QStringLiteral("AI name"), m_name);
        gf->addRow(QStringLiteral("Appearance"), m_appearance);
        gf->addRow(QStringLiteral("UI performance"), m_performance);
        gf->addRow(m_stream);
        gf->addRow(m_autoMemory);
        gf->addRow(m_autoSummary);
        tabs->addTab(general, QStringLiteral("General"));

        auto* provider = new QWidget(this);
        auto* pf = new QFormLayout(provider);
        m_provider = new QComboBox(provider);
        m_provider->addItems({QStringLiteral("Hugging Face"), QStringLiteral("OpenAI-compatible")});
        pf->addRow(QStringLiteral("Provider"), m_provider);
        m_pages = new QStackedWidget(provider);
        pf->addRow(m_pages);

        auto* hf = new QWidget(m_pages);
        auto* hfForm = new QFormLayout(hf);
        m_hfToken = new QLineEdit(hf);
        m_hfToken->setEchoMode(QLineEdit::Password);
        m_hfModel = new QComboBox(hf);
        m_hfModel->addItems(kHfModels);
        hfForm->addRow(QStringLiteral("HF token"), m_hfToken);
        hfForm->addRow(QStringLiteral("Model"), m_hfModel);
        m_pages->addWidget(hf);

        auto* openai = new QWidget(m_pages);
        auto* oaForm = new QFormLayout(openai);
        m_openAiKey = new QLineEdit(openai);
        m_openAiKey->setEchoMode(QLineEdit::Password);
        m_endpoint = new QLineEdit(openai);
        m_openAiModel = new QComboBox(openai);
        m_openAiModel->addItems(kOpenAiModels);
        oaForm->addRow(QStringLiteral("API key"), m_openAiKey);
        oaForm->addRow(QStringLiteral("Endpoint"), m_endpoint);
        oaForm->addRow(QStringLiteral("Model"), m_openAiModel);
        m_pages->addWidget(openai);
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
        connect(save, &QPushButton::clicked, this, [this]() { saveSettings(); accept(); });
    }

    void saveSettings() {
        QSettings s;
        const bool openai = m_provider->currentIndex() == 1;
        s.setValue(QStringLiteral("provider"), openai ? QStringLiteral("openai") : QStringLiteral("huggingface"));
        s.setValue(QStringLiteral("ai_name"), m_name->text().trimmed().left(40));
        s.setValue(QStringLiteral("appearance"), m_appearance->currentText());
        s.setValue(QStringLiteral("ui_performance"), m_performance->currentText());
        s.setValue(QStringLiteral("stream"), m_stream->isChecked());
        s.setValue(QStringLiteral("auto_memory"), m_autoMemory->isChecked());
        s.setValue(QStringLiteral("auto_summary"), m_autoSummary->isChecked());
        s.setValue(QStringLiteral("hf_token"), m_hfToken->text().trimmed());
        s.setValue(QStringLiteral("hf_model"), m_hfModel->currentText());
        s.setValue(QStringLiteral("openai_key"), m_openAiKey->text().trimmed());
        s.setValue(QStringLiteral("openai_base_url"), m_endpoint->text().trimmed());
        s.setValue(QStringLiteral("openai_model"), m_openAiModel->currentText());
        s.sync();
    }

private:
    void load() {
        QSettings s;
        const QString p = providerName();
        m_provider->setCurrentIndex(p == QStringLiteral("openai") ? 1 : 0);
        m_pages->setCurrentIndex(p == QStringLiteral("openai") ? 1 : 0);
        m_name->setText(s.value(QStringLiteral("ai_name"), QStringLiteral("Vaxx")).toString());
        m_appearance->setCurrentText(s.value(QStringLiteral("appearance"), QStringLiteral("Dark")).toString());
        m_performance->setCurrentText(s.value(QStringLiteral("ui_performance"), QStringLiteral("Balanced")).toString());
        m_stream->setChecked(s.value(QStringLiteral("stream"), true).toBool());
        m_autoMemory->setChecked(s.value(QStringLiteral("auto_memory"), true).toBool());
        m_autoSummary->setChecked(s.value(QStringLiteral("auto_summary"), true).toBool());

        QString hf = s.value(QStringLiteral("hf_token")).toString();
        if (hf.isEmpty()) hf = qEnvironmentVariable("HF_TOKEN");
        m_hfToken->setText(hf);
        QString oa = s.value(QStringLiteral("openai_key")).toString();
        if (oa.isEmpty()) oa = qEnvironmentVariable("OPENAI_API_KEY");
        m_openAiKey->setText(oa);
        m_endpoint->setText(s.value(QStringLiteral("openai_base_url"), QStringLiteral("https://api.openai.com/v1/chat/completions")).toString());
        m_hfModel->setCurrentText(s.value(QStringLiteral("hf_model"), kHfModels.first()).toString());
        m_openAiModel->setCurrentText(s.value(QStringLiteral("openai_model"), kOpenAiModels.first()).toString());
    }

    QLineEdit* m_name = nullptr;
    QComboBox* m_appearance = nullptr;
    QComboBox* m_performance = nullptr;
    QCheckBox* m_stream = nullptr;
    QCheckBox* m_autoMemory = nullptr;
    QCheckBox* m_autoSummary = nullptr;
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
        if (!hasCredential(providerName())) QTimer::singleShot(250, this, &NativeWindow::showSettings);
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
        auto* header = new QHBoxLayout;
        m_title = new QLabel(QStringLiteral("Chat"), main);
        m_title->setObjectName(QStringLiteral("header"));
        header->addWidget(m_title);
        header->addStretch();
        m_status = new QLabel(QStringLiteral("Ready"), main);
        m_status->setObjectName(QStringLiteral("status"));
        header->addWidget(m_status);
        mainLayout->addLayout(header);

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
        m_entry->setFont(uiFont());
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
            if (status == QProcess::CrashExit) m_status->setText(QStringLiteral("Backend crashed"));
        });
    }

    void applyStyle() {
        setStyleSheet(QStringLiteral(
            "QMainWindow,QWidget{background:#0e1117;color:#eef2f7;font-size:11pt;}"
            "QLabel{background:transparent;}"
            "#sidebar{background:#151922;border:1px solid #242b38;border-radius:16px;}"
            "#brand{font-size:23pt;font-weight:750;}"
            "#subtitle{color:#8791a1;font-size:9.5pt;}"
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
            "QComboBox QAbstractItemView{background:#151922;color:#eef2f7;border:1px solid #2a3240;selection-background-color:#283757;selection-color:#fff;}"
            "QMenu{background:#171d27;color:#eef2f7;border:1px solid #2a3342;padding:5px;}"
            "QMenu::item{background:transparent;padding:7px 18px;border-radius:6px;}"
            "QMenu::item:selected{background:#2b3b5e;color:white;}"
        ));
        qApp->setFont(uiFont());
    }

    QString pythonProgram() const {
        const QString root = QCoreApplication::applicationDirPath() + QStringLiteral("/..");
        const QString venv = root + QStringLiteral("/.venv/bin/python");
        return QFileInfo::exists(venv) ? venv : QStringLiteral("python3");
    }

    QProcessEnvironment backendEnvironment() const {
        QSettings s;
        auto env = QProcessEnvironment::systemEnvironment();
        const QString provider = providerName();
        env.insert(QStringLiteral("PROJECT_PROVIDER"), provider);

        QString hf = s.value(QStringLiteral("hf_token")).toString().trimmed();
        if (hf.isEmpty()) hf = qEnvironmentVariable("HF_TOKEN");
        env.insert(QStringLiteral("HF_TOKEN"), hf);
        env.insert(QStringLiteral("HF_MODEL"), s.value(QStringLiteral("hf_model"), kHfModels.first()).toString());

        QString openai = s.value(QStringLiteral("openai_key")).toString().trimmed();
        if (openai.isEmpty()) openai = qEnvironmentVariable("OPENAI_API_KEY");
        env.insert(QStringLiteral("OPENAI_API_KEY"), openai);
        env.insert(QStringLiteral("OPENAI_BASE_URL"), s.value(QStringLiteral("openai_base_url"), QStringLiteral("https://api.openai.com/v1/chat/completions")).toString());
        env.insert(QStringLiteral("OPENAI_MODEL"), s.value(QStringLiteral("openai_model"), kOpenAiModels.first()).toString());
        env.insert(QStringLiteral("PYTHONUNBUFFERED"), QStringLiteral("1"));
        return env;
    }

    void startBackend() {
        if (m_backend->state() != QProcess::NotRunning) return;
        const QString root = QCoreApplication::applicationDirPath() + QStringLiteral("/..");
        m_backend->setProcessEnvironment(backendEnvironment());
        m_backend->setProgram(pythonProgram());
        m_backend->setArguments({QStringLiteral("-m"), QStringLiteral("src.backend_bridge")});
        m_backend->setWorkingDirectory(root);
        m_backend->start();
        log(QStringLiteral("backend start provider=%1 program=%2").arg(providerName(), m_backend->program()));
    }

    void stopBackend() {
        if (m_backend->state() == QProcess::NotRunning) return;
        m_backend->terminate();
        if (!m_backend->waitForFinished(700)) m_backend->kill();
    }

    void sendJson(QJsonObject object) {
        if (m_backend->state() != QProcess::Running) startBackend();
        m_backend->write(QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n');
    }

    void requestChats() {
        sendJson({{QStringLiteral("action"), QStringLiteral("list_chats")}});
    }

    void selectChat(int row) {
        if (row < 0 || row >= m_chatNames.size()) return;
        m_requestSerial++;
        m_activeRequest = m_requestSerial;
        m_waiting = false;
        m_typing = false;
        m_typeTimer.stop();
        clearTypingState();

        const QString name = m_chatNames.at(row);
        sendJson({
            {QStringLiteral("action"), QStringLiteral("select_chat")},
            {QStringLiteral("name"), name},
            {QStringLiteral("request_id"), m_activeRequest}
        });
        setStatus(QStringLiteral("Loading chat…"));
        log(QStringLiteral("select chat=%1 request=%2").arg(name).arg(m_activeRequest));
    }

    void newChat() {
        bool ok = false;
        const QString name = QInputDialog::getText(this, QStringLiteral("New chat"), QStringLiteral("Chat name:"), QLineEdit::Normal, QString(), &ok).trimmed();
        if (!ok || name.isEmpty()) return;
        sendJson({{QStringLiteral("action"), QStringLiteral("new_chat")}, {QStringLiteral("name"), name}});
    }

    void sendMessage() {
        const QString text = m_entry->toPlainText().trimmed();
        if (text.isEmpty() || m_waiting || m_typing || m_currentChat.isEmpty()) return;

        const int requestId = ++m_requestSerial;
        m_activeRequest = requestId;
        const QString chat = m_currentChat;
        addBubble(QStringLiteral("You"), text, false);
        m_entry->clear();
        m_waiting = true;
        m_send->setEnabled(false);
        setStatus(QStringLiteral("Thinking…"));

        sendJson({
            {QStringLiteral("action"), QStringLiteral("reply")},
            {QStringLiteral("text"), text},
            {QStringLiteral("chat"), chat},
            {QStringLiteral("request_id"), requestId}
        });
        log(QStringLiteral("request chat=%1 id=%2 chars=%3").arg(chat).arg(requestId).arg(text.size()));
    }

    void readBackend() {
        while (m_backend->canReadLine()) {
            const QByteArray line = m_backend->readLine().trimmed();
            if (line.isEmpty()) continue;
            const QJsonDocument doc = QJsonDocument::fromJson(line);
            if (!doc.isObject()) continue;
            const QJsonObject o = doc.object();
            const QString action = o.value(QStringLiteral("action")).toString();
            const int requestId = o.value(QStringLiteral("request_id")).toInt(-1);
            log(QStringLiteral("backend action=%1 request=%2").arg(action).arg(requestId));

            if (!o.value(QStringLiteral("ok")).toBool()) {
                if (requestId >= 0 && requestId != m_activeRequest) continue;
                m_waiting = false;
                m_typing = false;
                m_send->setEnabled(!m_entry->toPlainText().trimmed().isEmpty());
                setStatus(QStringLiteral("AI error"));
                QMessageBox::warning(this, QStringLiteral("AI backend"), o.value(QStringLiteral("error")).toString());
                continue;
            }

            if (action == QStringLiteral("chat_list")) {
                populateChatList(o);
                continue;
            }

            if (action == QStringLiteral("history")) {
                if (requestId >= 0 && requestId != m_activeRequest) continue;
                const QString name = o.value(QStringLiteral("name")).toString();
                m_currentChat = name;
                loadMessages(o.value(QStringLiteral("messages")).toArray(), name);
                continue;
            }

            if (action == QStringLiteral("created")) {
                requestChats();
                continue;
            }

            if (action == QStringLiteral("memory")) {
                QStringList values;
                for (const auto& value : o.value(QStringLiteral("memories")).toArray()) values << value.toString();
                QMessageBox::information(this, QStringLiteral("Memory"), values.isEmpty() ? QStringLiteral("No saved memories.") : values.join(QStringLiteral("\n\n")));
                continue;
            }

            if (action == QStringLiteral("reply")) {
                const QString name = o.value(QStringLiteral("name")).toString();
                if (requestId != m_activeRequest || name != m_currentChat || !m_waiting) {
                    log(QStringLiteral("ignored stale reply chat=%1 request=%2 current=%3 active=%4")
                        .arg(name).arg(requestId).arg(m_currentChat).arg(m_activeRequest));
                    continue;
                }
                m_typedText = o.value(QStringLiteral("answer")).toString();
                m_typeIndex = 0;
                m_typingBubble = addBubble(QStringLiteral("Vaxx"), QStringLiteral("▌"), true);
                m_waiting = false;
                m_typing = true;
                setStatus(QStringLiteral("Typing…"));
                m_typeTimer.start();
            }
        }
    }

    void populateChatList(const QJsonObject& object) {
        const QString current = object.value(QStringLiteral("current")).toString();
        m_chatNames.clear();
        m_chatList->clear();
        const QJsonArray chats = object.value(QStringLiteral("chats")).toArray();
        for (const auto& value : chats) {
            const QJsonObject chat = value.toObject();
            const QString name = chat.value(QStringLiteral("name")).toString();
            const QString title = chat.value(QStringLiteral("title")).toString();
            m_chatNames << name;
            m_chatList->addItem(title.isEmpty() ? name : title);
        }

        int row = m_chatNames.indexOf(m_currentChat);
        if (row < 0) row = m_chatNames.indexOf(current);
        if (row < 0 && !m_chatNames.isEmpty()) row = 0;
        if (row >= 0) {
            m_chatList->blockSignals(true);
            m_chatList->setCurrentRow(row);
            m_chatList->blockSignals(false);
            m_currentChat = m_chatNames.at(row);
        }

        if (row >= 0 && !m_currentChat.isEmpty() && object.value(QStringLiteral("messages")).isArray()) {
            loadMessages(object.value(QStringLiteral("messages")).toArray(), m_currentChat);
        }
    }

    void loadMessages(const QJsonArray& messages, const QString& name) {
        clearConversation();
        if (!name.isEmpty()) {
            m_title->setText(name);
            m_currentChat = name;
        }
        for (const auto& value : messages) {
            const QJsonObject message = value.toObject();
            const QString role = message.value(QStringLiteral("role")).toString();
            const QString content = message.value(QStringLiteral("content")).toString();
            if (role == QStringLiteral("user")) addBubble(QStringLiteral("You"), content, false);
            else if (role == QStringLiteral("assistant")) addBubble(QStringLiteral("Vaxx"), content, true);
        }
        setStatus(QStringLiteral("Ready"));
    }

    void showMemory() {
        sendJson({{QStringLiteral("action"), QStringLiteral("memory")}});
    }

    void showSettings() {
        SettingsDialog dialog(this);
        if (dialog.exec() != QDialog::Accepted) return;
        restartBackend();
        QTimer::singleShot(150, this, &NativeWindow::requestChats);
        setStatus(QStringLiteral("Settings saved"));
    }

    void showDebug() {
        auto* dialog = new QDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle(QStringLiteral("Debug mode"));
        dialog->resize(900, 560);
        auto* layout = new QVBoxLayout(dialog);
        auto* output = new QPlainTextEdit(dialog);
        output->setReadOnly(true);
        output->setPlainText(m_logs.join(QStringLiteral("\n")));
        layout->addWidget(output, 1);
        dialog->show();
    }

    ChatBubble* addBubble(const QString& sender, const QString& text, bool assistant) {
        auto* row = new QWidget(m_conversation);
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        auto* bubble = new ChatBubble(sender, text, assistant, row);
        if (assistant) {
            layout->addStretch();
            layout->addWidget(bubble, 0, Qt::AlignRight);
        } else {
            layout->addWidget(bubble, 0, Qt::AlignLeft);
            layout->addStretch();
        }
        m_conversationLayout->insertWidget(m_conversationLayout->count() - 1, row);
        scrollToBottom();
        return bubble;
    }

    void clearConversation() {
        while (m_conversationLayout->count() > 1) {
            auto* item = m_conversationLayout->takeAt(0);
            if (auto* widget = item->widget()) widget->deleteLater();
            delete item;
        }
        clearTypingState();
    }

    void clearTypingState() {
        m_typingBubble = nullptr;
        m_typedText.clear();
        m_typeIndex = 0;
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
        clearTypingState();
        m_typing = false;
        m_waiting = false;
        m_send->setEnabled(!m_entry->toPlainText().trimmed().isEmpty());
        setStatus(QStringLiteral("Ready"));
        requestChats();
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

    void setStatus(const QString& text) { m_status->setText(text); }
    void log(const QString& text) {
        m_logs << QStringLiteral("[%1] %2").arg(QDateTime::currentDateTime().toString(Qt::ISODate), text);
        if (m_logs.size() > 400) m_logs.removeFirst();
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
    QString m_currentChat;
    QString m_typedText;
    ChatBubble* m_typingBubble = nullptr;
    QTimer m_typeTimer;
    int m_typeIndex = 0;
    int m_requestSerial = 0;
    int m_activeRequest = 0;
    bool m_waiting = false;
    bool m_typing = false;
};

} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("Vaxx"));
    app.setApplicationName(QStringLiteral("AI Chat"));
    app.setFont(uiFont());
    NativeWindow window;
    window.show();
    return app.exec();
}
