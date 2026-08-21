#include "modernFloorBoard.h"
#include "SysxIO.h"
#include "MidiTable.h"
#include "modernTheme.h"
#include "modernWidgets.h"
#include "effectArtworkWidget.h"
#include "effectModelBrowser.h"
#include "parameterBar.h"
#include "patchSidebar.h"

#include <QComboBox>
#include <QDial>
#include <QFrame>
#include <QGridLayout>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QResizeEvent>
#include <QEvent>
#include <QTimer>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QVBoxLayout>
#include "globalVariables.h"

#include <algorithm>

namespace {
QString oddsArtworkType(QString type)
{
    if (type.startsWith('(')) {
        const int categoryEnd = type.indexOf(") ");
        if (categoryEnd >= 0)
            type = type.mid(categoryEnd + 2);
    }
    return type.toUpper();
}

QString delayArtworkType(QString type)
{
    type = type.trimmed().toUpper();
    if (type.startsWith("DUAL") && type.size() > 4 && type.at(4) != ' ')
        type.insert(4, ' ');
    return type;
}

QString formatRhythmicDivision(const QString &value,
                               bool *recognized = nullptr)
{
    static const QHash<QString, QString> compactDivisions = {
        {"sixteenth note", "1/16"},
        {"eighth note triplet", "1/8T"},
        {"dotted sixteenth note", "1/16D"},
        {"doted sixteenth note", "1/16D"},
        {"eighth note", "1/8"},
        {"quarter note triplet", "1/4T"},
        {"dotted eighth note", "1/8D"},
        {"doted eighth note", "1/8D"},
        {"quarter note", "1/4"},
        {"half note triplet", "1/2T"},
        {"dotted quarter note", "1/4D"},
        {"doted quarter note", "1/4D"},
        {"half note", "1/2"},
        {"whole note triplet", "1/1T"},
        {"dotted half note", "1/2D"},
        {"doted half note", "1/2D"},
        {"whole note", "1/1"}
    };
    const QString compact = compactDivisions.value(
        value.trimmed().toLower());
    if (recognized)
        *recognized = !compact.isEmpty();
    return compact.isEmpty() ? value : compact;
}

QVector<int> rhythmicDivisionRawValues(const Midi &parameter)
{
    struct RhythmicEntry {
        int raw;
        QString display;
    };

    QVector<RhythmicEntry> entries;
    for (const Midi &highByte : parameter.level) {
        bool highByteOk = false;
        const int high = highByte.value.toInt(&highByteOk, 16);
        if (!highByteOk || highByte.level.isEmpty())
            continue;

        for (const Midi &entry : highByte.level) {
            if (entry.value == "range")
                continue;

            bool recognized = false;
            const QString display = formatRhythmicDivision(
                entry.name, &recognized);
            if (!recognized)
                continue;

            bool lowByteOk = false;
            const int low = entry.value.toInt(&lowByteOk, 16);
            if (lowByteOk)
                entries.append({high * 128 + low, display});
        }
    }

    static const QStringList visualOrder = {
        "1/1", "1/1T",
        "1/2", "1/2D", "1/2T",
        "1/4", "1/4D", "1/4T",
        "1/8", "1/8D", "1/8T",
        "1/16", "1/16D"
    };

    QVector<int> rawValues;
    rawValues.reserve(entries.size());
    for (const QString &display : visualOrder) {
        for (const RhythmicEntry &entry : entries) {
            if (entry.display == display) {
                rawValues.append(entry.raw);
                break;
            }
        }
    }
    return rawValues;
}

bool centerValueFromMapping(const Midi &parameter, int *rawCenter)
{
    if (!rawCenter || parameter.level.isEmpty())
        return false;
    const Midi range = parameter.level.last();
    if (range.value != "range")
        return false;

    const QStringList parts = range.name.split('/');
    if (parts.size() < 4)
        return false;

    bool rawMinimumOk = false;
    bool rawMaximumOk = false;
    bool displayMinimumOk = false;
    bool displayMaximumOk = false;
    const int rawMinimum = parts.at(0).toInt(&rawMinimumOk, 16);
    const int rawMaximum = parts.at(1).toInt(&rawMaximumOk, 16);
    const qreal displayMinimum = parts.at(2).toDouble(&displayMinimumOk);
    const qreal displayMaximum = parts.at(3).toDouble(&displayMaximumOk);
    if (!rawMinimumOk || !rawMaximumOk || !displayMinimumOk
        || !displayMaximumOk || rawMaximum <= rawMinimum
        || displayMinimum >= 0.0 || displayMaximum <= 0.0)
        return false;

    const qreal ratio = -displayMinimum
        / (displayMaximum - displayMinimum);
    *rawCenter = qRound(rawMinimum
        + ratio * (rawMaximum - rawMinimum));
    return true;
}
}

modernFloorBoard::modernFloorBoard(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("ModernFloorBoard");
    setMinimumSize(1280, 800);

    setStyleSheet(ModernTheme::applicationStyleSheet());

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // HEADER
    QFrame *header = new QFrame;
    header->setObjectName("AppHeader");
    header->setFixedHeight(60);

    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(16, 6, 16, 6);

    QVBoxLayout *brandLayout = new QVBoxLayout;

    QLabel *title = new QLabel("GT LAB Editor");
    title->setObjectName("BrandTitle");

    QLabel *subtitle = new QLabel("BOSS GT-10");
    subtitle->setObjectName("BrandSubtitle");

    brandLayout->addWidget(title);
    brandLayout->addWidget(subtitle);

    headerLayout->addLayout(brandLayout);
    headerLayout->addStretch();

    QVBoxLayout *patchNumberLayout = new QVBoxLayout;
    patchNumberLayout->setSpacing(1);
    QLabel *patchCaption = new QLabel("PATCH");
    patchCaption->setObjectName("PatchCaption");
    patchNumber = new QLabel(QString::fromUtf8("—"));
    patchNumber->setObjectName("PatchNumber");
    patchNumberLayout->addWidget(patchCaption);
    patchNumberLayout->addWidget(patchNumber);

    patchName = new QLabel("NO PATCH DATA");
    patchName->setObjectName("PatchName");

    connectionStatus = new StatusBadge;

    headerLayout->addLayout(patchNumberLayout);
    headerLayout->addSpacing(14);
    headerLayout->addWidget(patchName);
    headerLayout->addSpacing(24);
    headerLayout->addWidget(connectionStatus);

    root->addWidget(header);

    // BODY
    QHBoxLayout *body = new QHBoxLayout;
    body->setSpacing(0);

    patchSidebar = new PatchSidebar(&patchListModel);
    connect(patchSidebar, SIGNAL(bankExpanded(int)),
            this, SIGNAL(requestPatchNames(int)));
    connect(patchSidebar, SIGNAL(patchActivated(int,int,QString)),
            this, SIGNAL(selectPatchRequested(int,int,QString)));
    body->addWidget(patchSidebar);

    // MAIN AREA
    QWidget *mainArea = new QWidget;

    QVBoxLayout *mainLayout = new QVBoxLayout(mainArea);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(7);

    QLabel *chainTitle = new QLabel("SIGNAL CHAIN");
    chainTitle->setObjectName("SectionTitle");

    mainLayout->addWidget(chainTitle);

    signalChainPanel = new SignalChainPanel;
    signalChainScroll = new QScrollArea(signalChainPanel);
    signalChainScroll->setWidgetResizable(true);
    signalChainScroll->setFrameShape(QFrame::NoFrame);
    signalChainScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    signalChainScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    signalChainScroll->setStyleSheet(QString(
        "QScrollArea{background:transparent;border:none;}"
        "QScrollBar:horizontal{height:8px;background:%1;}"
        "QScrollBar::handle:horizontal{background:%2;border-radius:2px;min-width:40px;}")
        .arg(ModernTheme::color(ModernTheme::ControlBackground),
             ModernTheme::color(ModernTheme::Border)));
    signalChainScroll->viewport()->installEventFilter(this);
    QVBoxLayout *chainPanelLayout = new QVBoxLayout(signalChainPanel);
    chainPanelLayout->setContentsMargins(6, 6, 6, 6);
    chainPanelLayout->addWidget(signalChainScroll);
    signalChainPanel->setFixedHeight(185);
    mainLayout->addWidget(signalChainPanel);
    rebuildSignalChainView();

    effectEditorStack = new QStackedWidget;

    reverbEditor = new EffectEditorPanel("REVERB");
    reverbTypeDisplay = reverbEditor->typeLabel();
    reverbTypeDisplay->hide();
    reverbModelBrowser = new EffectModelBrowser;
    reverbModelBrowser->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("REVERB")));
    reverbEditor->setModelBrowserWidget(reverbModelBrowser);
    connect(reverbModelBrowser, &EffectModelBrowser::modelSelected,
            this, &modernFloorBoard::reverbModelSelected);
    reverbArtwork = new EffectArtworkWidget;
    reverbArtwork->setArtwork(":/assets/effects/reverb.png");
    QFont reverbDisplayFont;
    reverbDisplayFont.setFamily("Menlo");
    reverbDisplayFont.setBold(true);
    reverbDisplayFont.setStretch(QFont::Condensed);
    reverbArtwork->setTextOverlay(
        "type", QRectF(0.30, 0.244, 0.40, 0.064), QString(),
        reverbDisplayFont, QColor("#35F238"), Qt::AlignCenter, 0.66);
    reverbEditor->setArtworkWidget(reverbArtwork);
    QVBoxLayout *parameterLayout = new QVBoxLayout(reverbEditor->parameterArea());
    parameterLayout->setContentsMargins(0, 0, 0, 0);
    parameterLayout->setSpacing(8);

    QWidget *reverbPrimaryControls = new QWidget;
    QHBoxLayout *reverbPrimaryLayout =
        new QHBoxLayout(reverbPrimaryControls);
    reverbPrimaryLayout->setContentsMargins(0, 0, 0, 0);
    reverbPrimaryLayout->setSpacing(0);
    EffectToggleControl *reverbToggle = new EffectToggleControl("State");
    reverbOnOff = reverbToggle->toggle();
    reverbOnOff->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("REVERB")));
    connect(reverbOnOff, SIGNAL(clicked()), this, SLOT(toggleReverb()));
    reverbPrimaryLayout->addWidget(reverbToggle, 0, Qt::AlignTop);
    reverbPrimaryLayout->addStretch(1);
    parameterLayout->addWidget(reverbPrimaryControls);

    QWidget *reverbTypeControl = createReverbCombo("Type", "31");
    reverbTypeControl->setParent(reverbEditor->parameterArea());
    reverbTypeControl->hide();

    QLabel *spaceTitle = new QLabel("SPACE");
    spaceTitle->setObjectName("ParameterSectionTitle");
    parameterLayout->addWidget(spaceTitle);
    parameterLayout->addWidget(createReverbBar("Time", "32"));
    parameterLayout->addWidget(createReverbBar("Pre Delay", "3A", true));
    parameterLayout->addWidget(createReverbBar("Density", "36"));
    parameterLayout->addWidget(
        createReverbBar("Spring Sensitivity", "39"));

    QLabel *filterTitle = new QLabel("FILTER");
    filterTitle->setObjectName("ParameterSectionTitle");
    parameterLayout->addWidget(filterTitle);
    parameterLayout->addWidget(createReverbCombo("Low Cut", "34"));
    parameterLayout->addWidget(createReverbCombo("High Cut", "35"));

    QLabel *mixTitle = new QLabel("MIX");
    mixTitle->setObjectName("ParameterSectionTitle");
    parameterLayout->addWidget(mixTitle);
    parameterLayout->addWidget(createReverbBar("Effect Level", "37"));
    parameterLayout->addWidget(createReverbBar("Direct Level", "38"));
    parameterLayout->addStretch(1);
    effectEditorStack->addWidget(reverbEditor);

    compEditor = new EffectEditorPanel("COMP");
    compTypeDisplay = compEditor->typeLabel();
    compTypeDisplay->hide();
    compModelBrowser = new EffectModelBrowser;
    compModelBrowser->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("COMP")));
    compEditor->setModelBrowserWidget(compModelBrowser);
    connect(compModelBrowser, &EffectModelBrowser::modelSelected,
            this, &modernFloorBoard::compModelSelected);
    EffectArtworkWidget *compArtwork = new EffectArtworkWidget;
    compArtwork->setArtwork(":/assets/effects/comp.png");
    compEditor->setArtworkWidget(compArtwork);

    QVBoxLayout *compParameterLayout = new QVBoxLayout(compEditor->parameterArea());
    compParameterLayout->setContentsMargins(0, 0, 0, 0);
    compParameterLayout->setSpacing(8);

    QWidget *compPrimaryControls = new QWidget;
    QHBoxLayout *compPrimaryLayout = new QHBoxLayout(compPrimaryControls);
    compPrimaryLayout->setContentsMargins(0, 0, 0, 0);
    compPrimaryLayout->setSpacing(0);
    EffectToggleControl *compToggle = new EffectToggleControl("State");
    compOnOff = compToggle->toggle();
    compOnOff->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("COMP")));
    connect(compOnOff, SIGNAL(clicked()), this, SLOT(toggleComp()));
    compPrimaryLayout->addWidget(compToggle, 0, Qt::AlignTop);
    compPrimaryLayout->addStretch(1);
    compParameterLayout->addWidget(compPrimaryControls);
    QWidget *compTypeControl = createCompCombo("Type", "41");
    compTypeControl->setParent(compEditor->parameterArea());
    compTypeControl->hide();

    compModeStack = new QStackedWidget;
    QWidget *compressorPage = new QWidget;
    QVBoxLayout *compressorLayout = new QVBoxLayout(compressorPage);
    compressorLayout->setContentsMargins(0, 0, 0, 0);
    compressorLayout->setSpacing(7);
    QLabel *compressorTitle = new QLabel("COMPRESSOR");
    compressorTitle->setObjectName("ParameterSectionTitle");
    compressorLayout->addWidget(compressorTitle);
    compressorLayout->addWidget(createCompBar("Sustain", "42"));
    compressorLayout->addWidget(createCompBar("Attack", "43"));
    compressorLayout->addWidget(createCompBar("Tone", "46"));
    compressorLayout->addWidget(createCompBar("Level", "47"));
    compressorLayout->addStretch(1);

    QWidget *limiterPage = new QWidget;
    QVBoxLayout *limiterLayout = new QVBoxLayout(limiterPage);
    limiterLayout->setContentsMargins(0, 0, 0, 0);
    limiterLayout->setSpacing(7);
    QLabel *limiterTitle = new QLabel("LIMITER");
    limiterTitle->setObjectName("ParameterSectionTitle");
    limiterLayout->addWidget(limiterTitle);
    limiterLayout->addWidget(createCompBar("Threshold", "44"));
    limiterLayout->addWidget(createCompBar("Release", "45"));
    limiterLayout->addWidget(createCompBar("Tone", "46"));
    limiterLayout->addWidget(createCompBar("Level", "47"));
    limiterLayout->addStretch(1);

    compModeStack->addWidget(compressorPage);
    compModeStack->addWidget(limiterPage);
    compParameterLayout->addWidget(compModeStack, 1);
    effectEditorStack->addWidget(compEditor);

    oddsEditor = new EffectEditorPanel("OD/DS");
    oddsEditor->typeLabel()->hide();
    oddsModelBrowser = new EffectModelBrowser;
    oddsModelBrowser->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("OD/DS")));
    oddsEditor->setModelBrowserWidget(oddsModelBrowser);
    connect(oddsModelBrowser, &EffectModelBrowser::modelSelected,
            this, &modernFloorBoard::oddsModelSelected);
    oddsArtwork = new EffectArtworkWidget;
    oddsArtwork->setArtwork(":/assets/effects/od_ds.png");
    QFont oddsDisplayFont;
    oddsDisplayFont.setFamily("Menlo");
    oddsDisplayFont.setBold(true);
    oddsDisplayFont.setStretch(QFont::Condensed);
    oddsArtwork->setTextOverlay(
        "type", QRectF(0.27, 0.245, 0.46, 0.058), QString(),
        oddsDisplayFont, QColor("#FFC21A"), Qt::AlignCenter, 0.52);
    oddsEditor->setArtworkWidget(oddsArtwork);

    QVBoxLayout *oddsParameterLayout =
        new QVBoxLayout(oddsEditor->parameterArea());
    oddsParameterLayout->setContentsMargins(0, 0, 0, 0);
    oddsParameterLayout->setSpacing(8);

    QWidget *oddsPrimaryControls = new QWidget;
    QHBoxLayout *oddsPrimaryLayout = new QHBoxLayout(oddsPrimaryControls);
    oddsPrimaryLayout->setContentsMargins(0, 0, 0, 0);
    oddsPrimaryLayout->setSpacing(0);
    EffectToggleControl *oddsToggle = new EffectToggleControl("State");
    oddsOnOff = oddsToggle->toggle();
    oddsOnOff->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("OD/DS")));
    oddsOnOff->setProperty("address", "70");
    connect(oddsOnOff, SIGNAL(clicked()), this, SLOT(oddsToggleChanged()));
    oddsPrimaryLayout->addWidget(oddsToggle, 0, Qt::AlignTop);
    oddsPrimaryLayout->addStretch(1);
    oddsParameterLayout->addWidget(oddsPrimaryControls);

    QWidget *oddsTypeControl = createOddsCombo("Type", "71");
    oddsTypeControl->setParent(oddsEditor->parameterArea());
    oddsTypeControl->hide();

    QLabel *oddsCommonTitle = new QLabel("DRIVE / MIX");
    oddsCommonTitle->setObjectName("ParameterSectionTitle");
    oddsParameterLayout->addWidget(oddsCommonTitle);
    oddsParameterLayout->addWidget(createOddsBar("Drive", "72"));
    oddsParameterLayout->addWidget(createOddsBar("Bottom", "73"));
    oddsParameterLayout->addWidget(createOddsBar("Tone", "74"));
    oddsParameterLayout->addWidget(createOddsBar("Effect", "75"));
    oddsParameterLayout->addWidget(createOddsBar("Direct", "76"));
    oddsParameterLayout->addWidget(createOddsBar("Solo Level", "78"));

    QLabel *oddsSoloTitle = new QLabel("SOLO");
    oddsSoloTitle->setObjectName("ParameterSectionTitle");
    oddsParameterLayout->addWidget(oddsSoloTitle);
    EffectToggleControl *oddsSolo = new EffectToggleControl("Solo Switch");
    oddsSoloSwitch = oddsSolo->toggle();
    oddsSoloSwitch->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("OD/DS")));
    oddsSoloSwitch->setProperty("address", "77");
    connect(oddsSoloSwitch, SIGNAL(clicked()),
            this, SLOT(oddsToggleChanged()));
    oddsParameterLayout->addWidget(oddsSolo, 0, Qt::AlignLeft);

    QWidget *customSection = new QWidget;
    QVBoxLayout *customLayout = new QVBoxLayout(customSection);
    customLayout->setContentsMargins(0, 0, 0, 0);
    customLayout->setSpacing(7);
    QLabel *customTitle = new QLabel("CUSTOM");
    customTitle->setObjectName("ParameterSectionTitle");
    customLayout->addWidget(customTitle);
    customLayout->addWidget(createOddsCombo("Custom Type", "79"));
    customLayout->addWidget(createOddsBar("Bottom", "7A"));
    customLayout->addWidget(createOddsBar("Top", "7B"));
    customLayout->addWidget(createOddsBar("Low", "7C"));
    customLayout->addWidget(createOddsBar("High", "7D"));
    oddsCustomSection = customSection;
    oddsCustomSection->hide();
    oddsParameterLayout->addWidget(oddsCustomSection);
    oddsParameterLayout->addStretch(1);
    effectEditorStack->addWidget(oddsEditor);

    delayEditor = new EffectEditorPanel("DELAY");
    delayEditor->typeLabel()->hide();
    delayModelBrowser = new EffectModelBrowser;
    delayModelBrowser->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("DELAY")));
    delayEditor->setModelBrowserWidget(delayModelBrowser);
    connect(delayModelBrowser, &EffectModelBrowser::modelSelected,
            this, &modernFloorBoard::delayModelSelected);
    delayArtwork = new EffectArtworkWidget;
    delayArtwork->setArtwork(":/assets/effects/delay.png");
    QFont delayDisplayFont;
    delayDisplayFont.setFamily("Menlo");
    delayDisplayFont.setBold(true);
    delayDisplayFont.setStretch(QFont::Condensed);
    delayArtwork->setTextOverlay(
        "type", QRectF(0.255, 0.242, 0.49, 0.048), QString(),
        delayDisplayFont, QColor("#35D8FF"), Qt::AlignCenter, 0.50);
    delayEditor->setArtworkWidget(delayArtwork);

    QVBoxLayout *delayParameterLayout =
        new QVBoxLayout(delayEditor->parameterArea());
    delayParameterLayout->setContentsMargins(0, 0, 0, 0);
    delayParameterLayout->setSpacing(8);

    QWidget *delayPrimaryControls = new QWidget;
    QHBoxLayout *delayPrimaryLayout = new QHBoxLayout(delayPrimaryControls);
    delayPrimaryLayout->setContentsMargins(0, 0, 0, 0);
    delayPrimaryLayout->setSpacing(0);
    EffectToggleControl *delayToggle = new EffectToggleControl("State");
    delayOnOff = delayToggle->toggle();
    delayOnOff->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("DELAY")));
    delayOnOff->setProperty("address", "00");
    connect(delayOnOff, SIGNAL(clicked()), this, SLOT(delayToggleChanged()));
    delayPrimaryLayout->addWidget(delayToggle, 0, Qt::AlignTop);
    delayPrimaryLayout->addStretch(1);
    delayParameterLayout->addWidget(delayPrimaryControls);

    QWidget *delayTypeControl = createDelayCombo("Type", "01");
    delayTypeControl->setParent(delayEditor->parameterArea());
    delayTypeControl->hide();

    delayPageStack = new QStackedWidget;
    QWidget *delayStandardPage = new QWidget;
    QVBoxLayout *delayStandardLayout = new QVBoxLayout(delayStandardPage);
    delayStandardLayout->setContentsMargins(0, 0, 0, 0);
    delayStandardLayout->setSpacing(8);
    QLabel *delayCommonTitle = new QLabel("DELAY");
    delayCommonTitle->setObjectName("ParameterSectionTitle");
    delayStandardLayout->addWidget(delayCommonTitle);
    delayStandardLayout->addWidget(createDelayBar("Time", "02", true));
    delayStandardLayout->addWidget(createDelayBar("Feedback", "05"));
    delayStandardLayout->addWidget(createDelayCombo("High Cut", "06"));
    delayStandardLayout->addWidget(createDelayBar("Effect", "17"));
    delayStandardLayout->addWidget(createDelayBar("Direct", "18"));

    delayExtraStack = new QStackedWidget;
    delayExtraStack->addWidget(new QWidget);
    QWidget *delayPanSection = new QWidget;
    QVBoxLayout *delayPanLayout = new QVBoxLayout(delayPanSection);
    delayPanLayout->setContentsMargins(0, 0, 0, 0);
    delayPanLayout->setSpacing(7);
    QLabel *delayPanTitle = new QLabel("PAN");
    delayPanTitle->setObjectName("ParameterSectionTitle");
    delayPanLayout->addWidget(delayPanTitle);
    delayPanLayout->addWidget(createDelayBar("Tap Time", "04"));
    delayExtraStack->addWidget(delayPanSection);

    QWidget *delayWarpSection = new QWidget;
    QVBoxLayout *delayWarpLayout = new QVBoxLayout(delayWarpSection);
    delayWarpLayout->setContentsMargins(0, 0, 0, 0);
    delayWarpLayout->setSpacing(7);
    QLabel *delayWarpTitle = new QLabel("WARP");
    delayWarpTitle->setObjectName("ParameterSectionTitle");
    delayWarpLayout->addWidget(delayWarpTitle);
    EffectToggleControl *delayWarpToggle =
        new EffectToggleControl("Warp Switch");
    delayWarpSwitch = delayWarpToggle->toggle();
    delayWarpSwitch->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("DELAY")));
    delayWarpSwitch->setProperty("address", "11");
    connect(delayWarpSwitch, SIGNAL(clicked()),
            this, SLOT(delayToggleChanged()));
    delayWarpLayout->addWidget(delayWarpToggle, 0, Qt::AlignLeft);
    delayWarpLayout->addWidget(createDelayBar("Rise Time", "12"));
    delayWarpLayout->addWidget(createDelayBar("Feedback Depth", "13"));
    delayWarpLayout->addWidget(createDelayBar("Depth Level", "14"));
    delayExtraStack->addWidget(delayWarpSection);

    QWidget *delayModSection = new QWidget;
    QVBoxLayout *delayModLayout = new QVBoxLayout(delayModSection);
    delayModLayout->setContentsMargins(0, 0, 0, 0);
    delayModLayout->setSpacing(7);
    QLabel *delayModTitle = new QLabel("MODULATION");
    delayModTitle->setObjectName("ParameterSectionTitle");
    delayModLayout->addWidget(delayModTitle);
    delayModLayout->addWidget(createDelayBar("Rate", "15"));
    delayModLayout->addWidget(createDelayBar("Depth", "16"));
    delayExtraStack->addWidget(delayModSection);
    delayStandardLayout->addWidget(delayExtraStack);
    delayStandardLayout->addStretch(1);
    delayPageStack->addWidget(delayStandardPage);

    QWidget *delayDualPage = new QWidget;
    QVBoxLayout *delayDualLayout = new QVBoxLayout(delayDualPage);
    delayDualLayout->setContentsMargins(0, 0, 0, 0);
    delayDualLayout->setSpacing(8);
    ResponsiveSectionArea *delayDualArea = new ResponsiveSectionArea;
    QWidget *delayOneSection = new QWidget;
    QVBoxLayout *delayOneLayout = new QVBoxLayout(delayOneSection);
    delayOneLayout->setContentsMargins(0, 0, 0, 0);
    delayOneLayout->setSpacing(7);
    QLabel *delayOneTitle = new QLabel("DELAY 1");
    delayOneTitle->setObjectName("ParameterSectionTitle");
    delayOneLayout->addWidget(delayOneTitle);
    delayOneLayout->addWidget(createDelayBar("Time", "07", true));
    delayOneLayout->addWidget(createDelayBar("Feedback", "09"));
    delayOneLayout->addWidget(createDelayCombo("High Cut", "0A"));
    delayOneLayout->addWidget(createDelayBar("Effect", "0B"));

    QWidget *delayTwoSection = new QWidget;
    QVBoxLayout *delayTwoLayout = new QVBoxLayout(delayTwoSection);
    delayTwoLayout->setContentsMargins(0, 0, 0, 0);
    delayTwoLayout->setSpacing(7);
    QLabel *delayTwoTitle = new QLabel("DELAY 2");
    delayTwoTitle->setObjectName("ParameterSectionTitle");
    delayTwoLayout->addWidget(delayTwoTitle);
    delayTwoLayout->addWidget(createDelayBar("Time", "0C", true));
    delayTwoLayout->addWidget(createDelayBar("Feedback", "0E"));
    delayTwoLayout->addWidget(createDelayCombo("High Cut", "0F"));
    delayTwoLayout->addWidget(createDelayBar("Effect", "10"));
    delayDualArea->addSection(delayOneSection);
    delayDualArea->addSection(delayTwoSection);
    delayDualLayout->addWidget(delayDualArea);
    QLabel *delayDualCommonTitle = new QLabel("COMMON");
    delayDualCommonTitle->setObjectName("ParameterSectionTitle");
    delayDualLayout->addWidget(delayDualCommonTitle);
    delayDualLayout->addWidget(createDelayBar("Direct", "18"));
    delayDualLayout->addStretch(1);
    delayPageStack->addWidget(delayDualPage);
    delayParameterLayout->addWidget(delayPageStack);
    delayParameterLayout->addStretch(1);
    effectEditorStack->addWidget(delayEditor);

    mainLayout->addWidget(effectEditorStack, 1);

    BottomControlStrip *bottomControlStrip = new BottomControlStrip;
    bottomControlStrip->setFixedHeight(122);
    mainLayout->addWidget(bottomControlStrip);

    body->addWidget(mainArea, 1);

    root->addLayout(body, 1);

    backendDisconnected();
}

QWidget *modernFloorBoard::createReverbCombo(const QString &label,
                                              const QString &address)
{
    ParameterCombo *container = new ParameterCombo(label);
    QComboBox *combo = container->comboBox();
    combo->setProperty("address", address);

    const Midi parameter = MidiTable::Instance()->getMidiMap(
        "Structure", "0A", "00", address);
    QStringList labels;
    for (const Midi &item : parameter.level) {
        const QString text = item.desc.isEmpty() ? item.name : item.desc;
        combo->addItem(text);
        labels.append(text);
    }

    connect(combo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(reverbComboChanged(int)));
    if (address == "31") {
        reverbType = combo;
        if (reverbModelBrowser)
            reverbModelBrowser->setModels(labels);
    } else if (address == "34") reverbLowCut = combo;
    else if (address == "35") reverbHighCut = combo;
    return container;
}

QWidget *modernFloorBoard::createReverbBar(const QString &label,
                                            const QString &address,
                                            bool twoByte)
{
    ParameterBar *bar = new ParameterBar(label);
    bar->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("REVERB")));
    bar->setProperty("address", address);
    bar->setProperty("twoByte", twoByte);
    MidiTable *midiTable = MidiTable::Instance();
    const Midi parameter = midiTable->getMidiMap(
        "Structure", "0A", "00", address);
    bar->setRange(midiTable->getRangeMinimum(
                      "Structure", "0A", "00", address),
                  midiTable->getRange(
                      "Structure", "0A", "00", address));
    int rawCenter = 0;
    if (centerValueFromMapping(parameter, &rawCenter))
        bar->setCenterValue(rawCenter);
    connect(bar, &QAbstractSlider::valueChanged,
            this, &modernFloorBoard::reverbBarChanged);
    if (address == "39")
        reverbSpringSensitivity = bar;
    reverbBars.append(bar);
    return bar;
}

QWidget *modernFloorBoard::createCompCombo(const QString &label,
                                            const QString &address)
{
    ParameterCombo *container = new ParameterCombo(label);
    QComboBox *combo = container->comboBox();
    combo->setProperty("address", address);

    const Midi parameter = MidiTable::Instance()->getMidiMap(
        "Structure", "00", "00", address);
    QStringList labels;
    for (const Midi &item : parameter.level) {
        const QString text = item.desc.isEmpty() ? item.name : item.desc;
        combo->addItem(text);
        labels.append(text);
    }

    connect(combo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(compTypeChanged(int)));
    compType = combo;
    if (compModelBrowser)
        compModelBrowser->setModels(labels);
    return container;
}

QWidget *modernFloorBoard::createCompBar(const QString &label,
                                          const QString &address)
{
    ParameterBar *bar = new ParameterBar(label);
    bar->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("COMP")));
    bar->setProperty("address", address);
    MidiTable *midiTable = MidiTable::Instance();
    const Midi parameter = midiTable->getMidiMap(
        "Structure", "00", "00", address);
    bar->setRange(midiTable->getRangeMinimum(
                      "Structure", "00", "00", address),
                  midiTable->getRange(
                      "Structure", "00", "00", address));
    int rawCenter = 0;
    if (centerValueFromMapping(parameter, &rawCenter))
        bar->setCenterValue(rawCenter);
    connect(bar, &QAbstractSlider::valueChanged,
            this, &modernFloorBoard::compBarChanged);
    compBars.append(bar);
    return bar;
}

QWidget *modernFloorBoard::createOddsCombo(const QString &label,
                                            const QString &address)
{
    ParameterCombo *container = new ParameterCombo(label);
    QComboBox *combo = container->comboBox();
    combo->setProperty("address", address);

    const Midi parameter = MidiTable::Instance()->getMidiMap(
        "Structure", "00", "00", address);
    QStringList labels;
    for (const Midi &item : parameter.level) {
        const QString text = item.desc.isEmpty() ? item.name : item.desc;
        combo->addItem(text);
        labels.append(text);
    }

    connect(combo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(oddsComboChanged(int)));
    if (address == "71") {
        oddsType = combo;
        if (oddsModelBrowser)
            oddsModelBrowser->setModels(labels);
    } else if (address == "79") {
        oddsCustomType = combo;
    }
    return container;
}

QWidget *modernFloorBoard::createOddsBar(const QString &label,
                                          const QString &address)
{
    ParameterBar *bar = new ParameterBar(label);
    bar->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("OD/DS")));
    bar->setProperty("address", address);
    MidiTable *midiTable = MidiTable::Instance();
    const Midi parameter = midiTable->getMidiMap(
        "Structure", "00", "00", address);
    bar->setRange(midiTable->getRangeMinimum(
                      "Structure", "00", "00", address),
                  midiTable->getRange(
                      "Structure", "00", "00", address));
    int rawCenter = 0;
    if (centerValueFromMapping(parameter, &rawCenter))
        bar->setCenterValue(rawCenter);
    connect(bar, &QAbstractSlider::valueChanged,
            this, &modernFloorBoard::oddsBarChanged);
    oddsBars.append(bar);
    return bar;
}

QWidget *modernFloorBoard::createDelayCombo(const QString &label,
                                            const QString &address)
{
    ParameterCombo *container = new ParameterCombo(label);
    QComboBox *combo = container->comboBox();
    combo->setProperty("address", address);

    const Midi parameter = MidiTable::Instance()->getMidiMap(
        "Structure", "0A", "00", address);
    QStringList labels;
    for (const Midi &item : parameter.level) {
        const QString text = item.desc.isEmpty() ? item.name : item.desc;
        combo->addItem(text);
        labels.append(text);
    }

    connect(combo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(delayComboChanged(int)));
    delayCombos.append(combo);
    if (address == "01") {
        delayType = combo;
        if (delayModelBrowser)
            delayModelBrowser->setModels(labels);
    }
    return container;
}

QWidget *modernFloorBoard::createDelayBar(const QString &label,
                                          const QString &address,
                                          bool twoByte)
{
    ParameterBar *bar = new ParameterBar(label);
    bar->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("DELAY")));
    bar->setProperty("address", address);
    bar->setProperty("twoByte", twoByte);
    MidiTable *midiTable = MidiTable::Instance();
    const Midi parameter = midiTable->getMidiMap(
        "Structure", "0A", "00", address);
    bar->setRange(midiTable->getRangeMinimum(
                      "Structure", "0A", "00", address),
                  midiTable->getRange(
                      "Structure", "0A", "00", address));
    if (twoByte) {
        const QVector<int> rhythmicRawValues =
            rhythmicDivisionRawValues(parameter);
        if (!rhythmicRawValues.isEmpty()) {
            const int firstRhythmicRaw = *std::min_element(
                rhythmicRawValues.constBegin(),
                rhythmicRawValues.constEnd());
            bar->setSegmentedMapping(
                firstRhythmicRaw - 1,
                rhythmicRawValues, 0.5);
        }
    }
    int rawCenter = 0;
    if (centerValueFromMapping(parameter, &rawCenter))
        bar->setCenterValue(rawCenter);
    connect(bar, &QAbstractSlider::valueChanged,
            this, &modernFloorBoard::delayBarChanged);
    delayBars.append(bar);
    return bar;
}

EffectModule *modernFloorBoard::createEffectBlock(const QString &name,
                                                   bool available)
{
    const EffectModule::VisualKind kind = name == "EQ"
        ? EffectModule::Equalizer : EffectModule::DualKnob;
    return new EffectModule(name, ModernTheme::effectColor(name), available, kind);
}


bool modernFloorBoard::hasValidReverbBuffer() const
{
    SysxIO *sysxIO = SysxIO::Instance();
    const SysxData source = sysxIO->getFileSource();
    const int addressIndex = source.address.indexOf("0A00");

    if (!backendIsConnected || !backendHasPatchData
        || !sysxIO->isConnected() || addressIndex < 0)
        return false;

    const int valueIndex = sysxDataOffset + 0x3B;
    return addressIndex < source.hex.size()
        && valueIndex >= 0
        && valueIndex < source.hex.at(addressIndex).size();
}

bool modernFloorBoard::hasValidCompBuffer() const
{
    SysxIO *sysxIO = SysxIO::Instance();
    const SysxData source = sysxIO->getFileSource();
    const int addressIndex = source.address.indexOf("0000");

    if (!backendIsConnected || !backendHasPatchData
        || !sysxIO->isConnected() || addressIndex < 0)
        return false;

    const int valueIndex = sysxDataOffset + 0x47;
    return addressIndex < source.hex.size()
        && valueIndex >= 0
        && valueIndex < source.hex.at(addressIndex).size();
}

bool modernFloorBoard::hasValidOddsBuffer() const
{
    SysxIO *sysxIO = SysxIO::Instance();
    const SysxData source = sysxIO->getFileSource();
    const int addressIndex = source.address.indexOf("0000");

    if (!backendIsConnected || !backendHasPatchData
        || !sysxIO->isConnected() || addressIndex < 0)
        return false;

    const int valueIndex = sysxDataOffset + 0x7D;
    return addressIndex < source.hex.size()
        && valueIndex >= 0
        && valueIndex < source.hex.at(addressIndex).size();
}

bool modernFloorBoard::hasValidDelayBuffer() const
{
    SysxIO *sysxIO = SysxIO::Instance();
    const SysxData source = sysxIO->getFileSource();
    const int addressIndex = source.address.indexOf("0A00");

    if (!backendIsConnected || !backendHasPatchData
        || !sysxIO->isConnected() || addressIndex < 0)
        return false;

    const int valueIndex = sysxDataOffset + 0x18;
    return addressIndex < source.hex.size()
        && valueIndex >= 0
        && valueIndex < source.hex.at(addressIndex).size();
}

void modernFloorBoard::setReverbUnavailable()
{
    if (reverbCard)
        reverbCard->setEffectState(false, false);
    updateReverbParameterControls(false);
}

void modernFloorBoard::setCompUnavailable()
{
    if (compCard)
        compCard->setEffectState(false, false);
    updateCompParameterControls(false);
}

void modernFloorBoard::setOddsUnavailable()
{
    if (oddsCard)
        oddsCard->setEffectState(false, false);
    updateOddsParameterControls(false);
}

void modernFloorBoard::setDelayUnavailable()
{
    if (delayCard)
        delayCard->setEffectState(false, false);
    updateDelayParameterControls(false);
}

void modernFloorBoard::backendConnected()
{
    backendIsConnected = true;
    backendHasPatchData = false;
    connectionStatus->setConnected(true);
    setReverbUnavailable();
    setCompUnavailable();
    setOddsUnavailable();
    setDelayUnavailable();
}

void modernFloorBoard::backendDisconnected()
{
    backendIsConnected = false;
    backendHasPatchData = false;
    connectionStatus->setConnected(false);
    signalChainModel.clear();
    rebuildSignalChainView();
    setReverbUnavailable();
    setCompUnavailable();
    setOddsUnavailable();
    setDelayUnavailable();
    patchNumber->setText(QString::fromUtf8("—"));
    patchName->setText("NO PATCH DATA");
    patchListModel.setCurrentPatch(0, 0, QString());
}

void modernFloorBoard::patchNameResolved(int bank, int patch, QString name)
{
    patchListModel.setPatchName(bank, patch, name);
}

void modernFloorBoard::refreshReverbState()
{
    SysxIO *sysxIO = SysxIO::Instance();
    if (backendIsConnected && sysxIO->isConnected() && sysxIO->isDevice())
        backendHasPatchData = true;

    if (backendHasPatchData) {
        const int bank = sysxIO->getLoadedBank();
        const int patch = sysxIO->getLoadedPatch();
        const QString name = sysxIO->getCurrentPatchName().trimmed();
        patchNumber->setText(patchListModel.patchNumber(bank, patch));
        patchName->setText(name.isEmpty() ? QString::fromUtf8("—") : name);
        patchListModel.setCurrentPatch(bank, patch, name);
    }

    refreshSignalChainModel();

    if (!hasValidReverbBuffer()) {
        setReverbUnavailable();
        refreshCompState();
        refreshOddsState();
        refreshDelayState();
        return;
    }

    const int value = sysxIO->getSourceValue(
        "Structure", "0A", "00", "30"
    );
    const bool on = (value == 1);

    reverbCard->setEffectState(true, on);
    updateReverbParameterControls(true);
    refreshCompState();
    refreshOddsState();
    refreshDelayState();
}

void modernFloorBoard::refreshCompState()
{
    if (!hasValidCompBuffer()) {
        setCompUnavailable();
        return;
    }

    const bool on = SysxIO::Instance()->getSourceValue(
        "Structure", "00", "00", "40") == 1;
    if (compCard)
        compCard->setEffectState(true, on);
    updateCompParameterControls(true);
}

void modernFloorBoard::refreshOddsState()
{
    if (!hasValidOddsBuffer()) {
        setOddsUnavailable();
        return;
    }

    const bool on = SysxIO::Instance()->getSourceValue(
        "Structure", "00", "00", "70") == 1;
    if (oddsCard)
        oddsCard->setEffectState(true, on);
    updateOddsParameterControls(true);
}

void modernFloorBoard::refreshDelayState()
{
    if (!hasValidDelayBuffer()) {
        setDelayUnavailable();
        return;
    }

    const bool on = SysxIO::Instance()->getSourceValue(
        "Structure", "0A", "00", "00") == 1;
    if (delayCard)
        delayCard->setEffectState(true, on);
    updateDelayParameterControls(true);
}

void modernFloorBoard::refreshSignalChainModel()
{
    if (!backendIsConnected || !backendHasPatchData) {
        signalChainModel.clear();
        rebuildSignalChainView();
        return;
    }

    signalChainModel.refreshFromLegacyBackend();
    signalChainModel.logInterpretedChain();
    rebuildSignalChainView();
}

SignalChainModule *modernFloorBoard::createSignalChainModule(
    const modernSignalChainModel::Entry &entry)
{
    const QString fullName = modernSignalChainModel::displayName(entry);
    QString name = fullName;
    if (name == "SEND/RETURN") name = "S/R";
    else if (name == "FOOT VOLUME") name = "FV";
    else if (name == "PEDAL FX") name = "P.FX";
    else if (name == "DIGITAL OUT") name = "D.OUT";
    else if (name == "NS-1") name = "NS1";
    else if (name == "NS-2") name = "NS2";
    const bool isReverb = entry.moduleId == 0x09;
    const bool isComp = entry.moduleId == 0x00;
    const bool isOdds = entry.moduleId == 0x01;
    const bool isDelay = entry.moduleId == 0x07;
    SignalChainModule *module = new SignalChainModule(
        name, QColor(ModernTheme::effectColor(fullName)),
        QColor(ModernTheme::effectFaceColor(fullName)));
    module->setEffectState(false, false);
    module->setNavigable(isComp || isReverb || isOdds || isDelay);
    module->setProperty("chainPosition", entry.originalPosition);
    module->setProperty("rawValue", entry.rawValue);
    module->setProperty("signalPath", entry.path);
    signalChainModules.append(module);

    if (isReverb) {
        reverbCard = module;
        reverbCard->setSelected(selectedEditor == "REVERB");
        connect(module, SIGNAL(clicked()), this, SLOT(showReverbEditor()));
    }
    if (isComp) {
        compCard = module;
        compCard->setSelected(selectedEditor == "COMP");
        connect(module, SIGNAL(clicked()), this, SLOT(showCompEditor()));
    }
    if (isOdds) {
        oddsCard = module;
        oddsCard->setSelected(selectedEditor == "OD/DS");
        connect(module, SIGNAL(clicked()), this, SLOT(showOddsEditor()));
    }
    if (isDelay) {
        delayCard = module;
        delayCard->setSelected(selectedEditor == "DELAY");
        connect(module, SIGNAL(clicked()), this, SLOT(showDelayEditor()));
    }
    return module;
}

void modernFloorBoard::rebuildSignalChainView()
{
    reverbCard = nullptr;
    compCard = nullptr;
    delayCard = nullptr;
    signalChainModules.clear();
    signalChainJunctions.clear();
    signalChainConnectors.clear();
    signalFlowLayout = nullptr;
    signalPathsLayout = nullptr;
    signalPathALabel = nullptr;
    signalPathBLabel = nullptr;

    QWidget *content = new QWidget;
    content->setObjectName("SignalChainContent");
    content->setStyleSheet("QWidget#SignalChainContent{background:transparent;}");
    signalFlowLayout = new QHBoxLayout(content);
    signalFlowLayout->setContentsMargins(4, 0, 4, 0);
    signalFlowLayout->setSpacing(5);
    SignalConnector *inputConnector = new SignalConnector(SignalConnector::Input);
    signalChainConnectors.append(inputConnector);
    signalFlowLayout->addWidget(inputConnector);

    if (!signalChainModel.isValid()) {
        QLabel *placeholder = new QLabel("Load a GT-10 patch to display its real signal chain");
        placeholder->setAlignment(Qt::AlignCenter);
        placeholder->setStyleSheet(QString(
            "color:%1;font-size:12px;padding:30px;")
            .arg(ModernTheme::color(ModernTheme::DisabledText)));
        signalFlowLayout->addWidget(placeholder, 1);
        SignalConnector *outputConnector = new SignalConnector(SignalConnector::Output);
        signalChainConnectors.append(outputConnector);
        signalFlowLayout->addWidget(outputConnector);
        signalChainScroll->setWidget(content);
        return;
    }

    for (const modernSignalChainModel::Entry &entry : signalChainModel.commonPrefix())
        signalFlowLayout->addWidget(createSignalChainModule(entry));

    SignalJunction *split = new SignalJunction(SignalJunction::Split);
    signalChainJunctions.append(split);
    signalFlowLayout->addWidget(split);

    QWidget *parallelPaths = new QWidget;
    parallelPaths->setObjectName("ParallelPaths");
    parallelPaths->setStyleSheet(QString(
        "QWidget#ParallelPaths{background:%1;border:1px solid %2;"
        "border-radius:4px;}")
        .arg(ModernTheme::color(ModernTheme::ControlBackground),
             ModernTheme::color(ModernTheme::Border)));
    signalPathsLayout = new QGridLayout(parallelPaths);
    signalPathsLayout->setContentsMargins(5, 5, 5, 5);
    signalPathsLayout->setHorizontalSpacing(5);
    signalPathsLayout->setVerticalSpacing(4);
    signalPathALabel = new QLabel("A");
    signalPathBLabel = new QLabel("B");
    const QString pathLabelStyle = QString(
        "color:%1;font-size:11px;font-weight:700;background:transparent;")
        .arg(ModernTheme::color(ModernTheme::AccentCyanDim));
    signalPathALabel->setStyleSheet(pathLabelStyle);
    signalPathBLabel->setStyleSheet(pathLabelStyle);
    signalPathALabel->setAlignment(Qt::AlignCenter);
    signalPathBLabel->setAlignment(Qt::AlignCenter);
    signalPathsLayout->addWidget(signalPathALabel, 0, 0);
    signalPathsLayout->addWidget(signalPathBLabel, 1, 0);

    int column = 1;
    for (const modernSignalChainModel::Entry &entry : signalChainModel.pathA())
        signalPathsLayout->addWidget(createSignalChainModule(entry), 0, column++);
    if (column == 1) {
        QLabel *empty = new QLabel("EMPTY PATH");
        empty->setStyleSheet(QString(
            "color:%1;font-size:9px;padding:20px;")
            .arg(ModernTheme::color(ModernTheme::DisabledText)));
        signalPathsLayout->addWidget(empty, 0, column);
    }

    column = 1;
    for (const modernSignalChainModel::Entry &entry : signalChainModel.pathB())
        signalPathsLayout->addWidget(createSignalChainModule(entry), 1, column++);
    if (column == 1) {
        QLabel *empty = new QLabel("EMPTY PATH");
        empty->setStyleSheet(QString(
            "color:%1;font-size:9px;padding:20px;")
            .arg(ModernTheme::color(ModernTheme::DisabledText)));
        signalPathsLayout->addWidget(empty, 1, column);
    }
    signalFlowLayout->addWidget(parallelPaths);
    SignalJunction *merge = new SignalJunction(SignalJunction::Merge);
    signalChainJunctions.append(merge);
    signalFlowLayout->addWidget(merge);

    for (const modernSignalChainModel::Entry &entry : signalChainModel.commonSuffix())
        signalFlowLayout->addWidget(createSignalChainModule(entry));
    SignalConnector *outputConnector = new SignalConnector(SignalConnector::Output);
    signalChainConnectors.append(outputConnector);
    signalFlowLayout->addWidget(outputConnector);

    signalChainScroll->setWidget(content);
    QTimer::singleShot(0, this, [this]() { applyResponsiveSignalChainLayout(); });
}

void modernFloorBoard::applyResponsiveSignalChainLayout()
{
    if (!signalChainModel.isValid() || !signalChainScroll || !signalFlowLayout)
        return;

    const int available = signalChainScroll->viewport()->width();
    if (available <= 0)
        return;

    const int slotCount = signalChainModel.commonPrefix().size()
        + qMax(signalChainModel.pathA().size(), signalChainModel.pathB().size())
        + signalChainModel.commonSuffix().size();
    if (slotCount <= 0)
        return;

    const int gap = qBound(2, available / 240, 6);
    const int connectorWidth = qBound(24, available / 44, 32);
    const int junctionWidth = qBound(18, available / 55, 28);
    const int pathLabelWidth = qBound(13, available / 80, 18);
    const int outerItemCount = signalChainModel.commonPrefix().size()
        + signalChainModel.commonSuffix().size() + 5;
    const int totalGapWidth = qMax(0, outerItemCount - 1) * gap
        + qMax(signalChainModel.pathA().size(), signalChainModel.pathB().size()) * gap;
    const int fixedWidth = connectorWidth * 2 + junctionWidth * 2
        + pathLabelWidth + 16 + totalGapWidth;
    const int moduleWidth = qBound(52, (available - fixedWidth) / slotCount, 96);

    signalFlowLayout->setContentsMargins(2, 0, 2, 0);
    signalFlowLayout->setSpacing(gap);
    if (signalPathsLayout) {
        const int pathMargin = moduleWidth < 66 ? 2 : 4;
        signalPathsLayout->setContentsMargins(pathMargin, 4, pathMargin, 4);
        signalPathsLayout->setHorizontalSpacing(gap);
    }
    if (signalPathALabel) signalPathALabel->setFixedWidth(pathLabelWidth);
    if (signalPathBLabel) signalPathBLabel->setFixedWidth(pathLabelWidth);
    for (SignalChainModule *module : signalChainModules)
        module->setCompactWidth(moduleWidth);
    for (SignalJunction *junction : signalChainJunctions)
        junction->setCompactWidth(junctionWidth);
    for (SignalConnector *connector : signalChainConnectors)
        connector->setCompactWidth(connectorWidth);
}

void modernFloorBoard::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    applyResponsiveSignalChainLayout();
}

bool modernFloorBoard::eventFilter(QObject *watched, QEvent *event)
{
    if (signalChainScroll
        && watched == signalChainScroll->viewport()
        && event->type() == QEvent::Resize) {
        QTimer::singleShot(0, this, [this]() {
            applyResponsiveSignalChainLayout();
        });
    }

    return QWidget::eventFilter(watched, event);
}

void modernFloorBoard::updateReverbParameterControls(bool available)
{
    if (reverbModelBrowser)
        reverbModelBrowser->setEnabled(available);

    const QList<QComboBox *> combos = {reverbType, reverbLowCut, reverbHighCut};

    for (QComboBox *combo : combos)
        if (combo) combo->setEnabled(available);
    if (reverbOnOff) {
        reverbOnOff->setEnabled(available);
        reverbOnOff->setVisible(available);
        if (!available)
            reverbOnOff->setChecked(false);
    }
    for (ParameterBar *bar : reverbBars) {
        if (!bar)
            continue;
        bar->setEnabled(available);
        if (!available)
            bar->setDisplayText(QString::fromUtf8("—"));
    }

    if (!available)
    {
        for (QComboBox *combo : combos) {
            if (!combo)
                continue;
            const QSignalBlocker blocker(combo);
            combo->setCurrentIndex(-1);
        }
        if (reverbTypeDisplay) reverbTypeDisplay->setText(QString::fromUtf8("—"));
        if (reverbArtwork) reverbArtwork->setTextOverlayText("type", QString());
        if (reverbModelBrowser)
            reverbModelBrowser->setCurrentIndex(-1);
        return;
    }

    SysxIO *sysxIO = SysxIO::Instance();
    MidiTable *midiTable = MidiTable::Instance();
    if (reverbOnOff)
        reverbOnOff->setChecked(sysxIO->getSourceValue(
            "Structure", "0A", "00", "30") == 1);
    for (QComboBox *combo : combos) {
        if (!combo) continue;
        const QString address = combo->property("address").toString();
        const QSignalBlocker blocker(combo);
        combo->setCurrentIndex(sysxIO->getSourceValue(
            "Structure", "0A", "00", address));
    }

    if (reverbTypeDisplay && reverbType) {
        reverbTypeDisplay->setText(reverbType->currentText());
    }
    if (reverbArtwork && reverbType)
        reverbArtwork->setTextOverlayText(
            "type", reverbType->currentText().toUpper());
    if (reverbModelBrowser && reverbType)
        reverbModelBrowser->setCurrentIndex(reverbType->currentIndex());

    for (ParameterBar *bar : reverbBars) {
        if (!bar)
            continue;
        const QString address = bar->property("address").toString();
        const int value = sysxIO->getSourceValue(
            "Structure", "0A", "00", address);
        const QSignalBlocker blocker(bar);
        bar->setValue(value);
        bar->setDisplayText(midiTable->getValue(
            "Structure", "0A", "00", address,
            QString::number(value, 16).toUpper()));
    }

    if (reverbSpringSensitivity && reverbType)
        reverbSpringSensitivity->setEnabled(
            reverbType->currentIndex() == 5);
}

void modernFloorBoard::setReverbValue(const QString &address,
                                      int value,
                                      bool twoByte)
{
    if (!hasValidReverbBuffer())
        return;

    SysxIO *sysxIO = SysxIO::Instance();
    if (twoByte) {
        const QString high = QString("%1").arg(value / 128, 2, 16, QChar('0')).toUpper();
        const QString low = QString("%1").arg(value % 128, 2, 16, QChar('0')).toUpper();
        sysxIO->setFileSource("Structure", "0A", "00", address, high, low);
    } else {
        const QString hex = QString("%1").arg(value, 2, 16, QChar('0')).toUpper();
        sysxIO->setFileSource("Structure", "0A", "00", address, hex);
    }
}

void modernFloorBoard::reverbComboChanged(int value)
{
    QComboBox *combo = qobject_cast<QComboBox *>(sender());
    if (!combo)
        return;
    if (combo == reverbType) {
        setReverbType(value);
        return;
    }
    setReverbValue(combo->property("address").toString(), value, false);
}

void modernFloorBoard::setReverbType(int index)
{
    if (!reverbType || index < 0 || index >= reverbType->count()
        || !hasValidReverbBuffer())
        return;

    {
        const QSignalBlocker blocker(reverbType);
        reverbType->setCurrentIndex(index);
    }
    setReverbValue("31", index, false);
    if (reverbModelBrowser)
        reverbModelBrowser->setCurrentIndex(index);
    if (reverbTypeDisplay)
        reverbTypeDisplay->setText(reverbType->itemText(index));
    if (reverbArtwork)
        reverbArtwork->setTextOverlayText(
            "type", reverbType->itemText(index).toUpper());
    if (reverbSpringSensitivity)
        reverbSpringSensitivity->setEnabled(index == 5);
}

void modernFloorBoard::reverbModelSelected(int index)
{
    setReverbType(index);
}

void modernFloorBoard::reverbBarChanged(int value)
{
    ParameterBar *bar = qobject_cast<ParameterBar *>(sender());
    if (!bar)
        return;

    const QString address = bar->property("address").toString();
    setReverbValue(address, value, bar->property("twoByte").toBool());
    const QString display = MidiTable::Instance()->getValue(
        "Structure", "0A", "00", address,
        QString::number(value, 16).toUpper());
    for (ParameterBar *peer : reverbBars) {
        if (!peer || peer->property("address").toString() != address)
            continue;
        const QSignalBlocker blocker(peer);
        peer->setValue(value);
        peer->setDisplayText(display);
    }
}

void modernFloorBoard::showCompEditor()
{
    selectedEditor = "COMP";
    if (effectEditorStack && compEditor)
        effectEditorStack->setCurrentWidget(compEditor);
    if (compCard)
        compCard->setSelected(true);
    if (reverbCard)
        reverbCard->setSelected(false);
    if (oddsCard)
        oddsCard->setSelected(false);
    if (delayCard)
        delayCard->setSelected(false);
}

void modernFloorBoard::showReverbEditor()
{
    selectedEditor = "REVERB";
    if (effectEditorStack && reverbEditor)
        effectEditorStack->setCurrentWidget(reverbEditor);
    if (reverbCard)
        reverbCard->setSelected(true);
    if (compCard)
        compCard->setSelected(false);
    if (oddsCard)
        oddsCard->setSelected(false);
    if (delayCard)
        delayCard->setSelected(false);
}

void modernFloorBoard::showOddsEditor()
{
    selectedEditor = "OD/DS";
    if (effectEditorStack && oddsEditor)
        effectEditorStack->setCurrentWidget(oddsEditor);
    if (oddsCard)
        oddsCard->setSelected(true);
    if (compCard)
        compCard->setSelected(false);
    if (reverbCard)
        reverbCard->setSelected(false);
    if (delayCard)
        delayCard->setSelected(false);
}

void modernFloorBoard::showDelayEditor()
{
    selectedEditor = "DELAY";
    if (effectEditorStack && delayEditor)
        effectEditorStack->setCurrentWidget(delayEditor);
    if (delayCard)
        delayCard->setSelected(true);
    if (compCard)
        compCard->setSelected(false);
    if (reverbCard)
        reverbCard->setSelected(false);
    if (oddsCard)
        oddsCard->setSelected(false);
}

void modernFloorBoard::updateCompParameterControls(bool available)
{
    if (compModelBrowser)
        compModelBrowser->setEnabled(available);
    if (compType)
        compType->setEnabled(available);
    if (compOnOff) {
        compOnOff->setEnabled(available);
        compOnOff->setVisible(available);
        if (!available)
            compOnOff->setChecked(false);
    }
    for (ParameterBar *bar : compBars) {
        if (!bar)
            continue;
        bar->setEnabled(available);
        if (!available)
            bar->setDisplayText(QString::fromUtf8("—"));
    }

    if (!available) {
        if (compType) {
            const QSignalBlocker blocker(compType);
            compType->setCurrentIndex(-1);
        }
        if (compTypeDisplay)
            compTypeDisplay->setText(QString::fromUtf8("—"));
        if (compModelBrowser)
            compModelBrowser->setCurrentIndex(-1);
        return;
    }

    SysxIO *sysxIO = SysxIO::Instance();
    MidiTable *midiTable = MidiTable::Instance();
    const int type = sysxIO->getSourceValue(
        "Structure", "00", "00", "41");
    if (compType) {
        const QSignalBlocker blocker(compType);
        compType->setCurrentIndex(type);
    }
    if (compModeStack)
        compModeStack->setCurrentIndex(type == 1 ? 1 : 0);
    if (compModelBrowser)
        compModelBrowser->setCurrentIndex(type);
    if (compTypeDisplay && compType)
        compTypeDisplay->setText(compType->currentText());
    if (compOnOff)
        compOnOff->setChecked(sysxIO->getSourceValue(
            "Structure", "00", "00", "40") == 1);

    for (ParameterBar *bar : compBars) {
        if (!bar)
            continue;
        const QString address = bar->property("address").toString();
        const int value = sysxIO->getSourceValue(
            "Structure", "00", "00", address);
        const QSignalBlocker blocker(bar);
        bar->setValue(value);
        bar->setDisplayText(midiTable->getValue(
            "Structure", "00", "00", address,
            QString::number(value, 16).toUpper()));
    }
}

void modernFloorBoard::setCompValue(const QString &address, int value)
{
    if (!hasValidCompBuffer())
        return;
    SysxIO::Instance()->setFileSource(
        "Structure", "00", "00", address,
        QString("%1").arg(value, 2, 16, QChar('0')).toUpper());
}

void modernFloorBoard::setCompType(int index)
{
    if (!compType || index < 0 || index >= compType->count()
        || !hasValidCompBuffer())
        return;

    {
        const QSignalBlocker blocker(compType);
        compType->setCurrentIndex(index);
    }
    setCompValue(compType->property("address").toString(), index);
    if (compModelBrowser)
        compModelBrowser->setCurrentIndex(index);
    if (compModeStack)
        compModeStack->setCurrentIndex(index == 1 ? 1 : 0);
    if (compTypeDisplay)
        compTypeDisplay->setText(compType->itemText(index));
}

void modernFloorBoard::compTypeChanged(int value)
{
    if (!compType || sender() != compType)
        return;
    setCompType(value);
}

void modernFloorBoard::compModelSelected(int index)
{
    setCompType(index);
}

void modernFloorBoard::compBarChanged(int value)
{
    ParameterBar *bar = qobject_cast<ParameterBar *>(sender());
    if (!bar)
        return;
    const QString address = bar->property("address").toString();
    setCompValue(address, value);
    const QString display = MidiTable::Instance()->getValue(
        "Structure", "00", "00", address,
        QString::number(value, 16).toUpper());
    for (ParameterBar *peer : compBars) {
        if (!peer || peer->property("address").toString() != address)
            continue;
        const QSignalBlocker blocker(peer);
        peer->setValue(value);
        peer->setDisplayText(display);
    }
}

void modernFloorBoard::updateOddsParameterControls(bool available)
{
    if (oddsModelBrowser)
        oddsModelBrowser->setEnabled(available);

    const QList<QComboBox *> combos = {oddsType, oddsCustomType};
    for (QComboBox *combo : combos) {
        if (combo)
            combo->setEnabled(available);
    }

    const QList<ModernToggleSwitch *> toggles = {
        oddsOnOff, oddsSoloSwitch
    };
    for (ModernToggleSwitch *toggle : toggles) {
        if (!toggle)
            continue;
        toggle->setEnabled(available);
        toggle->setVisible(available);
        if (!available)
            toggle->setChecked(false);
    }

    for (ParameterBar *bar : oddsBars) {
        if (!bar)
            continue;
        bar->setEnabled(available);
        if (!available)
            bar->setDisplayText(QString::fromUtf8("—"));
    }

    if (!available) {
        for (QComboBox *combo : combos) {
            if (!combo)
                continue;
            const QSignalBlocker blocker(combo);
            combo->setCurrentIndex(-1);
        }
        if (oddsArtwork)
            oddsArtwork->setTextOverlayText("type", QString());
        if (oddsModelBrowser)
            oddsModelBrowser->setCurrentIndex(-1);
        if (oddsCustomSection)
            oddsCustomSection->hide();
        return;
    }

    SysxIO *sysxIO = SysxIO::Instance();
    MidiTable *midiTable = MidiTable::Instance();
    for (QComboBox *combo : combos) {
        if (!combo)
            continue;
        const QString address = combo->property("address").toString();
        const QSignalBlocker blocker(combo);
        combo->setCurrentIndex(sysxIO->getSourceValue(
            "Structure", "00", "00", address));
    }

    if (oddsOnOff)
        oddsOnOff->setChecked(sysxIO->getSourceValue(
            "Structure", "00", "00", "70") == 1);
    if (oddsSoloSwitch)
        oddsSoloSwitch->setChecked(sysxIO->getSourceValue(
            "Structure", "00", "00", "77") == 1);
    if (oddsCustomSection && oddsType)
        oddsCustomSection->setVisible(oddsType->currentIndex() == 0x19);
    if (oddsModelBrowser && oddsType)
        oddsModelBrowser->setCurrentIndex(oddsType->currentIndex());
    if (oddsArtwork && oddsType)
        oddsArtwork->setTextOverlayText(
            "type", oddsArtworkType(oddsType->currentText()));

    for (ParameterBar *bar : oddsBars) {
        if (!bar)
            continue;
        const QString address = bar->property("address").toString();
        const int value = sysxIO->getSourceValue(
            "Structure", "00", "00", address);
        const QSignalBlocker blocker(bar);
        bar->setValue(value);
        bar->setDisplayText(midiTable->getValue(
            "Structure", "00", "00", address,
            QString::number(value, 16).toUpper()));
    }
}

void modernFloorBoard::setOddsValue(const QString &address, int value)
{
    if (!hasValidOddsBuffer())
        return;
    SysxIO::Instance()->setFileSource(
        "Structure", "00", "00", address,
        QString("%1").arg(value, 2, 16, QChar('0')).toUpper());
}

void modernFloorBoard::setOddsType(int index)
{
    if (!oddsType || index < 0 || index >= oddsType->count()
        || !hasValidOddsBuffer())
        return;

    {
        const QSignalBlocker blocker(oddsType);
        oddsType->setCurrentIndex(index);
    }

    setOddsValue(oddsType->property("address").toString(), index);
    if (oddsModelBrowser)
        oddsModelBrowser->setCurrentIndex(index);
    if (oddsCustomSection)
        oddsCustomSection->setVisible(index == 0x19);
    if (oddsArtwork)
        oddsArtwork->setTextOverlayText(
            "type", oddsArtworkType(oddsType->itemText(index)));
}

void modernFloorBoard::oddsComboChanged(int value)
{
    QComboBox *combo = qobject_cast<QComboBox *>(sender());
    if (!combo)
        return;
    if (combo == oddsType) {
        setOddsType(value);
        return;
    }
    setOddsValue(combo->property("address").toString(), value);
}

void modernFloorBoard::oddsModelSelected(int index)
{
    setOddsType(index);
}

void modernFloorBoard::oddsBarChanged(int value)
{
    ParameterBar *bar = qobject_cast<ParameterBar *>(sender());
    if (!bar)
        return;
    const QString address = bar->property("address").toString();
    setOddsValue(address, value);
    const QString display = MidiTable::Instance()->getValue(
        "Structure", "00", "00", address,
        QString::number(value, 16).toUpper());
    for (ParameterBar *peer : oddsBars) {
        if (!peer || peer->property("address").toString() != address)
            continue;
        const QSignalBlocker blocker(peer);
        peer->setValue(value);
        peer->setDisplayText(display);
    }
}

void modernFloorBoard::oddsToggleChanged()
{
    QObject *toggle = sender();
    if (!toggle || !hasValidOddsBuffer())
        return;
    const QString address = toggle->property("address").toString();
    const bool newState = SysxIO::Instance()->getSourceValue(
        "Structure", "00", "00", address) != 1;
    setOddsValue(address, newState ? 1 : 0);
    refreshOddsState();
}

void modernFloorBoard::updateDelayPageForType(int type)
{
    const bool dual = type >= 3 && type <= 5;
    if (delayPageStack)
        delayPageStack->setCurrentIndex(dual ? 1 : 0);

    int extraPage = 0;
    if (type == 1)
        extraPage = 1;
    else if (type == 9)
        extraPage = 2;
    else if (type == 10)
        extraPage = 3;
    if (delayExtraStack) {
        delayExtraStack->setCurrentIndex(extraPage);
        delayExtraStack->setVisible(extraPage != 0);
    }
}

void modernFloorBoard::updateDelayParameterControls(bool available)
{
    if (delayModelBrowser)
        delayModelBrowser->setEnabled(available);

    for (QComboBox *combo : delayCombos) {
        if (combo)
            combo->setEnabled(available);
    }

    const QList<ModernToggleSwitch *> toggles = {
        delayOnOff, delayWarpSwitch
    };
    for (ModernToggleSwitch *toggle : toggles) {
        if (!toggle)
            continue;
        toggle->setEnabled(available);
        toggle->setVisible(available);
        if (!available)
            toggle->setChecked(false);
    }

    for (ParameterBar *bar : delayBars) {
        if (!bar)
            continue;
        bar->setEnabled(available);
        if (!available)
            bar->setDisplayText(QString::fromUtf8("—"));
    }

    if (!available) {
        for (QComboBox *combo : delayCombos) {
            if (!combo)
                continue;
            const QSignalBlocker blocker(combo);
            combo->setCurrentIndex(-1);
        }
        if (delayArtwork)
            delayArtwork->setTextOverlayText("type", QString());
        if (delayModelBrowser)
            delayModelBrowser->setCurrentIndex(-1);
        updateDelayPageForType(-1);
        return;
    }

    SysxIO *sysxIO = SysxIO::Instance();
    MidiTable *midiTable = MidiTable::Instance();
    for (QComboBox *combo : delayCombos) {
        if (!combo)
            continue;
        const QString address = combo->property("address").toString();
        const QSignalBlocker blocker(combo);
        combo->setCurrentIndex(sysxIO->getSourceValue(
            "Structure", "0A", "00", address));
    }

    const int type = delayType ? delayType->currentIndex() : -1;
    updateDelayPageForType(type);
    if (delayArtwork && delayType)
        delayArtwork->setTextOverlayText(
            "type", delayArtworkType(delayType->currentText()));
    if (delayModelBrowser && delayType)
        delayModelBrowser->setCurrentIndex(delayType->currentIndex());
    if (delayOnOff)
        delayOnOff->setChecked(sysxIO->getSourceValue(
            "Structure", "0A", "00", "00") == 1);
    if (delayWarpSwitch)
        delayWarpSwitch->setChecked(sysxIO->getSourceValue(
            "Structure", "0A", "00", "11") == 1);

    for (ParameterBar *bar : delayBars) {
        if (!bar)
            continue;
        const QString address = bar->property("address").toString();
        const int value = sysxIO->getSourceValue(
            "Structure", "0A", "00", address);
        const QSignalBlocker blocker(bar);
        bar->setValue(value);
        bar->setDisplayText(formatRhythmicDivision(
            midiTable->getValue(
                "Structure", "0A", "00", address,
                QString::number(value, 16).toUpper())));
    }
}

void modernFloorBoard::setDelayValue(const QString &address, int value,
                                     bool twoByte)
{
    if (!hasValidDelayBuffer())
        return;

    if (twoByte) {
        const QString high = QString("%1").arg(
            value / 128, 2, 16, QChar('0')).toUpper();
        const QString low = QString("%1").arg(
            value % 128, 2, 16, QChar('0')).toUpper();
        SysxIO::Instance()->setFileSource(
            "Structure", "0A", "00", address, high, low);
    } else {
        SysxIO::Instance()->setFileSource(
            "Structure", "0A", "00", address,
            QString("%1").arg(value, 2, 16, QChar('0')).toUpper());
    }
}

void modernFloorBoard::delayComboChanged(int value)
{
    QComboBox *combo = qobject_cast<QComboBox *>(sender());
    if (!combo)
        return;
    if (combo == delayType) {
        setDelayType(value);
        return;
    }
    setDelayValue(combo->property("address").toString(), value);
}

void modernFloorBoard::setDelayType(int index)
{
    if (!delayType || index < 0 || index >= delayType->count()
        || !hasValidDelayBuffer())
        return;

    {
        const QSignalBlocker blocker(delayType);
        delayType->setCurrentIndex(index);
    }
    setDelayValue("01", index);
    if (delayModelBrowser)
        delayModelBrowser->setCurrentIndex(index);
    updateDelayPageForType(index);
    if (delayArtwork)
        delayArtwork->setTextOverlayText(
            "type", delayArtworkType(delayType->itemText(index)));
}

void modernFloorBoard::delayModelSelected(int index)
{
    setDelayType(index);
}

void modernFloorBoard::delayBarChanged(int value)
{
    ParameterBar *bar = qobject_cast<ParameterBar *>(sender());
    if (!bar)
        return;
    const QString address = bar->property("address").toString();
    setDelayValue(address, value, bar->property("twoByte").toBool());
    const QString display = formatRhythmicDivision(
        MidiTable::Instance()->getValue(
            "Structure", "0A", "00", address,
            QString::number(value, 16).toUpper()));

    // Direct Level is presented on both the standard and Dual pages.
    // Keep duplicate views of the same backend address synchronized.
    for (ParameterBar *peer : delayBars) {
        if (!peer || peer->property("address").toString() != address)
            continue;
        const QSignalBlocker blocker(peer);
        peer->setValue(value);
        peer->setDisplayText(display);
    }
}

void modernFloorBoard::delayToggleChanged()
{
    QObject *toggle = sender();
    if (!toggle || !hasValidDelayBuffer())
        return;
    const QString address = toggle->property("address").toString();
    const bool newState = SysxIO::Instance()->getSourceValue(
        "Structure", "0A", "00", address) != 1;
    setDelayValue(address, newState ? 1 : 0);
    refreshDelayState();
}

void modernFloorBoard::toggleComp()
{
    if (!hasValidCompBuffer())
        return;
    SysxIO *sysxIO = SysxIO::Instance();
    const bool newState = sysxIO->getSourceValue(
        "Structure", "00", "00", "40") != 1;
    setCompValue("40", newState ? 1 : 0);
    refreshCompState();
}

void modernFloorBoard::toggleReverb()
{
    SysxIO *sysxIO = SysxIO::Instance();

    if (!hasValidReverbBuffer())
        return;

    int current = sysxIO->getSourceValue(
        "Structure",
        "0A",
        "00",
        "30"
    );

    bool newState = (current != 1);

    sysxIO->setFileSource(
        "Structure",
        "0A",
        "00",
        "30",
        newState ? "01" : "00"
    );

    refreshReverbState();
}
