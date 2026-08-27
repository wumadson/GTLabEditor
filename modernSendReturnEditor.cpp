#include "modernSendReturnEditor.h"

#include "MidiTable.h"
#include "SysxIO.h"
#include "globalVariables.h"
#include "effectArtworkWidget.h"
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

void ModernSendReturnEditor::addStateRowAction(QWidget *action)
{
    if (stateRowLayout && action)
        stateRowLayout->addWidget(action, 0, Qt::AlignTop);
}

void ModernSendReturnEditor::buildEditor()
{
    const QColor accent(ModernTheme::activeEffectAccent("SEND/RETURN"));

    editor = new EffectEditorPanel("SEND/RETURN");
    editor->typeLabel()->hide();
    editor->setRightPanelTitle("LOOP MODE");

    artwork = new EffectArtworkWidget;
    artwork->setArtwork(":/assets/effects/pedal_generic.png");
    artwork->setGenericPedalIdentity(
        "S/R", QColor(ModernTheme::color(ModernTheme::PrimaryText)),
        QColor(ModernTheme::effectColor("SEND/RETURN")));
    editor->setArtworkWidget(artwork);

    QVBoxLayout *parameterLayout =
        new QVBoxLayout(editor->parameterArea());
    parameterLayout->setContentsMargins(0, 0, 0, 0);
    parameterLayout->setSpacing(8);

    QWidget *stateRow = new QWidget;
    stateRowLayout = new QHBoxLayout(stateRow);
    stateRowLayout->setContentsMargins(0, 0, 0, 0);
    stateRowLayout->setSpacing(0);
    EffectToggleControl *stateControl = new EffectToggleControl("State");
    stateToggle = stateControl->toggle();
    stateToggle->setAccentColor(accent);
    stateRowLayout->addWidget(stateControl, 0, Qt::AlignTop);
    stateRowLayout->addStretch(1);
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
    modeControl->setLabelVisible(false);
    modeCombo = modeControl->comboBox();
    modeCombo->addItem("NORMAL", 0x00);
    modeCombo->addItem("DIRECT MIX", 0x01);
    modeCombo->addItem("BRANCH OUT", 0x02);
    QWidget *modePanel = new QWidget;
    QVBoxLayout *modeLayout = new QVBoxLayout(modePanel);
    modeLayout->setContentsMargins(0, 0, 0, 0);
    modeLayout->setSpacing(8);
    modeLayout->addWidget(modeControl);
    modeLayout->addStretch(1);
    editor->setRightPanelWidget(modePanel);

    connect(stateToggle, &QAbstractButton::clicked,
            this, [this](bool checked) {
        if (refreshing || !available)
            return;
        setSendReturnValue(kStateAddress, checked ? 1 : 0);
        artwork->setGenericPedalState(true, checked);
        emit stateChanged(true, checked);
    });
    connect(modeCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        if (refreshing || !available || index < 0)
            return;
        bool rawOk = false;
        const int raw = modeCombo->itemData(index).toInt(&rawOk);
        if (rawOk) {
            setSendReturnValue(kModeAddress, raw);
            artwork->setTextOverlayText("type",
                                        modeCombo->itemText(index));
        }
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
        artwork->setTextOverlayText("type", QString());
        artwork->setGenericPedalState(false, false);
        refreshing = false;
        emit stateChanged(false, false);
        return;
    }

    const bool on = readValue(kStateAddress) == 1;
    artwork->setGenericPedalState(true, on);
    {
        const QSignalBlocker blocker(stateToggle);
        stateToggle->setChecked(on);
    }
    if (bufferContains(kModeAddress)) {
        const int raw = readValue(kModeAddress);
        const QSignalBlocker blocker(modeCombo);
        modeCombo->setCurrentIndex(modeCombo->findData(raw));
        artwork->setTextOverlayText("type", modeCombo->currentText());
    } else {
        artwork->setTextOverlayText("type", QString());
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
