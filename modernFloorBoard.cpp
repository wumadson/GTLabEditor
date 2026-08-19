#include "modernFloorBoard.h"
#include "SysxIO.h"

#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QTimer>

modernFloorBoard::modernFloorBoard(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("ModernFloorBoard");
    setMinimumSize(1200, 720);

    setStyleSheet(R"(
        QWidget#ModernFloorBoard {
            background: #0b0d10;
            color: #e8e8e8;
            font-family: "Helvetica Neue";
        }

        QLabel#Title {
            font-size: 25px;
            font-weight: 700;
        }

        QLabel#Subtitle {
            color: #8b929b;
            font-size: 12px;
        }

        QLabel#Connected {
            color: #27df70;
            font-size: 13px;
            font-weight: 700;
        }

        QFrame#Header {
            background: #111419;
            border-bottom: 1px solid #272c33;
        }

        QFrame#Sidebar {
            background: #101318;
            border-right: 1px solid #272c33;
        }

        QListWidget {
            background: transparent;
            border: none;
            color: #cdd2d8;
            font-size: 13px;
            outline: none;
        }

        QListWidget::item {
            padding: 10px;
            border-radius: 5px;
        }

        QListWidget::item:selected {
            background: #173c68;
            color: white;
        }

        QPushButton {
            background: #191d23;
            border: 1px solid #343a43;
            border-radius: 6px;
            padding: 8px 16px;
            color: #e5e5e5;
        }

        QPushButton:hover {
            background: #252a31;
        }
    )");

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // HEADER
    QFrame *header = new QFrame;
    header->setObjectName("Header");
    header->setFixedHeight(100);

    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(28, 18, 28, 18);

    QVBoxLayout *brandLayout = new QVBoxLayout;

    QLabel *title = new QLabel("BOSS GT-10");
    title->setObjectName("Title");

    QLabel *subtitle = new QLabel("FX FLOORBOARD  •  MODERN EDITION");
    subtitle->setObjectName("Subtitle");

    brandLayout->addWidget(title);
    brandLayout->addWidget(subtitle);

    headerLayout->addLayout(brandLayout);
    headerLayout->addStretch();

    patchNumber = new QLabel("U01-1");
    patchNumber->setStyleSheet(
        "font-size:22px; font-weight:700; color:#37a8ff;"
    );

    patchName = new QLabel("WORSHIP");
    patchName->setStyleSheet(
        "font-size:22px; font-weight:600;"
    );

    connectionStatus = new QLabel("●  GT-10 USB Bridge");
    connectionStatus->setObjectName("Connected");

    headerLayout->addWidget(patchNumber);
    headerLayout->addSpacing(18);
    headerLayout->addWidget(patchName);
    headerLayout->addSpacing(40);
    headerLayout->addWidget(connectionStatus);

    root->addWidget(header);

    // BODY
    QHBoxLayout *body = new QHBoxLayout;
    body->setSpacing(0);

    // SIDEBAR
    QFrame *sidebar = new QFrame;
    sidebar->setObjectName("Sidebar");
    sidebar->setFixedWidth(235);

    QVBoxLayout *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(16, 20, 16, 20);

    QLabel *presetTitle = new QLabel("PRESET BANK");
    presetTitle->setStyleSheet(
        "color:#8b929b; font-weight:700; font-size:11px;"
    );

    presetList = new QListWidget;

    presetList->addItem("U01-1    WORSHIP");
    presetList->addItem("U01-2    PP R K PEDAL");
    presetList->addItem("U01-3    BALAD PEDAL");
    presetList->addItem("U01-4    BLACK PEDAL");
    presetList->addItem("U01-5    CLEAN");
    presetList->addItem("U02-1    DRIVE");
    presetList->addItem("U02-2    AMBIENT");

    presetList->setCurrentRow(0);

    sideLayout->addWidget(presetTitle);
    sideLayout->addSpacing(10);
    sideLayout->addWidget(presetList);

    body->addWidget(sidebar);

    // MAIN AREA
    QWidget *mainArea = new QWidget;

    QVBoxLayout *mainLayout = new QVBoxLayout(mainArea);
    mainLayout->setContentsMargins(24, 22, 24, 24);
    mainLayout->setSpacing(18);

    QLabel *chainTitle = new QLabel("EFFECT CHAIN");
    chainTitle->setStyleSheet(
        "color:#8b929b; font-weight:700; font-size:11px;"
    );

    mainLayout->addWidget(chainTitle);

    QHBoxLayout *chain = new QHBoxLayout;
    chain->setSpacing(12);

    chain->addWidget(createEffectBlock("COMP", "SUSTAIN", "#19a974"));
    chain->addWidget(createEffectBlock("OD/DS", "BLUES", "#d7aa19"));
    chain->addWidget(createEffectBlock("PREAMP", "VO DRIVE", "#e44747"));
    chain->addWidget(createEffectBlock("EQ", "PARAMETRIC", "#2789d8"));
    chain->addWidget(createEffectBlock("FX-1", "TREMOLO", "#9254c8"));
    chain->addWidget(createEffectBlock("FX-2", "CHORUS", "#9254c8"));
    chain->addWidget(createEffectBlock("DELAY", "DIGITAL", "#348bd4"));
    chain->addWidget(createEffectBlock("REVERB", "MODULATE", "#1db5a6"));

    mainLayout->addLayout(chain);

    QFrame *editor = new QFrame;
    editor->setStyleSheet(
        "background:#111419;"
        "border:1px solid #292e35;"
        "border-radius:10px;"
    );

    QVBoxLayout *editorLayout = new QVBoxLayout(editor);
    editorLayout->setContentsMargins(24, 20, 24, 20);

    QLabel *editorTitle = new QLabel("PREAMP");
    editorTitle->setStyleSheet(
        "font-size:18px; font-weight:700; color:#e44747;"
    );

    QLabel *placeholder = new QLabel(
        "Modern Effect Editor\n\n"
        "Aqui entraremos com os parâmetros REAIS da GT-10."
    );

    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet(
        "color:#69717c; font-size:16px;"
    );

    editorLayout->addWidget(editorTitle);
    editorLayout->addWidget(placeholder, 1);

    mainLayout->addWidget(editor, 1);

    body->addWidget(mainArea, 1);

    root->addLayout(body, 1);

    // Atualiza status real do backend periodicamente.
    QTimer *statusTimer = new QTimer(this);
    connect(statusTimer, SIGNAL(timeout()),
            this, SLOT(refreshBackendStatus()));
    statusTimer->start(500);

    refreshBackendStatus();
}

QFrame *modernFloorBoard::createEffectBlock(
    const QString &name,
    const QString &subtitle,
    const QString &color)
{
    QFrame *frame = new QFrame;

    frame->setMinimumWidth(100);
    frame->setMaximumWidth(145);
    frame->setFixedHeight(145);

    frame->setStyleSheet(
        QString(
            "QFrame {"
            "background:#15191f;"
            "border:1px solid %1;"
            "border-radius:9px;"
            "}"
        ).arg(color)
    );

    QVBoxLayout *layout = new QVBoxLayout(frame);

    QLabel *nameLabel = new QLabel(name);
    nameLabel->setAlignment(Qt::AlignCenter);

    nameLabel->setStyleSheet(
        QString(
            "font-size:15px;"
            "font-weight:700;"
            "color:%1;"
            "border:none;"
        ).arg(color)
    );

    QLabel *subLabel = new QLabel(subtitle);
    subLabel->setAlignment(Qt::AlignCenter);
    subLabel->setStyleSheet(
        "font-size:10px;"
        "color:#969da6;"
        "border:none;"
    );

    QLabel *led = new QLabel("●");
    led->setAlignment(Qt::AlignCenter);
    led->setStyleSheet(
        "font-size:20px;"
        "color:#28df70;"
        "border:none;"
    );

    QPushButton *button = new QPushButton("ON");

    if (name == "REVERB")
    {
        reverbButton = button;
        reverbLed = led;

        connect(reverbButton, SIGNAL(clicked()),
                this, SLOT(toggleReverb()));
    }

    button->setStyleSheet(
        "QPushButton {"
        "background:#102418;"
        "border:1px solid #1d6a36;"
        "color:#2ee66b;"
        "border-radius:5px;"
        "padding:5px;"
        "font-weight:700;"
        "}"
    );

    layout->addWidget(nameLabel);
    layout->addWidget(subLabel);
    layout->addStretch();
    layout->addWidget(led);
    layout->addWidget(button);

    return frame;
}


void modernFloorBoard::refreshBackendStatus()
{
    SysxIO *sysxIO = SysxIO::Instance();

    if (sysxIO->isConnected())
    {
        connectionStatus->setText("●  GT-10 CONNECTED");
        connectionStatus->setStyleSheet(
            "color:#27df70; font-size:13px; font-weight:700;"
        );

        if (reverbButton && reverbLed)
        {
            QString area;
            int value = sysxIO->getSourceValue(
                area, "0A", "00", "30"
            );

            bool on = (value == 1);

            reverbButton->setText(on ? "ON" : "OFF");

            reverbLed->setStyleSheet(
                on
                ? "font-size:20px;color:#28df70;border:none;"
                : "font-size:20px;color:#4b5159;border:none;"
            );
        }
    }
    else
    {
        connectionStatus->setText("●  NOT CONNECTED");
        connectionStatus->setStyleSheet(
            "color:#e05252; font-size:13px; font-weight:700;"
        );
    }
}

void modernFloorBoard::toggleReverb()
{
    SysxIO *sysxIO = SysxIO::Instance();

    if (!sysxIO->isConnected())
        return;

    QString area;

    // Consulta o estado REAL do patch no engine.
    int current = sysxIO->getSourceValue(
        area,
        "0A",
        "00",
        "30"
    );

    bool newState = (current != 1);

    // Mesmo endereco utilizado pelo stompbox_rv original.
    sysxIO->setFileSource(
        area,
        "0A",
        "00",
        "30",
        newState ? "01" : "00"
    );

    refreshBackendStatus();
}
