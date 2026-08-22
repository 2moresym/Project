#include "ChatBubble.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QFileInfo>
#include <QFont>
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
#include <QMenu>
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
    QStringLiteral("openai/gpt-oss-20b:groq"),
    QStringLiteral("Qwen/Qwen3-Coder-480B-A35B-Instruct"),
    QStringLiteral("Qwen/Qwen2.5-7B-Instruct-1M"),
    QStringLiteral("deepseek-ai/DeepSeek-R1")
};

const QStringList kOpenAiModels = {
    QStringLiteral("gpt-4o-mini"),
    QStringLiteral("gpt-4.1-mini"),
    QStringLiteral("gpt-4.1"),
    QStringLiteral("gpt-5-mini")
};

bool hasCredential(const QString& provider) {
    QSettings s;
    const bool hf = !s.value(QStringLiteral("hf_token")).toString().trimmed().isEmpty() ||
                    !qEnvironmentVariable("HF_TOKEN").trimmed().isEmpty();
    const bool openai = !s.value(QStringLiteral("openai_key")).toString().trimmed().isEmpty() ||
                        !qEnvironmentVariable("OPENAI_API_KEY").trimmed().isEmpty();
    if (provider == QStringLiteral("openai")) return openai;
    return hf || openai;
}

class SettingsDialog final : public QDialog {
public:
    explicit SettingsDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle(QStringLiteral("Settings"));
        resize(620, 520);

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(24, 24, 24, 24);
        root->setSpacing(14);

        auto* tabs = new QTabWidget(this);
        root->addWidget(tabs, 1);

        auto* general = new QWidget;
        auto* generalForm = new QFormLayout(general);
        generalForm->setContentsMargins(12, 12, 12, 12);
        generalForm->setSpacing(12);
        m_aiName = new QLineEdit(this);
        m_appearance = new QComboBox(this);
        m_appearance->addItems({QStringLiteral("Dark"), QStringLiteral("Light"), QStringLiteral("System")});
        m_accent = new QComboBox(this);
        m_accent->addItems({QStringLiteral("Default"), QStringLiteral("Cyan"), QStringLiteral("Green"), QStringLiteral("Magenta")});
        m_performance = new QComboBox(this);
        m_performance->addItems({QStringLiteral("Low GPU"), QStringLiteral("Balanced"), QStringLiteral("Smooth")});
        m_stream = new QCheckBox(QStringLiteral("Enable streaming responses"), this);
        m_autoMemory = new QCheckBox(QStringLiteral("Automatically remember simple personal facts"), this);
        m_autoSummary = new QCheckBox(QStringLiteral("Automatically summarize long conversations"), this);
        generalForm->addRow(QStringLiteral("AI name"), m_aiName);
        generalForm->addRow(QStringLiteral("Appearance"), m_appearance);
        generalForm->addRow(QStringLiteral("Accent"), m_accent);
        generalForm->addRow(QStringLiteral("UI performance"), m_performance);
        generalForm->addRow(m_stream);
        generalForm->addRow(m_autoMemory);
        generalForm->addRow(m_autoSummary);
        tabs->addTab(general, QStringLiteral("General"));

        auto* provider = new QWidget;
        auto* providerForm = new QFormLayout(provider);
        providerForm->setContentsMargins(12, 12, 12, 12);
        providerForm->setSpacing(12);

        m_provider = new QComboBox(this);
        m_provider->addItems({QStringLiteral("Hugging Face"), QStringLiteral("OpenAI-compatible")});
        providerForm->addRow(QStringLiteral("Provider"), m_provider);

        m_providerPages = new QStackedWidget(this);
        providerForm->addRow(m_providerPages);

        auto* hfPage = new QWidget;
        auto* hfForm = new QFormLayout(hfPage);
        m_hfToken = new QLineEdit(this);
        m_hfToken->setEchoMode(QLineEdit::Password);
        m_hfModel = new QComboBox(this);
        hfForm->addRow(QStringLiteral("HF token"), m_hfToken);
        hfForm->addRow(QStringLiteral("Model"), m_hfModel);
        m_providerPages->addWidget(hfPage);

        auto* openAiPage = new QWidget;
        auto* openAiForm = new QFormLayout(openAiPage);
        m_openAiKey = new QLineEdit(this);
        m_openAiKey->setEchoMode(QLineEdit::Password);
        m_openAiEndpoint = new QLineEdit(this);
        m_openAiEndpoint->setPlaceholderText(QStringLiteral("https://api.openai.com/v1/chat/completions"));
        m_openAiModel = new QComboBox(this);
        openAiForm->addRow(QStringLiteral("API key"), m_openAiKey);
        openAiForm->addRow(QStringLiteral("Endpoint"), m_openAiEndpoint);
        openAiForm->addRow(QStringLiteral("Model"), m_openAiModel);
        m_providerPages->addWidget(openAiPage);

        auto* note = new QLabel(QStringLiteral("Credentials stay in your desktop settings and are never written into the source tree."), this);
        note->setObjectName(QStringLiteral("settingsNote"));
        note->setWordWrap(true);
        providerForm->addRow(note);
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
        connect(m_provider, &QComboBox::currentTextChanged, this, &SettingsDialog::providerChanged);
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
        connect(save, &QPushButton::clicked, this, &SettingsDialog::save);
        providerChanged(m_provider->currentText());
    }

    bool saved() const { return m_saved; }

private:
    void load() {
        QSettings s;
        const QString savedProvider = s.value(QStringLiteral("provider")).toString();
        QString provider = savedProvider;
        if (provider.isEmpty()) {
            if (!qEnvironmentVariable("OPENAI_API_KEY").trimmed().isEmpty()) provider = QStringLiteral("openai");
            else provider = QStringLiteral("huggingface");
        }
        m_provider->setCurrentIndex(provider == QStringLiteral("openai") ? 1 : 0);

        m_aiName->setText(s.value(QStringLiteral("ai_name"), QStringLiteral("Vaxx")).toString());
        m_appearance->setCurrentText(s.value(QStringLiteral("appearance"), QStringLiteral("Dark")).toString());
        m_accent->setCurrentText(s.value(QStringLiteral("accent"), QStringLiteral("Default")).toString());
        m_performance->setCurrentText(s.value(QStringLiteral("ui_performance"), QStringLiteral("Balanced")).toString());
        m_stream->setChecked(s.value(QStringLiteral("stream"), true).toBool());
        m_autoMemory->setChecked(s.value(QStringLiteral("auto_memory"), true).toBool());
        m_autoSummary->setChecked(s.value(QStringLiteral("auto_summary"), true).toBool());

        QString hfToken = s.value(QStringLiteral("hf_token")).toString();
        if (hfToken.isEmpty()) hfToken = qEnvironmentVariable("HF_TOKEN");
        m_hfToken->setText(hfToken);

        QString openAiKey = s.value(QStringLiteral("openai_key")).toString();
        if (openAiKey.isEmpty()) openAiKey = qEnvironmentVariable("OPENAI_API_KEY");
        m_openAiKey->setText(openAiKey);

        m_openAiEndpoint->setText(s.value(QStringLiteral("openai_base_url"), QStringLiteral("https://api.openai.com/v1/chat/completions")).toString());
        fillModels(m_hfModel, kHfModels, s.value(QStringLiteral("hf_model"), kHfModels.first()).toString());
        fillModels(m_openAiModel, kOpenAiModels, s.value(QStringLiteral("openai_model"), kOpenAiModels.first()).toString());
    }

    static void fillModels(QComboBox* box, const QStringList& models, const QString& selected) {
        box->clear();
        box->addItems(models);
        if (models.contains(selected)) box->setCurrentText(selected);
        else {
            box->addItem(selected);
            box->setCurrentText(selected);
        }
    }

    void providerChanged(const QString& text) {
        m_providerPages->setCurrentIndex(text == QStringLiteral("OpenAI-compatible") ? 1 : 0);
    }

    void save() {
        QSettings s;
        const bool openai = m_provider->currentIndex() == 1;
        s.setValue(QStringLiteral("provider"), openai ? QStringLiteral("openai") : QStringLiteral("huggingface"));
        s.setValue(QStringLiteral("ai_name"), m_aiName->text().trimmed().left(40));
        s.setValue(QStringLiteral("appearance"), m_appearance->currentText());
        s.setValue(QStringLiteral("accent"), m_accent->currentText());
        s.setValue(QStringLiteral("ui_performance"), m_performance->currentText());
        s.setValue(QStringLiteral("stream"), m_stream->isChecked());
        s.setValue(QStringLiteral("auto_memory"), m_autoMemory->isChecked());
        s.setValue(QStringLiteral("auto_summary"), m_autoSummary->isChecked());
        s.setValue(QStringLiteral("hf_token"), m_hfToken->text().trimmed());
        s.setValue(QStringLiteral("hf_model"), m_hfModel->currentText());
        s.setValue(QStringLiteral("openai_key"), m_openAiKey->text().trimmed());
        s.setValue(QStringLiteral("openai_base_url"), m_openAiEndpoint->text().trimmed());
        s.setValue(QStringLiteral("openai_model"), m_openAiModel->currentText());
        s.sync();
        m_saved = true;
        accept();
    }

    QComboBox* m_provider = nullptr;
    QStackedWidget* m_providerPages = nullptr;
    QLineEdit* m_aiName = nullptr;
    QComboBox* m_appearance = nullptr;
    QComboBox* m_accent = nullptr;
    QComboBox* m_performance = nullptr;
    QCheckBox* m_stream = nullptr;
    QCheckBox* m_autoMemory = nullptr;
    QCheckBox* m_autoSummary = nullptr;
    QLineEdit* m_hfToken = nullptr;
    QComboBox* m_hfModel = nullptr;
    QLineEdit* m_openAiKey = nullptr;
    QLineEdit* m_openAiEndpoint = nullptr;
    QComboBox* m_openAiModel = nullptr;
    bool m_saved = false;
};

class DebugDialog final : public QDialog {
public:
    explicit DebugDialog(const QStringList& logs, QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle(QStringLiteral("Debug mode"));
        resize(860, 540);
        auto* layout = new QVBoxLayout(this);
        auto* output = new QPlainTextEdit(this);
        output->setReadOnly(true);
        output->setPlainText(logs.join(QStringLiteral("\n")));
        layout->addWidget(output, 1);
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
        const QString provider = s.value(QStringLiteral("provider")).toString().isEmpty()
            ? (!qEnvironmentVariable("OPENAI_API_KEY").trimmed().isEmpty() ? QStringLiteral("openai") : QStringLiteral("huggingface"))
            : s.value(QStringLiteral("provider")).toString();
        if (!hasCredential(provider)) QTimer::singleShot(250, this, &NativeWindow::showSettings);
    }

    ~NativeWindow() override { m_typeTimer.stop(); stopBackend(); }

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

        connect(m_send, &QPushButton::clicked, this, &NativeWindow::sendMessage);
        connect(m_entry, &QTextEdit::textChanged, this, [this]() { if (!m_waiting && !m_typing) m_send->setEnabled(!m_entry->toPlainText().trimmed().isEmpty()); });
        connect(newChat, &QPushButton::clicked, this, &NativeWindow::newChat);
        connect(memory, &QPushButton::clicked, this, &NativeWindow::showMemory);
        connect(settings, &QPushButton::clicked, this, &NativeWindow::showSettings);
        connect(debug, &QPushButton::clicked, this, &NativeWindow::showDebug);
        connect(m_backend, &QProcess::readyReadStandardOutput, this, &NativeWindow::readBackend);
        connect(m_backend, &QProcess::readyReadStandardError, this, [this]() {
            const QString text = QString::fromUtf8(m_backend->readAllStandardError()).trimmed();
            if (!text.isEmpty()) log(QStringLiteral("backend stderr: %1").arg(text));
        });
        connect(m_backend, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
            log(QStringLiteral("backend process error: %1").arg(m_backend->errorString()));
            failRequest(QStringLiteral("Backend process error: %1").arg(m_backend->errorString()));
        });
        connect(m_backend, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
            log(QStringLiteral("backend finished code=%1 status=%2 stderr=%3").arg(code).arg(static_cast<int>(status)).arg(QString::fromUtf8(m_backend->readAllStandardError()).trimmed()));
            if (status == QProcess::CrashExit) failRequest(QStringLiteral("Python backend crashed (exit code %1)").arg(code));
        });
        m_typeTimer.setInterval(14);
        connect(&m_typeTimer, &QTimer::timeout, this, &NativeWindow::typeNextCharacter);
        applyStyle();
    }

    void applyStyle() {
        setStyleSheet(QStringLiteral(
            "QMainWindow,QWidget{background:#0e1117;color:#eef2f7;font-size:11pt;}"
            "QLabel{background:transparent;}"
            "#sidebar{background:#151922;border:1px solid #242b38;border-radius:16px;}"
            "#brand{font-size:23pt;font-weight:750;color:#fff;}"
            "#subtitle,#settingsNote{color:#8791a1;font-size:9.5pt;}"
            "#sectionLabel{color:#8f9aaa;font-weight:650;padding-top:8px;}"
            "#header{font-size:17pt;font-weight:700;padding:5px;}"
            "#status{color:#76d69a;font-size:9.5pt;padding:6px;}"
            "QPushButton,QComboBox{background:#191e28;color:#eef2f7;border:1px solid #2a3240;border-radius:10px;padding:9px 12px;}"
            "QPushButton:hover{border-color:#5b8cff;background:#1d2430;}"
            "QPushButton#primaryButton{background:#5b8cff;color:#fff;border:0;font-weight:700;}"
            "QPushButton#primaryButton:hover{background:#6b99ff;}"
            "QListWidget{background:#11151d;color:#eef2f7;border:1px solid #242b38;border-radius:10px;padding:5px;}"
            "QListWidget::item{padding:10px;border-radius:8px;}"
            "QListWidget::item:selected{background:#283757;color:#fff;}"
            "#composer{background:#151922;border:1px solid #242b38;border-radius:15px;}"
            "QTextEdit,QPlainTextEdit,QLineEdit{background:#10141b;color:#eef2f7;border:1px solid #2a3240;border-radius:11px;padding:10px;}"
            "QComboBox QAbstractItemView{background:#151922;color:#eef2f7;border:1px solid #2a3240;selection-background-color:#283757;selection-color:#fff;}"
            "QMenu{background:#151922;color:#eef2f7;border:1px solid #2a3240;padding:5px;}"
            "QMenu::item{background:transparent;padding:7px 28px 7px 12px;border-radius:6px;}"
            "QMenu::item:selected{background:#283757;color:#fff;}"
            "QMenu::separator{height:1px;background:#2a3240;margin:5px 8px;}"
            "QToolTip{background:#151922;color:#eef2f7;border:1px solid #2a3240;}"
        ));
        QFont f(QStringLiteral("Sans Serif"));
        f.setStyleStrategy(QFont::PreferMatch);
        qApp->setFont(f);
    }

    QString pythonProgram() const {
        const QString root = QCoreApplication::applicationDirPath() + QStringLiteral("/..");
        const QString venv = root + QStringLiteral("/.venv/bin/python");
        return QFileInfo::exists(venv) ? venv : QStringLiteral("python3");
    }

    QProcessEnvironment backendEnvironment() const {
        QSettings s;
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("PROJECT_PROVIDER"), s.value(QStringLiteral("provider"), QStringLiteral("huggingface")).toString());
        env.insert(QStringLiteral("HF_TOKEN"), s.value(QStringLiteral("hf_token")).toString());
        env.insert(QStringLiteral("HF_MODEL"), s.value(QStringLiteral("hf_model"), kHfModels.first()).toString());
        env.insert(QStringLiteral("OPENAI_API_KEY"), s.value(QStringLiteral("openai_key")).toString());
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
        log(QStringLiteral("backend start program=%1 cwd=%2").arg(m_backend->program(), root));
    }

    void stopBackend() {
        if (m_backend->state() == QProcess::NotRunning) return;
        m_backend->terminate();
        if (!m_backend->waitForFinished(700)) m_backend->kill();
    }

    void restartBackend() { stopBackend(); startBackend(); }

    void sendMessage() {
        const QString text = m_entry->toPlainText().trimmed();
        if (text.isEmpty() || m_waiting || m_typing) return;
        startBackend();
        if (!m_backend->waitForStarted(1200)) {
            failRequest(QStringLiteral("Python backend could not start: %1").arg(m_backend->errorString()));
            return;
        }
        addBubble(QStringLiteral("You"), text, false);
        m_entry->clear();
        m_waiting = true;
        m_send->setEnabled(false);
        setStatus(QStringLiteral("Thinking…"));
        QJsonObject object;
        object.insert(QStringLiteral("action"), QStringLiteral("reply"));
        object.insert(QStringLiteral("text"), text);
        m_backend->write(QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n');
        log(QStringLiteral("request sent chars=%1").arg(text.size()));
    }

    void readBackend() {
        while (m_backend->canReadLine()) {
            const QByteArray line = m_backend->readLine().trimmed();
            if (line.isEmpty()) continue;
            log(QStringLiteral("backend response=%1").arg(QString::fromUtf8(line)));
            const QJsonDocument doc = QJsonDocument::fromJson(line);
            if (!doc.isObject()) continue;
            const QJsonObject object = doc.object();
            if (m_memoryRequest) {
                m_memoryRequest = false;
                QStringList values;
                for (const auto& value : object.value(QStringLiteral("memories")).toArray()) values << value.toString();
                QMessageBox::information(this, QStringLiteral("Memory"), values.isEmpty() ? QStringLiteral("No saved memories.") : values.join(QStringLiteral("\n\n")));
                continue;
            }
            if (!object.value(QStringLiteral("ok")).toBool()) {
                failRequest(object.value(QStringLiteral("error")).toString());
                continue;
            }
            m_typingBubble = addBubble(QStringLiteral("Vaxx"), QStringLiteral("▌"), true);
            m_typedText = object.value(QStringLiteral("answer")).toString();
            m_typeIndex = 0;
            m_waiting = false;
            m_typing = true;
            m_typeTimer.start();
            setStatus(QStringLiteral("Typing…"));
        }
    }

    ChatBubble* addBubble(const QString& sender, const QString& text, bool assistant) {
        auto* row = new QWidget(m_conversation);
        auto* rowLayout = new QHBoxLayout(row);
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
        m_typingBubble = nullptr;
        m_typedText.clear();
        m_typeIndex = 0;
        m_typing = false;
        m_waiting = false;
        setStatus(QStringLiteral("Ready"));
        m_send->setEnabled(!m_entry->toPlainText().trimmed().isEmpty());
    }

    void failRequest(const QString& error) {
        m_typeTimer.stop();
        m_waiting = false;
        m_typing = false;
        m_typingBubble = nullptr;
        log(QStringLiteral("request failure: %1").arg(error));
        setStatus(QStringLiteral("Backend error"));
        m_send->setEnabled(!m_entry->toPlainText().trimmed().isEmpty());
        QMessageBox::warning(this, QStringLiteral("AI backend"), error.isEmpty() ? QStringLiteral("The Python backend stopped unexpectedly.") : error);
    }

    void newChat() {
        if (m_waiting || m_typing) return;
        while (m_conversationLayout->count() > 1) {
            auto* item = m_conversationLayout->takeAt(0);
            if (auto* widget = item->widget()) widget->deleteLater();
            delete item;
        }
        setStatus(QStringLiteral("New chat"));
    }

    void showMemory() {
        if (m_backend->state() == QProcess::NotRunning) startBackend();
        if (!m_backend->waitForStarted(1200)) return;
        m_memoryRequest = true;
        QJsonObject object;
        object.insert(QStringLiteral("action"), QStringLiteral("memory"));
        m_backend->write(QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n');
    }

    void showSettings() {
        SettingsDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted && dialog.saved()) {
            restartBackend();
            setStatus(QStringLiteral("Settings saved"));
            log(QStringLiteral("settings saved; backend restarted"));
        }
    }

    void showDebug() {
        m_logs << QStringLiteral("[%1] backend state=%2 program=%3")
            .arg(QDateTime::currentDateTime().toString(Qt::ISODate), QString::number(m_backend->state()), m_backend->program());
        DebugDialog(m_logs, this).exec();
    }

    void scrollToBottom() {
        QTimer::singleShot(0, this, [this]() {
            auto* bar = m_scroll->verticalScrollBar();
            bar->setValue(bar->maximum());
        });
    }

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
    QFont appFont(QStringLiteral("Sans Serif"));
    appFont.setStyleStrategy(QFont::PreferMatch);
    app.setFont(appFont);
    NativeWindow window;
    window.show();
    return app.exec();
}
