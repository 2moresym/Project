#include "LiquidGlassWidget.hpp"

#include <QApplication>
#include <QComboBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMainWindow>
#include <QProcess>
#include <QPushButton>
#include <QTextBrowser>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

class MainWindow final : public QMainWindow {
public:
    MainWindow() : m_backend(new QProcess(this)) {
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

        sideLayout->addWidget(new QLabel(QStringLiteral("UI performance")));
        auto* performance = new QComboBox;
        performance->addItems({QStringLiteral("Low GPU"), QStringLiteral("Balanced"), QStringLiteral("Smooth")});
        performance->setCurrentText(QStringLiteral("Balanced"));
        sideLayout->addWidget(performance);

        auto* newChat = new QPushButton(QStringLiteral("＋  New chat"));
        sideLayout->addWidget(newChat);

        auto* glass = new LiquidGlassWidget;
        glass->setMinimumHeight(180);
        sideLayout->addWidget(glass, 1);

        sideLayout->addWidget(new QPushButton(QStringLiteral("Memory")));
        sideLayout->addWidget(new QPushButton(QStringLiteral("Settings")));
        layout->addWidget(sidebar);

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
            "<p>C++ GPU renderer online.</p>"
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
        connect(m_send, &QPushButton::clicked, this, &MainWindow::sendMessage);
        connect(m_entry, &QTextEdit::textChanged, this, [this]() {
            if (!m_pending) {
                m_send->setEnabled(!m_entry->toPlainText().trimmed().isEmpty());
            }
        });
        m_send->setEnabled(false);

        connect(m_backend, &QProcess::readyReadStandardOutput,
                this, &MainWindow::readBackend);
        connect(m_backend, &QProcess::errorOccurred,
                this, [this](QProcess::ProcessError) {
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
            "QTextBrowser,QTextEdit{background:#20242d;color:#f2f4f7;"
            "border:1px solid #2a303b;border-radius:15px;padding:10px;}"
        ));
    }

    ~MainWindow() override {
        if (m_backend) {
            m_backend->closeWriteChannel();
            m_backend->terminate();
            if (!m_backend->waitForFinished(500)) m_backend->kill();
        }
    }

private:
    void startBackend() {
        if (m_backend->state() != QProcess::NotRunning) return;
        m_backend->setProgram(QStringLiteral("python3"));
        m_backend->setArguments({QStringLiteral("-m"), QStringLiteral("src.backend_bridge")});
        m_backend->setWorkingDirectory(QCoreApplication::applicationDirPath() + QStringLiteral("/.."));
        m_backend->start();
    }

    void sendMessage() {
        const QString text = m_entry->toPlainText().trimmed();
        if (text.isEmpty() || m_pending) return;

        startBackend();
        if (!m_backend->waitForStarted(1000)) {
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
        m_backend->write(QJsonDocument(object).toJson(QJsonDocument::Compact));
        m_backend->write("\n");
    }

    void readBackend() {
        while (m_backend->canReadLine()) {
            const QByteArray line = m_backend->readLine().trimmed();
            if (line.isEmpty()) continue;
            const QJsonDocument doc = QJsonDocument::fromJson(line);
            if (!doc.isObject()) continue;
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
