#include "LiquidGlassWidget.hpp"

#include <QApplication>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QProcess>
#include <QPushButton>
#include <QTextBrowser>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

class MainWindow final : public QMainWindow {
public:
    MainWindow() {
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

        auto* performance = new QComboBox;
        performance->addItems({QStringLiteral("Low GPU"), QStringLiteral("Balanced"), QStringLiteral("Smooth")});
        performance->setCurrentText(QStringLiteral("Balanced"));
        sideLayout->addWidget(new QLabel(QStringLiteral("UI performance")));
        sideLayout->addWidget(performance);

        auto* glassHost = new QWidget;
        glassHost->setObjectName(QStringLiteral("glassHost"));
        auto* glass = new LiquidGlassWidget(glassHost);
        glass->setGeometry(glassHost->rect());
        glassHost->installEventFilter(new QObject(glassHost));
        Q_UNUSED(glass);

        auto* newChat = new QPushButton(QStringLiteral("＋  New chat"));
        sideLayout->addWidget(newChat);
        sideLayout->addWidget(glassHost, 1);

        auto* memory = new QPushButton(QStringLiteral("Memory"));
        auto* settings = new QPushButton(QStringLiteral("Settings"));
        sideLayout->addWidget(memory);
        sideLayout->addWidget(settings);

        layout->addWidget(sidebar);

        auto* main = new QWidget;
        auto* mainLayout = new QVBoxLayout(main);
        mainLayout->setContentsMargins(4, 4, 4, 4);
        mainLayout->setSpacing(10);

        auto* header = new QLabel(QStringLiteral("Vaxx  ·  main"));
        header->setObjectName(QStringLiteral("header"));
        mainLayout->addWidget(header);

        auto* output = new QTextBrowser;
        output->setOpenExternalLinks(true);
        output->setHtml(QStringLiteral("<p><b>Vaxx</b></p><p>C++ GPU renderer online.</p><p>The AI backend remains Python.</p>"));
        mainLayout->addWidget(output, 1);

        auto* bottom = new QHBoxLayout;
        auto* entry = new QTextEdit;
        entry->setPlaceholderText(QStringLiteral("Message Vaxx…"));
        entry->setFixedHeight(92);
        auto* send = new QPushButton(QStringLiteral("Send  ↑"));
        bottom->addWidget(entry, 1);
        bottom->addWidget(send);
        mainLayout->addLayout(bottom);
        layout->addWidget(main, 1);
        setCentralWidget(root);

        connect(performance, &QComboBox::currentTextChanged, glass, &LiquidGlassWidget::setPerformanceProfile);
        glass->setPerformanceProfile(performance->currentText());

        setStyleSheet(QStringLiteral(
            "QMainWindow,QWidget{background:#111318;color:#f2f4f7;font-family:'Noto Sans';font-size:11pt;}"
            "#sidebar{background:#181b22;border:1px solid #2a303b;border-radius:18px;}"
            "#appTitle{font-size:18pt;font-weight:700;}"
            "#header{font-size:14pt;font-weight:650;padding:6px;}"
            "QPushButton,QComboBox{background:#181b22;color:#f2f4f7;border:1px solid #2a303b;border-radius:11px;padding:9px 13px;}"
            "QTextBrowser,QTextEdit{background:#20242d;color:#f2f4f7;border:1px solid #2a303b;border-radius:15px;padding:10px;}"
        ));
    }
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("AI Chat"));
    MainWindow window;
    window.show();
    return app.exec();
}
