#include "ChatBubble.hpp"

#include <QApplication>
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
    QStringLiteral("gpt-4o-mini"), QStringLiteral("gpt-4.1-mini"),
    QStringLiteral("gpt-4.1"), QStringLiteral("gpt-5-mini")
};

QString provider() {
    QSettings s;
    const auto saved = s.value(QStringLiteral("provider")).toString();
    if (saved == QStringLiteral("openai") || saved == QStringLiteral("huggingface")) return saved;
    return qEnvironmentVariable("OPENAI_API_KEY").trimmed().isEmpty() ? QStringLiteral("huggingface") : QStringLiteral("openai");
}

QFont appFont() {
    const QStringList installed = QFontDatabase::families();
    QStringList families;
    for (const auto& name : {QStringLiteral("Noto Sans"), QStringLiteral("Noto Color Emoji"), QStringLiteral("Segoe UI Emoji"), QStringLiteral("Apple Color Emoji"), QStringLiteral("Sans Serif")}) {
        if (installed.contains(name)) families << name;
    }
    QFont f(families.isEmpty() ? QStringLiteral("Sans Serif") : families.first());
    if (!families.isEmpty()) f.setFamilies(families);
    f.setStyleStrategy(QFont::PreferMatch);
    return f;
}

class SettingsDialog final : public QDialog {
public:
    explicit SettingsDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle(QStringLiteral("Settings"));
        resize(620, 520);
        auto* root = new QVBoxLayout(this);
        auto* tabs = new QTabWidget(this);
        root->addWidget(tabs, 1);

        auto* general = new QWidget;
        auto* gf = new QFormLayout(general);
        m_name = new QLineEdit(this);
        m_appearance = new QComboBox(this); m_appearance->addItems({QStringLiteral("Dark"), QStringLiteral("Light"), QStringLiteral("System")});
        m_performance = new QComboBox(this); m_performance->addItems({QStringLiteral("Low GPU"), QStringLiteral("Balanced"), QStringLiteral("Smooth")});
        gf->addRow(QStringLiteral("AI name"), m_name);
        gf->addRow(QStringLiteral("Appearance"), m_appearance);
        gf->addRow(QStringLiteral("UI performance"), m_performance);
        tabs->addTab(general, QStringLiteral("General"));

        auto* api = new QWidget;
        auto* af = new QFormLayout(api);
        m_provider = new QComboBox(this); m_provider->addItems({QStringLiteral("Hugging Face"), QStringLiteral("OpenAI-compatible")});
        af->addRow(QStringLiteral("Provider"), m_provider);
        m_pages = new QStackedWidget(this); af->addRow(m_pages);
        auto* hf = new QWidget; auto* hff = new QFormLayout(hf);
        m_hfKey = new QLineEdit(this); m_hfKey->setEchoMode(QLineEdit::Password);
        m_hfModel = new QComboBox(this); m_hfModel->addItems(kHfModels);
        hff->addRow(QStringLiteral("HF token"), m_hfKey); hff->addRow(QStringLiteral("Model"), m_hfModel); m_pages->addWidget(hf);
        auto* oa = new QWidget; auto* oaf = new QFormLayout(oa);
        m_openKey = new QLineEdit(this); m_openKey->setEchoMode(QLineEdit::Password);
        m_endpoint = new QLineEdit(this);
        m_openModel = new QComboBox(this); m_openModel->addItems(kOpenAiModels);
        oaf->addRow(QStringLiteral("API key"), m_openKey); oaf->addRow(QStringLiteral("Endpoint"), m_endpoint); oaf->addRow(QStringLiteral("Model"), m_openModel); m_pages->addWidget(oa);
        tabs->addTab(api, QStringLiteral("AI provider"));

        auto* buttons = new QHBoxLayout; buttons->addStretch(); auto* cancel = new QPushButton(QStringLiteral("Cancel")); auto* save = new QPushButton(QStringLiteral("Save")); save->setObjectName(QStringLiteral("primaryButton")); buttons->addWidget(cancel); buttons->addWidget(save); root->addLayout(buttons);
        load();
        connect(m_provider, &QComboBox::currentTextChanged, this, [this](const QString& text){ m_pages->setCurrentIndex(text == QStringLiteral("OpenAI-compatible") ? 1 : 0); });
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
        connect(save, &QPushButton::clicked, this, &QDialog::accept);
    }

    void saveSettings() {
        QSettings s;
        s.setValue(QStringLiteral("provider"), m_provider->currentIndex() == 1 ? QStringLiteral("openai") : QStringLiteral("huggingface"));
        s.setValue(QStringLiteral("ai_name"), m_name->text().trimmed().left(40));
        s.setValue(QStringLiteral("appearance"), m_appearance->currentText());
        s.setValue(QStringLiteral("ui_performance"), m_performance->currentText());
        s.setValue(QStringLiteral("hf_token"), m_hfKey->text().trimmed());
        s.setValue(QStringLiteral("hf_model"), m_hfModel->currentText());
        s.setValue(QStringLiteral("openai_key"), m_openKey->text().trimmed());
        s.setValue(QStringLiteral("openai_base_url"), m_endpoint->text().trimmed());
        s.setValue(QStringLiteral("openai_model"), m_openModel->currentText());
        s.sync();
    }

private:
    void load() {
        QSettings s;
        const auto p = provider();
        m_provider->setCurrentIndex(p == QStringLiteral("openai") ? 1 : 0);
        m_pages->setCurrentIndex(p == QStringLiteral("openai") ? 1 : 0);
        m_name->setText(s.value(QStringLiteral("ai_name"), QStringLiteral("Vaxx")).toString());
        m_appearance->setCurrentText(s.value(QStringLiteral("appearance"), QStringLiteral("Dark")).toString());
        m_performance->setCurrentText(s.value(QStringLiteral("ui_performance"), QStringLiteral("Balanced")).toString());
        auto hf = s.value(QStringLiteral("hf_token")).toString(); if (hf.isEmpty()) hf = qEnvironmentVariable("HF_TOKEN"); m_hfKey->setText(hf);
        auto ok = s.value(QStringLiteral("openai_key")).toString(); if (ok.isEmpty()) ok = qEnvironmentVariable("OPENAI_API_KEY"); m_openKey->setText(ok);
        m_endpoint->setText(s.value(QStringLiteral("openai_base_url"), QStringLiteral("https://api.openai.com/v1/chat/completions")).toString());
        m_hfModel->setCurrentText(s.value(QStringLiteral("hf_model"), kHfModels.first()).toString());
        m_openModel->setCurrentText(s.value(QStringLiteral("openai_model"), kOpenAiModels.first()).toString());
    }
    QLineEdit* m_name = nullptr; QComboBox* m_appearance = nullptr; QComboBox* m_performance = nullptr;
    QComboBox* m_provider = nullptr; QStackedWidget* m_pages = nullptr; QLineEdit* m_hfKey = nullptr; QComboBox* m_hfModel = nullptr;
    QLineEdit* m_openKey = nullptr; QLineEdit* m_endpoint = nullptr; QComboBox* m_openModel = nullptr;
};

class NativeWindow final : public QMainWindow {
public:
    NativeWindow() : m_backend(new QProcess(this)) {
        setWindowTitle(QStringLiteral("AI Chat — Vaxx")); resize(1180, 780); setMinimumSize(900, 620);
        buildUi(); connectBackend(); startBackend();
        QTimer::singleShot(150, this, &NativeWindow::requestChats);
    }
    ~NativeWindow() override { m_typeTimer.stop(); stopBackend(); }

private:
    void buildUi() {
        auto* root = new QWidget(this); auto* layout = new QHBoxLayout(root); layout->setContentsMargins(12,12,12,12); layout->setSpacing(12);
        auto* sidebar = new QFrame(this); sidebar->setObjectName(QStringLiteral("sidebar")); sidebar->setFixedWidth(252); auto* side = new QVBoxLayout(sidebar); side->setContentsMargins(14,14,14,14); side->setSpacing(9);
        auto* brand = new QLabel(QStringLiteral("Vaxx"), sidebar); brand->setObjectName(QStringLiteral("brand")); side->addWidget(brand); auto* sub = new QLabel(QStringLiteral("AI playground"), sidebar); sub->setObjectName(QStringLiteral("subtitle")); side->addWidget(sub);
        auto* newChat = new QPushButton(QStringLiteral("New chat"), sidebar); newChat->setObjectName(QStringLiteral("primaryButton")); side->addWidget(newChat);
        side->addWidget(new QLabel(QStringLiteral("Chats"), sidebar)); m_chatList = new QListWidget(sidebar); side->addWidget(m_chatList,1);
        auto* memory = new QPushButton(QStringLiteral("Memory"), sidebar); auto* settings = new QPushButton(QStringLiteral("Settings"), sidebar); auto* debug = new QPushButton(QStringLiteral("Debug"), sidebar); side->addWidget(memory); side->addWidget(settings); side->addWidget(debug); layout->addWidget(sidebar);
        auto* main = new QWidget; auto* ml = new QVBoxLayout(main); ml->setContentsMargins(6,2,6,2); ml->setSpacing(12);
        auto* top = new QHBoxLayout; m_title = new QLabel(QStringLiteral("Chat")); m_title->setObjectName(QStringLiteral("header")); top->addWidget(m_title); top->addStretch(); m_status = new QLabel(QStringLiteral("Ready")); m_status->setObjectName(QStringLiteral("status")); top->addWidget(m_status); ml->addLayout(top);
        m_scroll = new QScrollArea; m_scroll->setWidgetResizable(true); m_scroll->setFrameShape(QFrame::NoFrame); m_conversation = new QWidget; m_conversationLayout = new QVBoxLayout(m_conversation); m_conversationLayout->setContentsMargins(10,10,10,10); m_conversationLayout->setSpacing(12); m_conversationLayout->addStretch(); m_scroll->setWidget(m_conversation); ml->addWidget(m_scroll,1);
        auto* composer = new QFrame; composer->setObjectName(QStringLiteral("composer")); auto* cl = new QHBoxLayout(composer); cl->setContentsMargins(10,8,10,8); m_entry = new QTextEdit; m_entry->setFont(appFont()); m_entry->setPlaceholderText(QStringLiteral("Message Vaxx…")); m_entry->setFixedHeight(76); cl->addWidget(m_entry,1); m_send = new QPushButton(QStringLiteral("Send")); m_send->setObjectName(QStringLiteral("primaryButton")); m_send->setFixedWidth(100); m_send->setEnabled(false); cl->addWidget(m_send); ml->addWidget(composer); layout->addWidget(main,1); setCentralWidget(root);
        connect(newChat,&QPushButton::clicked,this,&NativeWindow::newChat); connect(memory,&QPushButton::clicked,this,&NativeWindow::showMemory); connect(settings,&QPushButton::clicked,this,&NativeWindow::showSettings); connect(debug,&QPushButton::clicked,this,&NativeWindow::showDebug); connect(m_send,&QPushButton::clicked,this,&NativeWindow::sendMessage); connect(m_entry,&QTextEdit::textChanged,this,[this](){if(!m_waiting&&!m_typing)m_send->setEnabled(!m_entry->toPlainText().trimmed().isEmpty());}); connect(m_chatList,&QListWidget::currentRowChanged,this,&NativeWindow::selectChat);
        applyStyle();
    }

    void connectBackend() {
        connect(m_backend,&QProcess::readyReadStandardOutput,this,&NativeWindow::readBackend); connect(m_backend,&QProcess::readyReadStandardError,this,[this](){const auto text=m_backend->readAllStandardError().trimmed(); if(!text.isEmpty())log(QString::fromUtf8(text));});
        connect(m_backend,QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),this,[this](int code,QProcess::ExitStatus status){log(QStringLiteral("backend finished code=%1 status=%2").arg(code).arg(status==QProcess::NormalExit?QStringLiteral("normal"):QStringLiteral("crashed"))); if(status==QProcess::CrashExit){m_status->setText(QStringLiteral("Backend crashed"));}});
    }

    void applyStyle() {
        setStyleSheet(QStringLiteral("QMainWindow,QWidget{background:#0e1117;color:#eef2f7;font-size:11pt;} QLabel{background:transparent;} #sidebar{background:#151922;border:1px solid #242b38;border-radius:16px;} #brand{font-size:23pt;font-weight:750;} #subtitle{color:#8791a1;font-size:9.5pt;} #header{font-size:17pt;font-weight:700;} #status{color:#76d69a;} QPushButton,QComboBox{background:#191e28;color:#eef2f7;border:1px solid #2a3240;border-radius:10px;padding:9px 12px;} QPushButton:hover{border-color:#5b8cff;background:#1d2430;} QPushButton#primaryButton{background:#5b8cff;color:white;border:0;font-weight:700;} QListWidget{background:#11151d;color:#eef2f7;border:1px solid #242b38;border-radius:10px;padding:5px;} QListWidget::item{padding:10px;border-radius:8px;} QListWidget::item:selected{background:#283757;color:white;} #composer{background:#151922;border:1px solid #242b38;border-radius:15px;} QTextEdit,QPlainTextEdit,QLineEdit{background:#10141b;color:#eef2f7;border:1px solid #2a3240;border-radius:11px;padding:10px;} QMenu{background:#171d27;color:#eef2f7;border:1px solid #2a3342;} QMenu::item:selected{background:#2b3b5e;color:white;}"));
        qApp->setFont(appFont());
    }

    QString pythonProgram() const { const auto root=QCoreApplication::applicationDirPath()+QStringLiteral("/.."); const auto venv=root+QStringLiteral("/.venv/bin/python"); return QFileInfo::exists(venv)?venv:QStringLiteral("python3"); }
    QProcessEnvironment backendEnv() const { QSettings s; auto e=QProcessEnvironment::systemEnvironment(); e.insert(QStringLiteral("PROJECT_PROVIDER"),provider()); e.insert(QStringLiteral("HF_TOKEN"),s.value(QStringLiteral("hf_token")).toString()); e.insert(QStringLiteral("HF_MODEL"),s.value(QStringLiteral("hf_model"),kHfModels.first()).toString()); e.insert(QStringLiteral("OPENAI_API_KEY"),s.value(QStringLiteral("openai_key")).toString()); e.insert(QStringLiteral("OPENAI_BASE_URL"),s.value(QStringLiteral("openai_base_url"),QStringLiteral("https://api.openai.com/v1/chat/completions")).toString()); e.insert(QStringLiteral("OPENAI_MODEL"),s.value(QStringLiteral("openai_model"),kOpenAiModels.first()).toString()); e.insert(QStringLiteral("PYTHONUNBUFFERED"),QStringLiteral("1")); return e; }
    void startBackend(){if(m_backend->state()!=QProcess::NotRunning)return; const auto root=QCoreApplication::applicationDirPath()+QStringLiteral("/.."); m_backend->setProcessEnvironment(backendEnv()); m_backend->setProgram(pythonProgram()); m_backend->setArguments({QStringLiteral("-m"),QStringLiteral("src.backend_bridge")}); m_backend->setWorkingDirectory(root); m_backend->start(); log(QStringLiteral("backend start %1").arg(m_backend->program()));}
    void stopBackend(){if(m_backend->state()==QProcess::NotRunning)return; m_backend->terminate(); if(!m_backend->waitForFinished(700))m_backend->kill();}
    void send(const QJsonObject& obj){if(m_backend->state()!=QProcess::Running)startBackend(); m_backend->write(QJsonDocument(obj).toJson(QJsonDocument::Compact)+"\n");}

    void requestChats(){send({{QStringLiteral("action"),QStringLiteral("list_chats")}});}
    void selectChat(int row){if(row<0||row>=m_chatNames.size())return; m_waiting=false; m_typing=false; m_typeTimer.stop(); send({{QStringLiteral("action"),QStringLiteral("select_chat")},{QStringLiteral("name"),m_chatNames.at(row)}});}
    void newChat(){bool ok=false; const auto name=QInputDialog::getText(this,QStringLiteral("New chat"),QStringLiteral("Chat name:"),QLineEdit::Normal,QString(),&ok).trimmed(); if(!ok||name.isEmpty())return; send({{QStringLiteral("action"),QStringLiteral("new_chat")},{QStringLiteral("name"),name}});}

    void sendMessage(){const auto text=m_entry->toPlainText().trimmed(); if(text.isEmpty()||m_waiting||m_typing)return; addBubble(QStringLiteral("You"),text,false); m_entry->clear(); m_waiting=true; m_send->setEnabled(false); m_status->setText(QStringLiteral("Thinking…")); send({{QStringLiteral("action"),QStringLiteral("reply")},{QStringLiteral("text"),text}});}

    void readBackend(){while(m_backend->canReadLine()){const auto line=m_backend->readLine().trimmed(); if(line.isEmpty())continue; log(QString::fromUtf8(line)); const auto doc=QJsonDocument::fromJson(line); if(!doc.isObject())continue; const auto o=doc.object(); const auto action=o.value(QStringLiteral("action")).toString();
        if(action==QStringLiteral("chat_list")){m_chatNames.clear(); m_chatList->clear(); for(const auto& v:o.value(QStringLiteral("chats")).toArray()){const auto c=v.toObject(); m_chatNames<<c.value(QStringLiteral("name")).toString(); const auto label=c.value(QStringLiteral("title")).toString(); m_chatList->addItem(label.isEmpty()?c.value(QStringLiteral("name")).toString():label);} const auto current=o.value(QStringLiteral("current")).toString(); const int idx=m_chatNames.indexOf(current); if(idx>=0){m_chatList->blockSignals(true);m_chatList->setCurrentRow(idx);m_chatList->blockSignals(false);loadMessages(o.value(QStringLiteral("messages")).toArray(),current);} continue;}
        if(action==QStringLiteral("history")){loadMessages(o.value(QStringLiteral("messages")).toArray(),o.value(QStringLiteral("name")).toString()); continue;}
        if(action==QStringLiteral("created")){requestChats();continue;}
        if(action==QStringLiteral("memory")){QStringList vals;for(const auto& v:o.value(QStringLiteral("memories")).toArray())vals<<v.toString();QMessageBox::information(this,QStringLiteral("Memory"),vals.isEmpty()?QStringLiteral("No saved memories."):vals.join("\n\n"));continue;}
        if(o.value(QStringLiteral("ok")).toBool()&&o.contains(QStringLiteral("answer"))){m_typingBubble=addBubble(QStringLiteral("Vaxx"),QStringLiteral("▌"),true);m_typedText=o.value(QStringLiteral("answer")).toString();m_typeIndex=0;m_waiting=false;m_typing=true;m_typeTimer.start();m_status->setText(QStringLiteral("Typing…"));continue;}
        if(!o.value(QStringLiteral("ok")).toBool()&&!o.value(QStringLiteral("error")).toString().isEmpty()){m_waiting=false;m_send->setEnabled(true);m_status->setText(QStringLiteral("AI error"));QMessageBox::warning(this,QStringLiteral("AI backend"),o.value(QStringLiteral("error")).toString());}
    }}

    void loadMessages(const QJsonArray& messages,const QString& name){clearConversation(); if(!name.isEmpty())m_title->setText(name); for(const auto& v:messages){const auto m=v.toObject(); const auto role=m.value(QStringLiteral("role")).toString(); if(role==QStringLiteral("user"))addBubble(QStringLiteral("You"),m.value(QStringLiteral("content")).toString(),false); else if(role==QStringLiteral("assistant"))addBubble(QStringLiteral("Vaxx"),m.value(QStringLiteral("content")).toString(),true);} m_status->setText(QStringLiteral("Ready"));}
    void clearConversation(){while(m_conversationLayout->count()>1){auto* item=m_conversationLayout->takeAt(0);if(auto* w=item->widget())w->deleteLater();delete item;}m_typingBubble=nullptr;m_typedText.clear();m_typeIndex=0;}
    ChatBubble* addBubble(const QString& sender,const QString& text,bool assistant){auto* row=new QWidget(m_conversation);auto* rl=new QHBoxLayout(row);rl->setContentsMargins(0,0,0,0);auto* b=new ChatBubble(sender,text,assistant,row);if(assistant){rl->addStretch();rl->addWidget(b,0,Qt::AlignRight);}else{rl->addWidget(b,0,Qt::AlignLeft);rl->addStretch();}m_conversationLayout->insertWidget(m_conversationLayout->count()-1,row);scrollBottom();return b;}
    void typeNext(){if(!m_typingBubble)return;if(m_typeIndex>=m_typedText.size()){finishTyping();return;}++m_typeIndex;m_typingBubble->setText(m_typedText.left(m_typeIndex)+QStringLiteral("▌"));scrollBottom();}
    void finishTyping(){m_typeTimer.stop();if(m_typingBubble)m_typingBubble->setText(m_typedText);m_typingBubble=nullptr;m_typedText.clear();m_typeIndex=0;m_typing=false;m_waiting=false;m_send->setEnabled(!m_entry->toPlainText().trimmed().isEmpty());m_status->setText(QStringLiteral("Ready"));requestChats();}
    void showMemory(){send({{QStringLiteral("action"),QStringLiteral("memory")}});}
    void showSettings(){SettingsDialog d(this);if(d.exec()==QDialog::Accepted){d.saveSettings();restartBackend();QTimer::singleShot(120,this,&NativeWindow::requestChats);}}
    void showDebug(){QPlainTextEdit* out=new QPlainTextEdit;out->setReadOnly(true);out->setPlainText(m_logs.join("\n"));out->setWindowTitle(QStringLiteral("Debug mode"));out->resize(860,540);out->show();}
    void restartBackend(){stopBackend();startBackend();}
    void scrollBottom(){QTimer::singleShot(0,this,[this](){auto* b=m_scroll->verticalScrollBar();b->setValue(b->maximum());});}
    void log(const QString& s){m_logs<<QStringLiteral("[%1] %2").arg(QDateTime::currentDateTime().toString(Qt::ISODate),s);}

    QProcess* m_backend=nullptr; QListWidget* m_chatList=nullptr; QLabel* m_title=nullptr; QLabel* m_status=nullptr; QScrollArea* m_scroll=nullptr; QWidget* m_conversation=nullptr; QVBoxLayout* m_conversationLayout=nullptr; QTextEdit* m_entry=nullptr; QPushButton* m_send=nullptr; QStringList m_chatNames; QStringList m_logs; QTimer m_typeTimer; ChatBubble* m_typingBubble=nullptr; QString m_typedText; int m_typeIndex=0; bool m_waiting=false; bool m_typing=false;
};

}

int main(int argc,char** argv){QApplication app(argc,argv);app.setOrganizationName(QStringLiteral("Vaxx"));app.setApplicationName(QStringLiteral("AI Chat"));app.setFont(appFont());NativeWindow window;window.show();return app.exec();}
