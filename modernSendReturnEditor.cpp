#include "modernSendReturnEditor.h"

#include "MidiTable.h"
#include "SysxIO.h"
#include "globalVariables.h"
#include "modernTheme.h"
#include "modernWidgets.h"
#include "parameterBar.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace {
const QString kStructure = "Structure";
const QString kBank = "0A";
const QString kMiddleByte = "00";
const QString kStateAddress = "79";
const QString kModeAddress = "7A";
const QString kSendLevelAddress = "7B";
const QString kReturnLevelAddress = "7C";
}

ModernSendReturnEditor::ModernSendReturnEditor(QObject *parent)
    : QObject(parent)
{
    buildEditor();
}

EffectEditorPanel *ModernSendReturnEditor::widget() const
{
    return editor;
}

void ModernSendReturnEditor::buildEditor()
{
    const QColor accent(ModernTheme::activeEffectAccent("SEND/RETURN"));

    editor = new EffectEditorPanel("SEND/RETURN");
    editor->typeLabel()->hide();
    editor->setRightPanelTitle("LOOP MODE");

    QFrame *visual = new QFrame;
    visual->setObjectName("SendReturnVisual");
    visual->setStyleSheet(QString(
        "QFrame#SendReturnVisual{background:%1;border:1px solid %2;"
        "border-radius:10px;}"
        "QLabel#SendReturnMark{color:%3;font-size:30px;font-weight:700;"
        "letter-spacing:2px;}"
        "QLabel#SendReturnName{color:%4;font-size:11px;font-weight:700;"
        "letter-spacing:1px;}"
        "QLabel#SendReturnPort{color:%4;font-size:10px;font-weight:600;}"
        "QFrame#SendReturnLine{background:%3;border:0;}")
        .arg(ModernTheme::color(ModernTheme::ControlBackground),
             ModernTheme::color(ModernTheme::Border), accent.name(),
             ModernTheme::color(ModernTheme::SecondaryText)));
    QVBoxLayout *visualLayout = new QVBoxLayout(visual);
    visualLayout->setContentsMargins(18, 18, 18, 18);
    visualLayout->setSpacing(8);
    QLabel *mark = new QLabel("S/R");
    mark->setObjectName("SendReturnMark");
    mark->setAlignment(Qt::AlignCenter);
    QLabel *description = new QLabel("EXTERNAL LOOP");
    description->setObjectName("SendReturnName");
    description->setAlignment(Qt::AlignCenter);
    QWidget *ports = new QWidget;
    QHBoxLayout *portsLayout = new QHBoxLayout(ports);
    portsLayout->setContentsMargins(8, 0, 8, 0);
    portsLayout->setSpacing(8);
    QLabel *send = new QLabel("SEND");
    send->setObjectName("SendReturnPort");
    QFrame *line = new QFrame;
    line->setObjectName("SendReturnLine");
    line->setFixedHeight(1);
    QLabel *receive = new QLabel("RETURN");
    receive->setObjectName("SendReturnPort");
    portsLayout->addWidget(send);
    portsLayout->addWidget(line, 1);
    portsLayout->addWidget(receive);
    visualLayout->addStretch(2);
    visualLayout->addWidget(mark);
    visualLayout->addWidget(description);
    visualLayout->addSpacing(8);
    visualLayout->addWidget(ports);
    visualLayout->addStretch(3);
    editor->setArtworkWidget(visual);

    QVBoxLayout *parameterLayout =
        new QVBoxLayout(editor->parameterArea());
    parameterLayout->setContentsMargins(0, 0, 0, 0);
    parameterLayout->setSpacing(8);

    QWidget *stateRow = new QWidget;
    QHBoxLayout *stateLayout = new QHBoxLayout(stateRow);
    stateLayout->setContentsMargins(0, 0, 0, 0);
    stateLayout->setSpacing(0);
    EffectToggleControl *stateControl = new EffectToggleControl("State");
    stateToggle = stateControl->toggle();
    stateToggle->setAccentColor(accent);
    stateLayout->addWidget(stateControl, 0, Qt::AlignTop);
    stateLayout->addStretch(1);
    parameterLayout->addWidget(stateRow);

    QLabel *sectionTitle = new QLabel("LEVELS");
    sectionTitle->setObjectName("ParameterSectionTitle");
    parameterLayout->addWidget(sectionTitle);

    MidiTable *midiTable = MidiTable::Instance();
    sendLevelBar = new ParameterBar("Send Level");
    sendLevelBar->setAccentColor(accent);
    sendLevelBar->setRange(
        midiTable->getRangeMinimum(kStructure, kBank, kMiddleByte,
                                   kSendLevelAddress),
        midiTable->getRange(kStructure, kBank, kMiddleByte,
                            kSendLevelAddress));
    parameterLayout->addWidget(sendLevelBar);

    returnLevelBar = new ParameterBar("Return Level");
    returnLevelBar->setAccentColor(accent);
    returnLevelBar->setRange(
        midiTable->getRangeMinimum(kStructure, kBank, kMiddleByte,
                                   kReturnLevelAddress),
        midiTable->getRange(kStructure, kBank, kMiddleByte,
                            kReturnLevelAddress));
    parameterLayout->addWidget(returnLevelBar);
    parameterLayout->addStretch(1);

    ParameterCombo *modeControl = new ParameterCombo("Mode");
    modeCombo = modeControl->comboBox();
    modeCombo->addItem("NORMAL", 0x00);
    modeCombo->addItem("DIRECT MIX", 0x01);
    modeCombo->addItem("BRANCH OUT", 0x02);
    QWidget *modePanel = new QWidget;
    QVBoxLayout *modeLayout = new QVBoxLayout(modePanel);
    modeLayout->setContentsMargins(0, 0, 0, 0);
    modeLayout->setSpacing(8);
    QLabel *modeTitle = new QLabel("MODE");
    modeTitle->setObjectName("ParameterSectionTitle");
    modeLayout->addWidget(modeTitle);
    modeLayout->addWidget(modeControl);
    modeLayout->addStretch(1);
    editor->setRightPanelWidget(modePanel);

    connect(stateToggle, &QAbstractButton::clicked,
            this, [this](bool checked) {
        if (refreshing || !available)
            return;
        setSendReturnValue(kStateAddress, checked ? 1 : 0);
        emit stateChanged(true, checked);
    });
    connect(modeCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        if (refreshing || !available || index < 0)
            return;
        bool rawOk = false;
        const int raw = modeCombo->itemData(index).toInt(&rawOk);
        if (rawOk)
            setSendReturnValue(kModeAddress, raw);
    });
    connect(sendLevelBar, &QAbstractSlider::valueChanged,
            this, [this](int raw) {
        if (refreshing || !available)
            return;
        setSendReturnValue(kSendLevelAddress, raw);
        sendLevelBar->setDisplayText(
            displayValue(kSendLevelAddress, raw));
    });
    connect(returnLevelBar, &QAbstractSlider::valueChanged,
            this, [this](int raw) {
        if (refreshing || !available)
            return;
        setSendReturnValue(kReturnLevelAddress, raw);
        returnLevelBar->setDisplayText(
            displayValue(kReturnLevelAddress, raw));
    });

    updateControls(false);
}

bool ModernSendReturnEditor::bufferContains(const QString &address) const
{
    bool addressOk = false;
    const int addressValue = address.toInt(&addressOk, 16);
    if (!addressOk)
        return false;
    const SysxData source = SysxIO::Instance()->getFileSource();
    const int addressIndex = source.address.indexOf(kBank + kMiddleByte);
    if (addressIndex < 0 || addressIndex >= source.hex.size())
        return false;
    const int valueIndex = sysxDataOffset + addressValue;
    return valueIndex >= 0
        && valueIndex < source.hex.at(addressIndex).size();
}

bool ModernSendReturnEditor::hasValidBuffer(
    bool backendConnected, bool backendHasPatchData) const
{
    return backendConnected && backendHasPatchData
        && SysxIO::Instance()->isConnected()
        && bufferContains(kStateAddress);
}

int ModernSendReturnEditor::readValue(const QString &address) const
{
    return SysxIO::Instance()->getSourceValue(
        kStructure, kBank, kMiddleByte, address);
}

void ModernSendReturnEditor::setSendReturnValue(
    const QString &address, int raw)
{
    if (!available || !bufferContains(address))
        return;
    SysxIO::Instance()->setFileSource(
        kStructure, kBank, kMiddleByte, address,
        QString("%1").arg(raw, 2, 16, QChar('0')).toUpper());
}

QString ModernSendReturnEditor::displayValue(
    const QString &address, int raw) const
{
    return MidiTable::Instance()->getValue(
        kStructure, kBank, kMiddleByte, address,
        QString::number(raw, 16).toUpper());
}

void ModernSendReturnEditor::updateControls(bool controlsAvailable)
{
    available = controlsAvailable;
    if (stateToggle)
        stateToggle->setEnabled(controlsAvailable);

    const bool modeAvailable = controlsAvailable
        && bufferContains(kModeAddress);
    const bool sendAvailable = controlsAvailable
        && bufferContains(kSendLevelAddress);
    const bool returnAvailable = controlsAvailable
        && bufferContains(kReturnLevelAddress);
    if (modeCombo) {
        modeCombo->setEnabled(modeAvailable);
        if (!modeAvailable) {
            const QSignalBlocker blocker(modeCombo);
            modeCombo->setCurrentIndex(-1);
        }
    }
    if (sendLevelBar) {
        sendLevelBar->setEnabled(sendAvailable);
        if (!sendAvailable)
            sendLevelBar->setDisplayText(QString::fromUtf8("—"));
    }
    if (returnLevelBar) {
        returnLevelBar->setEnabled(returnAvailable);
        if (!returnAvailable)
            returnLevelBar->setDisplayText(QString::fromUtf8("—"));
    }
    if (!controlsAvailable && stateToggle) {
        const QSignalBlocker blocker(stateToggle);
        stateToggle->setChecked(false);
    }
}

void ModernSendReturnEditor::refreshSendReturn(
    bool backendConnected, bool backendHasPatchData)
{
    refreshing = true;
    const bool valid = hasValidBuffer(backendConnected,
                                      backendHasPatchData);
    updateControls(valid);
    if (!valid) {
        refreshing = false;
        emit stateChanged(false, false);
        return;
    }

    const bool on = readValue(kStateAddress) == 1;
    {
        const QSignalBlocker blocker(stateToggle);
        stateToggle->setChecked(on);
    }
    if (bufferContains(kModeAddress)) {
        const int raw = readValue(kModeAddress);
        const QSignalBlocker blocker(modeCombo);
        modeCombo->setCurrentIndex(modeCombo->findData(raw));
    }
    if (bufferContains(kSendLevelAddress)) {
        const int raw = readValue(kSendLevelAddress);
        const QSignalBlocker blocker(sendLevelBar);
        sendLevelBar->setValue(raw);
        sendLevelBar->setDisplayText(
            displayValue(kSendLevelAddress, raw));
    }
    if (bufferContains(kReturnLevelAddress)) {
        const int raw = readValue(kReturnLevelAddress);
        const QSignalBlocker blocker(returnLevelBar);
        returnLevelBar->setValue(raw);
        returnLevelBar->setDisplayText(
            displayValue(kReturnLevelAddress, raw));
    }
    refreshing = false;
    emit stateChanged(true, on);
}
