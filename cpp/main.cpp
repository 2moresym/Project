#include "DebugPanel.hpp"
#include "LiquidGlassWidget.hpp"

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QProcess>
#include <QPushButton>
#include <QTextBrowser>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

class MainWindow final : public QMainWindow {
public:
    MainWindow() : m_backend(new QProcess(this)), m_debug(new DebugPanel(this)) {
        setWindowTitle(QStringLiteral("AI Chat — Vaxx"));
        resize(1120, 740);
        setMinimumSize(820, 580);

        auto* root = new QWidget(this);
        auto* layout = new QHBoxLayout(root);
        layout->setContentsMargins(10, 10, 10, 10);
        layout->setSpacing(10);

        auto* sidebar = new QWidget;
        sidebar->setObjectName(QStringLiteral("sidebar"));
        auto* sideLayout = new QVBoxLayout(sidebar);
        sideLayout->setContentsMargins(14, 14, 14, 14);
        sideLayout->setSpacing(8);

        auto* title = new QLabel(QStringLiteral("AI Chat"));
        title->setObjectName(QStringLiteral("appTitle"));
        sideLayout->addWidget(title);

        auto* performanceLabel = new QLabel(QStringLiteral("UI performance"));
        sideLayout->addWidget(performanceLabel);
        auto* performance = new QComboBox;
        performance->addItems({QStringLiteral("Low GPU"), QStringLiteral("Balanced"), QStringLiteral("Smooth")});
        performance->setCurrentText(QStringLiteral("Balanced"));
        sideLayout->addWidget(performance);

        auto* newChat = new QPushButton(QStringLiteral("＋  New chat"));
        sideLayout->addWidget(newChat);

        m_chatList = new QListWidget;
        m_chatList->addItems({QStringLiteral("main")});
        m_chatList->setCurrentRow(0);
        sideLayout->addWidget(m_chatList, 1);

        auto* memory = new QPushButton(QStringLiteral("Memory"));
        auto* settings = new QPushButton(QStringLiteral("Settings"));
        auto* debug = new QPushButton(QStringLiteral("Debug"));
        sideLayout->addWidget(memory);
        sideLayout->addWidget(settings);
        sideLayout->addWidget(debug);
        layout->addWidget(sidebar);

        // Decorative GPU surface. It is deliberately a sibling of the controls,
        // not an overlay, so it can never block sidebar input.
        auto* glass = new LiquidGlassWidget(sidebar);
        glass->setObjectName(QStringLiteral("glass"));
        glass->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        glass->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        glass->lower();
        glass->setGeometry(sidebar->rect());

        connect(sidebar, &QWidget::destroyed, glass, &QObject::deleteLater);
        sidebar->installEventFilter(new GeometryFilter(glass, sidebar));

        auto* main = new QWidget;
        auto* mainLayout = new QVBoxLayout(main);
        mainLayout->setContentsMargins(4, 4, 4, 4);
        mainLayout->setSpacing(10);

        m_header = new QLabel(QStringLiteral("Vaxx  ·  main"));
        m_header->setObjectName(QStringLiteral("header"));
        mainLayout->addWidget(m_header);

        m_output = new QTextBrowser;
        m_output->setOpenExternalLinks(true);
        m_output->setHtml(QStringLiteral(
            "<p><b>Vaxx</b></p>"
            "<p>Native C++/OpenGL renderer online.</p>"
            "<p>Python AI backend connected through a local process bridge.</p>"));
        mainLayout->addWidget(m_output, 1);

        auto* bottom = new QHBoxLayout;
        m_entry = new QTextEdit;
        m_entry->setPlaceholderText(QStringLiteral("Message Vaxx…"));
        m_entry->setFixedHeight(92);
        m_send = new QPushButton(QStringLiteral("Send  ↑"));
        bottom->addWidget(m_entry, 1);
        bottom->addWidget(m_send);
        mainLayout->addLayout(bottom);
        layout->addWidget(main, 1);
        setCentralWidget(root);

        connect(performance, &QComboBox::currentTextChanged,
                glass, &LiquidGlassWidget::setPerformanceProfile);
        glass->setPerformanceProfile(performance->currentText());
        connect(debug, &QPushButton::clicked, this, &MainWindow::showDebug);
        connect(m_send, &QPushButton::clicked, this, &MainWindow::sendMessage);
        connect(m_entry, &QTextEdit::textChanged, this, [this]() {
            if (!m_pending) {
                m_send->setEnabled(!m_entry->toPlainText().trimmed().isEmpty());
            }
        });
        m_send->setEnabled(false);

        connect(m_backend, &QProcess::readyReadStandardOutput,
                this, &MainWindow::readBackend);
        connect(m_backend, &QProcess::readyReadStandardError,
                this, [this]() {
                    const QString error = QString::fromUtf8(m_backend->readAllStandardError()).trimmed();
                    if (!error.isEmpty()) m_debug->log(QStringLiteral("backend stderr: %1").arg(error));
                });
        connect(m_backend, &QProcess::errorOccurred,
                this, [this](QProcess::ProcessError) {
                    m_debug->log(QStringLiteral("backend process error: %1").arg(m_backend->errorString()));
                    m_output->append(QStringLiteral("<p><b>Backend error:</b> %1</p>").arg(m_backend->errorString().toHtmlEscaped()));
                    m_pending = false;
                    m_send->setEnabled(!m_entry->toPlainText().trimmed().isEmpty());
                });

        setStyleSheet(QStringLiteral(
            "QMainWindow,QWidget{background:#111318;color:#f2f4f7;font-family:'Noto Sans';font-size:11pt;}"
            "#sidebar{background:#181b22;border:1px solid #2a303b;border-radius:18px;}"
            "#appTitle{font-size:18pt;font-weight:700;}"
            "#header{font-size:14pt;font-weight:650;padding:6px;}"
            "QPushButton,QComboBox{background:#181b22;color:#f2f4f7;"
            "border:1px solid #2a303b;border-radius:11px;padding:9px 13px;}"
            "QListWidget{background:#20242d;color:#f2f4f7;border:1px solid #2a303b;border-radius:12px;padding:6px;}"
            "QListWidget::item{padding:8px;border-radius:8px;}"
            "QListWidget::item:selected{background:#5b8cff;color:white;}"
            "QTextBrowser,QTextEdit{background:#20242d;color:#f2f4f7;"
            "border:1px solid #2a303b;border-radius:15px;padding:10px;}"
        ));

        m_debug->log(QStringLiteral("window created"));
        m_debug->log(QStringLiteral("Qt version: %1").arg(QT_VERSION_STR));
        m_debug->log(QStringLiteral("OpenGL widget created"));
        m_debug->setStatus(QStringLiteral("startup complete"));
    }

    ~MainWindow() override {
        if (m_backend) {
            m_backend->closeWriteChannel();
            m_backend->terminate();
            if (!m_backend->waitForFinished(500)) m_backend->kill();
        }
    }

private:
    class GeometryFilter final : public QObject {
    public:
        GeometryFilter(QWidget* target, QObject* parent) : QObject(parent), m_target(target) {}
        bool eventFilter(QObject* watched, QEvent* event) override {
            if (watched == parent() && event->type() == QEvent::Resize && m_target) {
                m_target->setGeometry(qobject_cast<QWidget*>(watched)->rect());
            }
            return QObject::eventFilter(watched, event);
        }
    private:
        QWidget* m_target;
    };

    void showDebug() {
        m_debug->log(QStringLiteral("backend state: %1").arg(m_backend->state()));
        m_debug->log(QStringLiteral("backend program: %1").arg(m_backend->program()));
        m_debug->log(QStringLiteral("chat count: %1").arg(m_chatList->count()));
        m_debug->log(QStringLiteral("pending request: %1").arg(m_pending ? QStringLiteral("yes") : QStringLiteral("no")));
        m_debug->setStatus(QStringLiteral("debug snapshot captured"));
        m_debug->show();
        m_debug->raise();
        m_debug->activateWindow();
    }

    void startBackend() {
        if (m_backend->state() != QProcess::NotRunning) return;
        const QString root = QCoreApplication::applicationDirPath() + QStringLiteral("/..");
        m_backend->setProgram(QStringLiteral("python3"));
        m_backend->setArguments({QStringLiteral("-m"), QStringLiteral("src.backend_bridge")});
        m_backend->setWorkingDirectory(root);
        m_debug->log(QStringLiteral("starting backend in %1").arg(root));
        m_backend->start();
    }

    void sendMessage() {
        const QString text = m_entry->toPlainText().trimmed();
        if (text.isEmpty() || m_pending) return;

        startBackend();
        if (!m_backend->waitForStarted(1000)) {
            m_debug->log(QStringLiteral("backend failed to start: %1").arg(m_backend->errorString()));
            m_output->append(QStringLiteral("<p><b>Backend:</b> failed to start.</p>"));
            return;
        }

        const QString escaped = text.toHtmlEscaped();
        m_output->append(QStringLiteral("<p><b>You</b></p><p>%1</p>").arg(escaped));
        m_entry->clear();
        m_send->setEnabled(false);
        m_pending = true;

        QJsonObject object;
        object.insert(QStringLiteral("action"), QStringLiteral("reply"));
        object.insert(QStringLiteral("text"), text);
        const QByteArray payload = QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
        m_debug->log(QStringLiteral("sending backend request (%1 bytes)").arg(payload.size()));
        m_backend->write(payload);
    }

    void readBackend() {
        while (m_backend->canReadLine()) {
            const QByteArray line = m_backend->readLine().trimmed();
            if (line.isEmpty()) continue;
            m_debug->log(QStringLiteral("backend response: %1").arg(QString::fromUtf8(line)));
            const QJsonDocument doc = QJsonDocument::fromJson(line);
            if (!doc.isObject()) {
                m_debug->log(QStringLiteral("ignored non-object backend response"));
                continue;
            }
            const QJsonObject object = doc.object();
            if (object.value(QStringLiteral("ok")).toBool()) {
                const QString answer = object.value(QStringLiteral("answer")).toString();
                m_output->append(QStringLiteral("<p><b>Vaxx</b></p><p>%1</p>")
                    .arg(answer.toHtmlEscaped().replace("\n", "<br>")));
            } else {
                const QString error = object.value(QStringLiteral("error")).toString();
                m_output->append(QStringLiteral("<p><b>Vaxx error:</b> %1</p>").arg(error.toHtmlEscaped()));
            }
            m_pending = false;
            m_send->setEnabled(!m_entry->toPlainText().trimmed().isEmpty());
        }
    }

    QProcess* m_backend = nullptr;
    DebugPanel* m_debug = nullptr;
    QListWidget* m_chatList = nullptr;
    QLabel* m_header = nullptr;
    QTextBrowser* m_output = nullptr;
    QTextEdit* m_entry = nullptr;
    QPushButton* m_send = nullptr;
    bool m_pending = false;
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("AI Chat"));
    MainWindow window;
    window.show();
    return app.exec();
}
