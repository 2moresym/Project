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
    if (provider == QStringLiteral("openai")) {
        return !s.value(QStringLiteral("openai_key")).toString().trimmed().isEmpty() || !qEnvironmentVariable("OPENAI_API_KEY").trimmed().isEmpty();
    }
    return !s.value(QStringLiteral("hf_token")).toString().trimmed().isEmpty() || !qEnvironmentVariable("HF_TOKEN").trimmed().isEmpty();
}

class SettingsDialog final : public QDialog {
public:
    explicit SettingsDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle(QStringLiteral("Settings"));
        resize(620, 520);
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(24, 24, 24, 24);
        auto* tabs = new QTabWidget(this);
        root->addWidget(tabs, 1);

        auto* general = new QWidget;
        auto* generalForm = new QFormLayout(general);
        m_aiName = new QLineEdit(this);
        m_appearance = new QComboBox(this); m_appearance->addItems({"Dark", "Light", "System"});
        m_accent = new QComboBox(this); m_accent->addItems({"Default", "Cyan", "Green", "Magenta"});
        m_performance = new QComboBox(this); m_performance->addItems({"Low GPU", "Balanced", "Smooth"});
        m_stream = new QCheckBox("Enable streaming responses", this);
        m_autoMemory = new QCheckBox("Automatically remember simple personal facts", this);
        m_autoSummary = new QCheckBox("Automatically summarize long conversations", this);
        generalForm->addRow("AI name", m_aiName);
        generalForm->addRow("Appearance", m_appearance);
        generalForm->addRow("Accent", m_accent);
        generalForm->addRow("UI performance", m_performance);
        generalForm->addRow(m_stream); generalForm->addRow(m_autoMemory); generalForm->addRow(m_autoSummary);
        tabs->addTab(general, "General");

        auto* provider = new QWidget;
        auto* pf = new QFormLayout(provider);
        m_provider = new QComboBox(this); m_provider->addItems({"Hugging Face", "OpenAI-compatible"}); pf->addRow("Provider", m_provider);
        m_providerPages = new QStackedWidget(this); pf->addRow(m_providerPages);

        auto* hf = new QWidget; auto* hfForm = new QFormLayout(hf);
        m_hfToken = new QLineEdit(this); m_hfToken->setEchoMode(QLineEdit::Password);
        m_hfModel = new QComboBox(this);
        hfForm->addRow("HF token", m_hfToken); hfForm->addRow("Model", m_hfModel); m_providerPages->addWidget(hf);

        auto* oa = new QWidget; auto* oaForm = new QFormLayout(oa);
        m_openAiKey = new QLineEdit(this); m_openAiKey->setEchoMode(QLineEdit::Password);
        m_openAiEndpoint = new QLineEdit(this);
        m_openAiModel = new QComboBox(this);
        oaForm->addRow("API key", m_openAiKey); oaForm->addRow("Endpoint", m_openAiEndpoint); oaForm->addRow("Model", m_openAiModel); m_providerPages->addWidget(oa);

        auto* note = new QLabel("Credentials are kept in local desktop settings.", this); note->setObjectName("settingsNote"); note->setWordWrap(true); pf->addRow(note);
        tabs->addTab(provider, "AI provider");

        auto* buttons = new QHBoxLayout; buttons->addStretch(); auto* cancel = new QPushButton("Cancel"); auto* save = new QPushButton("Save"); save->setObjectName("primaryButton"); buttons->addWidget(cancel); buttons->addWidget(save); root->addLayout(buttons);
        load();
        connect(m_provider, &QComboBox::currentTextChanged, this, [this](const QString& text) { m_providerPages->setCurrentIndex(text == "OpenAI-compatible" ? 1 : 0); });
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
        connect(save, &QPushButton::clicked, this, &SettingsDialog::save);
    }

private:
    static void fillModels(QComboBox* box, const QStringList& models, const QString& selected) {
        box->clear(); box->addItems(models);
        if (models.contains(selected)) box->setCurrentText(selected); else { box->addItem(selected); box->setCurrentText(selected); }
    }

    void load() {
        QSettings s;
        const QString provider = s.value("provider", "huggingface").toString();
        m_provider->setCurrentIndex(provider == "openai" ? 1 : 0);
        m_aiName->setText(s.value("ai_name", "Vaxx").toString());
        m_appearance->setCurrentText(s.value("appearance", "Dark").toString());
        m_accent->setCurrentText(s.value("accent", "Default").toString());
        m_performance->setCurrentText(s.value("ui_performance", "Balanced").toString());
        m_stream->setChecked(s.value("stream", true).toBool()); m_autoMemory->setChecked(s.value("auto_memory", true).toBool()); m_autoSummary->setChecked(s.value("auto_summary", true).toBool());
        QString hf = s.value("hf_token").toString(); if (hf.isEmpty()) hf = qEnvironmentVariable("HF_TOKEN"); m_hfToken->setText(hf);
        QString key = s.value("openai_key").toString(); if (key.isEmpty()) key = qEnvironmentVariable("OPENAI_API_KEY"); m_openAiKey->setText(key);
        m_openAiEndpoint->setText(s.value("openai_base_url", "https://api.openai.com/v1/chat/completions").toString());
        fillModels(m_hfModel, kHfModels, s.value("hf_model", kHfModels.first()).toString());
        fillModels(m_openAiModel, kOpenAiModels, s.value("openai_model", kOpenAiModels.first()).toString());
        m_providerPages->setCurrentIndex(provider == "openai" ? 1 : 0);
    }

    void save() {
        QSettings s;
        const bool openai = m_provider->currentIndex() == 1;
        s.setValue("provider", openai ? "openai" : "huggingface");
        s.setValue("ai_name", m_aiName->text().trimmed().left(40));
        s.setValue("appearance", m_appearance->currentText()); s.setValue("accent", m_accent->currentText()); s.setValue("ui_performance", m_performance->currentText());
        s.setValue("stream", m_stream->isChecked()); s.setValue("auto_memory", m_autoMemory->isChecked()); s.setValue("auto_summary", m_autoSummary->isChecked());
        s.setValue("hf_token", m_hfToken->text().trimmed()); s.setValue("hf_model", m_hfModel->currentText());
        s.setValue("openai_key", m_openAiKey->text().trimmed()); s.setValue("openai_base_url", m_openAiEndpoint->text().trimmed()); s.setValue("openai_model", m_openAiModel->currentText());
        s.sync(); accept();
    }

    QComboBox* m_provider = nullptr; QStackedWidget* m_providerPages = nullptr; QLineEdit* m_aiName = nullptr; QComboBox* m_appearance = nullptr; QComboBox* m_accent = nullptr; QComboBox* m_performance = nullptr; QCheckBox* m_stream = nullptr; QCheckBox* m_autoMemory = nullptr; QCheckBox* m_autoSummary = nullptr; QLineEdit* m_hfToken = nullptr; QComboBox* m_hfModel = nullptr; QLineEdit* m_openAiKey = nullptr; QLineEdit* m_openAiEndpoint = nullptr; QComboBox* m_openAiModel = nullptr;
};

class DebugDialog final : public QDialog {
public:
    explicit DebugDialog(const QStringList& logs, QWidget* parent = nullptr) : QDialog(parent) { setWindowTitle("Debug mode"); resize(860, 540); auto* l = new QVBoxLayout(this); auto* o = new QPlainTextEdit(this); o->setReadOnly(true); o->setPlainText(logs.join("\n")); l->addWidget(o,1); auto* c = new QPushButton("Close"); l->addWidget(c,0,Qt::AlignRight); connect(c,&QPushButton::clicked,this,&QDialog::accept); }
};

class NativeWindow final : public QMainWindow {
public:
    NativeWindow() : m_backend(new QProcess(this)) {
        setWindowTitle("AI Chat — Vaxx"); resize(1180,780); setMinimumSize(900,620); buildUi(); startBackend();
        QSettings s; QString provider = s.value("provider", "").toString();
        if (provider.isEmpty()) provider = !qEnvironmentVariable("OPENAI_API_KEY").trimmed().isEmpty() ? "openai" : "huggingface";
        if (!hasCredential(provider)) QTimer::singleShot(250, this, &NativeWindow::showSettings);
    }
    ~NativeWindow() override { m_typeTimer.stop(); stopBackend(); }

private:
    void buildUi() {
        auto* root = new QWidget(this); auto* rl = new QHBoxLayout(root); rl->setContentsMargins(12,12,12,12); rl->setSpacing(12);
        auto* sidebar = new QFrame(this); sidebar->setObjectName("sidebar"); sidebar->setFixedWidth(252); auto* side = new QVBoxLayout(sidebar);
        side->setContentsMargins(14,14,14,14); side->setSpacing(9); auto* brand = new QLabel("Vaxx",sidebar); brand->setObjectName("brand"); side->addWidget(brand); auto* subtitle=new QLabel("AI playground",sidebar); subtitle->setObjectName("subtitle"); side->addWidget(subtitle);
        auto* newChat=new QPushButton("＋  New chat",sidebar); newChat->setObjectName("primaryButton"); side->addWidget(newChat); auto* cl=new QLabel("Chats",sidebar); cl->setObjectName("sectionLabel"); side->addWidget(cl); m_chatList=new QListWidget(sidebar); m_chatList->addItem("Main chat"); m_chatList->setCurrentRow(0); side->addWidget(m_chatList,1);
        auto* memory=new QPushButton("Memory",sidebar); auto* settings=new QPushButton("Settings",sidebar); auto* debug=new QPushButton("Debug",sidebar); side->addWidget(memory); side->addWidget(settings); side->addWidget(debug); rl->addWidget(sidebar);
        auto* main=new QWidget(this); auto* ml=new QVBoxLayout(main); ml->setContentsMargins(6,2,6,2); ml->setSpacing(12); auto* top=new QHBoxLayout; auto* title=new QLabel("Main chat",main); title->setObjectName("header"); top->addWidget(title); top->addStretch(); m_status=new QLabel("Ready",main); m_status->setObjectName("status"); top->addWidget(m_status); ml->addLayout(top);
        m_scroll=new QScrollArea(main); m_scroll->setWidgetResizable(true); m_scroll->setFrameShape(QFrame::NoFrame); m_conversation=new QWidget; m_conversationLayout=new QVBoxLayout(m_conversation); m_conversationLayout->setContentsMargins(10,10,10,10); m_conversationLayout->setSpacing(12); m_conversationLayout->addStretch(); m_scroll->setWidget(m_conversation); ml->addWidget(m_scroll,1);
        auto* composer=new QFrame(main); composer->setObjectName("composer"); auto* comp=new QHBoxLayout(composer); comp->setContentsMargins(10,8,10,8); m_entry=new QTextEdit(composer); m_entry->setPlaceholderText("Message Vaxx…"); m_entry->setFixedHeight(76); comp->addWidget(m_entry,1); m_send=new QPushButton("Send",composer); m_send->setObjectName("primaryButton"); m_send->setFixedWidth(100); m_send->setEnabled(false); comp->addWidget(m_send); ml->addWidget(composer); rl->addWidget(main,1); setCentralWidget(root);
        connect(m_send,&QPushButton::clicked,this,&NativeWindow::sendMessage); connect(m_entry,&QTextEdit::textChanged,this,[this](){if(!m_waiting&&!m_typing)m_send->setEnabled(!m_entry->toPlainText().trimmed().isEmpty());}); connect(newChat,&QPushButton::clicked,this,&NativeWindow::newChat); connect(memory,&QPushButton::clicked,this,&NativeWindow::showMemory); connect(settings,&QPushButton::clicked,this,&NativeWindow::showSettings); connect(debug,&QPushButton::clicked,this,&NativeWindow::showDebug);
        connect(m_backend,&QProcess::readyReadStandardOutput,this,&NativeWindow::readBackend); connect(m_backend,&QProcess::readyReadStandardError,this,[this](){const auto t=m_backend->readAllStandardError().trimmed(); if(!t.isEmpty()) log(QString("backend stderr: %1").arg(QString::fromUtf8(t)));}); connect(m_backend,&QProcess::errorOccurred,this,[this](QProcess::ProcessError){log(QString("backend error: %1").arg(m_backend->errorString()));}); connect(m_backend,QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),this,[this](int code,QProcess::ExitStatus status){log(QString("backend finished code=%1 status=%2").arg(code).arg(status==QProcess::NormalExit?"normal":"crashed"));});
        m_typeTimer.setInterval(14); connect(&m_typeTimer,&QTimer::timeout,this,&NativeWindow::typeNextCharacter); applyStyle();
    }

    void applyStyle(){setStyleSheet("QMainWindow,QWidget{background:#0e1117;color:#eef2f7;font-family:'Noto Sans';font-size:11pt;} QLabel{background:transparent;} QMenu{background:#171d27;color:#eef2f7;border:1px solid #2a3342;} QMenu::item{padding:7px 18px;} QMenu::item:selected{background:#2b3b5e;color:#fff;} #sidebar{background:#151922;border:1px solid #242b38;border-radius:16px;} #brand{font-size:23pt;font-weight:750;} #subtitle,#settingsNote{color:#8791a1;font-size:9.5pt;} #sectionLabel{color:#8f9aaa;font-weight:650;padding-top:8px;} #header{font-size:17pt;font-weight:700;padding:5px;} #status{color:#76d69a;font-size:9.5pt;padding:6px;} QPushButton,QComboBox{background:#191e28;color:#eef2f7;border:1px solid #2a3240;border-radius:10px;padding:9px 12px;} QPushButton:hover{border-color:#5b8cff;background:#1d2430;} #primaryButton{background:#5b8cff;color:#fff;border:0;font-weight:700;} QListWidget{background:#11151d;color:#eef2f7;border:1px solid #242b38;border-radius:10px;} #composer{background:#151922;border:1px solid #242b38;border-radius:15px;} QTextEdit,QPlainTextEdit,QLineEdit{background:#10141b;color:#eef2f7;border:1px solid #2a3240;border-radius:11px;padding:10px;} #userBubble{background:#202b40;border-radius:14px;} #assistantBubble{background:#171d27;border:1px solid #2a3342;border-radius:14px;} #bubbleSender{background:transparent;color:#8fb3ff;font-size:9pt;font-weight:700;} ");}

    QProcessEnvironment backendEnvironment() const { QSettings s; auto env=QProcessEnvironment::systemEnvironment(); QString provider=s.value("provider", "").toString(); if(provider.isEmpty()) provider=!qEnvironmentVariable("OPENAI_API_KEY").trimmed().isEmpty()?"openai":"huggingface"; env.insert("PROJECT_PROVIDER",provider); env.insert("HF_TOKEN",s.value("hf_token",qEnvironmentVariable("HF_TOKEN")).toString()); env.insert("OPENAI_API_KEY",s.value("openai_key",qEnvironmentVariable("OPENAI_API_KEY")).toString()); env.insert("OPENAI_BASE_URL",s.value("openai_base_url","https://api.openai.com/v1/chat/completions").toString()); env.insert("OPENAI_MODEL",s.value("openai_model","gpt-4o-mini").toString()); env.insert("HF_MODEL",s.value("hf_model",kHfModels.first()).toString()); return env; }
    void startBackend(){if(m_backend->state()!=QProcess::NotRunning)return; m_backend->setProcessEnvironment(backendEnvironment()); QString root=QCoreApplication::applicationDirPath()+"/.."; QString python=root+"/.venv/bin/python"; if(!QFileInfo::exists(python)) python="python3"; m_backend->setProgram(python); m_backend->setArguments({"-m","src.backend_bridge"}); m_backend->setWorkingDirectory(root); log(QString("backend start program=%1").arg(python)); m_backend->start();}
    void stopBackend(){if(m_backend->state()!=QProcess::NotRunning){m_backend->terminate();if(!m_backend->waitForFinished(500))m_backend->kill();}}
    void restartBackend(){stopBackend();startBackend();}
    void sendMessage(){const auto text=m_entry->toPlainText().trimmed();if(text.isEmpty()||m_waiting||m_typing)return;startBackend();if(!m_backend->waitForStarted(1000)){log("backend failed to start");return;}addBubble("You",text,false);m_entry->clear();m_waiting=true;m_send->setEnabled(false);setStatus("Thinking…");QJsonObject o;o.insert("action","reply");o.insert("text",text);m_backend->write(QJsonDocument(o).toJson(QJsonDocument::Compact)+'\n');}
    void readBackend(){while(m_backend->canReadLine()){const auto line=m_backend->readLine().trimmed();const auto doc=QJsonDocument::fromJson(line);if(!doc.isObject()){log(QString("invalid backend line: %1").arg(QString::fromUtf8(line)));continue;}const auto object=doc.object();if(m_memoryRequest){m_memoryRequest=false;QStringList values;for(const auto& v:object.value("memories").toArray())values<<v.toString();QMessageBox::information(this,"Memory",values.isEmpty()?"No saved memories.":values.join("\n\n"));continue;}if(!object.value("ok").toBool()){addBubble("Vaxx",QString("Something went wrong: %1").arg(object.value("error").toString()),true);finishTyping();continue;}m_typingBubble=addBubble("Vaxx","▌",true);m_typedText=object.value("answer").toString();m_typeIndex=0;m_waiting=false;m_typing=true;m_typeTimer.start();setStatus("Typing…");}}
    ChatBubble* addBubble(const QString& sender,const QString& text,bool assistant){auto* row=new QWidget(m_conversation);auto* l=new QHBoxLayout(row);l->setContentsMargins(0,0,0,0);auto* b=new ChatBubble(sender,text,assistant,row);if(assistant){l->addStretch();l->addWidget(b,0,Qt::AlignRight);}else{l->addWidget(b,0,Qt::AlignLeft);l->addStretch();}m_conversationLayout->insertWidget(m_conversationLayout->count()-1,row);scrollToBottom();return b;}
    void typeNextCharacter(){if(!m_typingBubble)return;if(m_typeIndex>=m_typedText.size()){finishTyping();return;}++m_typeIndex;m_typingBubble->setText(m_typedText.left(m_typeIndex)+"▌");scrollToBottom();}
    void finishTyping(){m_typeTimer.stop();if(m_typingBubble)m_typingBubble->setText(m_typedText);m_typingBubble=nullptr;m_typedText.clear();m_typeIndex=0;m_typing=false;m_waiting=false;setStatus("Ready");m_send->setEnabled(!m_entry->toPlainText().trimmed().isEmpty());}
    void newChat(){if(m_waiting||m_typing)return;while(m_conversationLayout->count()>1){auto* item=m_conversationLayout->takeAt(0);if(auto* w=item->widget())w->deleteLater();delete item;}setStatus("New chat");}
    void showSettings(){SettingsDialog d(this);if(d.exec()==QDialog::Accepted){restartBackend();setStatus("Settings saved");log("settings saved");}}
    void showMemory(){if(m_backend->state()==QProcess::NotRunning)startBackend();if(!m_backend->waitForStarted(1000)){log("memory: backend failed to start");return;}m_memoryRequest=true;QJsonObject o;o.insert("action","memory");m_backend->write(QJsonDocument(o).toJson(QJsonDocument::Compact)+'\n');}
    void showDebug(){m_logs<<QString("[%1] backend state=%2").arg(QDateTime::currentDateTime().toString(Qt::ISODate),QString::number(m_backend->state()));DebugDialog(m_logs,this).exec();}
    void scrollToBottom(){QTimer::singleShot(0,this,[this](){auto* bar=m_scroll->verticalScrollBar();bar->setValue(bar->maximum());});}
    void setStatus(const QString& text){m_status->setText(text);} void log(const QString& msg){m_logs<<QString("[%1] %2").arg(QDateTime::currentDateTime().toString(Qt::ISODate),msg);}
    QProcess* m_backend=nullptr;QListWidget* m_chatList=nullptr;QLabel* m_status=nullptr;QScrollArea* m_scroll=nullptr;QWidget* m_conversation=nullptr;QVBoxLayout* m_conversationLayout=nullptr;QTextEdit* m_entry=nullptr;QPushButton* m_send=nullptr;QTimer m_typeTimer;ChatBubble* m_typingBubble=nullptr;QString m_typedText;int m_typeIndex=0;bool m_waiting=false;bool m_typing=false;bool m_memoryRequest=false;QStringList m_logs;
};

} // namespace

int main(int argc,char** argv){QApplication app(argc,argv);app.setOrganizationName("Vaxx");app.setApplicationName("AI Chat");NativeWindow window;window.show();return app.exec();}
