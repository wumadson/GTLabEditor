#include "modernNoiseSuppressorEditor.h"

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

QString enumLabel(const Midi &entry)
{
    if (!entry.customdesc.trimmed().isEmpty())
        return entry.customdesc.trimmed();
    if (!entry.desc.trimmed().isEmpty())
        return entry.desc.trimmed();
    return entry.name.trimmed();
}
}

ModernNoiseSuppressorEditor::ModernNoiseSuppressorEditor(
    NoiseSuppressorSlot slot, QObject *parent)
    : QObject(parent), nsSlot(slot)
{
    buildEditor();
}

EffectEditorPanel *ModernNoiseSuppressorEditor::widget() const
{
    return editor;
}

NoiseSuppressorSlot ModernNoiseSuppressorEditor::slot() const
{
    return nsSlot;
}

QString ModernNoiseSuppressorEditor::addressForOffset(int offset) const
{
    const int base = nsSlot == NoiseSuppressorSlot::NS1 ? 0x71 : 0x75;
    return QString("%1").arg(base + offset, 2, 16, QChar('0')).toUpper();
}

void ModernNoiseSuppressorEditor::buildEditor()
{
    const QString name = nsSlot == NoiseSuppressorSlot::NS1
        ? "NS-1" : "NS-2";
    const QColor accent(ModernTheme::activeEffectAccent(name));

    editor = new EffectEditorPanel(name);
    editor->typeLabel()->hide();
    editor->setRightPanelTitle("DETECTION SOURCE");

    QFrame *visual = new QFrame;
    visual->setObjectName("NoiseSuppressorVisual");
    visual->setStyleSheet(QString(
        "QFrame#NoiseSuppressorVisual{background:%1;border:1px solid %2;"
        "border-radius:10px;}"
        "QLabel#NoiseSuppressorMark{color:%3;font-size:30px;"
        "font-weight:700;letter-spacing:2px;}"
        "QLabel#NoiseSuppressorName{color:%4;font-size:11px;"
        "font-weight:700;letter-spacing:1px;}")
        .arg(ModernTheme::color(ModernTheme::ControlBackground),
             ModernTheme::color(ModernTheme::Border), accent.name(),
             ModernTheme::color(ModernTheme::SecondaryText)));
    QVBoxLayout *visualLayout = new QVBoxLayout(visual);
    visualLayout->setContentsMargins(18, 18, 18, 18);
    visualLayout->setSpacing(6);
    QLabel *mark = new QLabel(name);
    mark->setObjectName("NoiseSuppressorMark");
    mark->setAlignment(Qt::AlignCenter);
    QLabel *description = new QLabel("NOISE SUPPRESSOR");
    description->setObjectName("NoiseSuppressorName");
    description->setAlignment(Qt::AlignCenter);
    visualLayout->addStretch(2);
    visualLayout->addWidget(mark);
    visualLayout->addWidget(description);
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

    QLabel *sectionTitle = new QLabel("SUPPRESSION");
    sectionTitle->setObjectName("ParameterSectionTitle");
    parameterLayout->addWidget(sectionTitle);

    MidiTable *midiTable = MidiTable::Instance();
    thresholdBar = new ParameterBar("Threshold");
    thresholdBar->setAccentColor(accent);
    thresholdBar->setRange(
        midiTable->getRangeMinimum(kStructure, kBank, kMiddleByte,
                                   addressForOffset(1)),
        midiTable->getRange(kStructure, kBank, kMiddleByte,
                            addressForOffset(1)));
    parameterLayout->addWidget(thresholdBar);

    releaseBar = new ParameterBar("Release");
    releaseBar->setAccentColor(accent);
    releaseBar->setRange(
        midiTable->getRangeMinimum(kStructure, kBank, kMiddleByte,
                                   addressForOffset(2)),
        midiTable->getRange(kStructure, kBank, kMiddleByte,
                            addressForOffset(2)));
    parameterLayout->addWidget(releaseBar);
    parameterLayout->addStretch(1);

    ParameterCombo *detectControl = new ParameterCombo("Detect");
    detectControl->setLabelVisible(false);
    detectCombo = detectControl->comboBox();
    const Midi detectMap = midiTable->getMidiMap(
        kStructure, kBank, kMiddleByte, addressForOffset(3));
    for (const Midi &entry : detectMap.level) {
        if (entry.value == "range")
            continue;
        bool rawOk = false;
        const int raw = entry.value.toInt(&rawOk, 16);
        if (rawOk)
            detectCombo->addItem(enumLabel(entry).toUpper(), raw);
    }
    QWidget *detectionPanel = new QWidget;
    QVBoxLayout *detectionLayout = new QVBoxLayout(detectionPanel);
    detectionLayout->setContentsMargins(0, 0, 0, 0);
    detectionLayout->setSpacing(8);
    detectionLayout->addWidget(detectControl);
    detectionLayout->addStretch(1);
    editor->setRightPanelWidget(detectionPanel);

    connect(stateToggle, &QAbstractButton::clicked,
            this, [this](bool checked) {
        if (refreshing || !available)
            return;
        setNoiseSuppressorValue(0, checked ? 1 : 0);
        emit stateChanged(true, checked);
    });
    connect(thresholdBar, &QAbstractSlider::valueChanged,
            this, [this](int raw) {
        if (refreshing || !available)
            return;
        setNoiseSuppressorValue(1, raw);
        thresholdBar->setDisplayText(displayValue(1, raw));
    });
    connect(releaseBar, &QAbstractSlider::valueChanged,
            this, [this](int raw) {
        if (refreshing || !available)
            return;
        setNoiseSuppressorValue(2, raw);
        releaseBar->setDisplayText(displayValue(2, raw));
    });
    connect(detectCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        if (refreshing || !available || index < 0)
            return;
        bool rawOk = false;
        const int raw = detectCombo->itemData(index).toInt(&rawOk);
        if (rawOk)
            setNoiseSuppressorValue(3, raw);
    });

    updateControls(false);
}

bool ModernNoiseSuppressorEditor::bufferContains(int offset) const
{
    bool offsetOk = false;
    const int address = addressForOffset(offset).toInt(&offsetOk, 16);
    if (!offsetOk)
        return false;
    const SysxData source = SysxIO::Instance()->getFileSource();
    const int addressIndex = source.address.indexOf(kBank + kMiddleByte);
    if (addressIndex < 0 || addressIndex >= source.hex.size())
        return false;
    const int valueIndex = sysxDataOffset + address;
    return valueIndex >= 0
        && valueIndex < source.hex.at(addressIndex).size();
}

bool ModernNoiseSuppressorEditor::hasValidBuffer(
    bool backendConnected, bool backendHasPatchData) const
{
    return backendConnected && backendHasPatchData
        && SysxIO::Instance()->isConnected()
        && bufferContains(0);
}

int ModernNoiseSuppressorEditor::readValue(int offset) const
{
    return SysxIO::Instance()->getSourceValue(
        kStructure, kBank, kMiddleByte, addressForOffset(offset));
}

void ModernNoiseSuppressorEditor::setNoiseSuppressorValue(
    int offset, int raw)
{
    if (!available || !bufferContains(offset))
        return;
    SysxIO::Instance()->setFileSource(
        kStructure, kBank, kMiddleByte, addressForOffset(offset),
        QString("%1").arg(raw, 2, 16, QChar('0')).toUpper());
}

QString ModernNoiseSuppressorEditor::displayValue(int offset, int raw) const
{
    return MidiTable::Instance()->getValue(
        kStructure, kBank, kMiddleByte, addressForOffset(offset),
        QString::number(raw, 16).toUpper());
}

void ModernNoiseSuppressorEditor::updateControls(bool controlsAvailable)
{
    available = controlsAvailable;
    if (stateToggle)
        stateToggle->setEnabled(controlsAvailable);

    const bool thresholdAvailable = controlsAvailable && bufferContains(1);
    const bool releaseAvailable = controlsAvailable && bufferContains(2);
    const bool detectAvailable = controlsAvailable && bufferContains(3);
    if (thresholdBar) {
        thresholdBar->setEnabled(thresholdAvailable);
        if (!thresholdAvailable)
            thresholdBar->setDisplayText(QString::fromUtf8("—"));
    }
    if (releaseBar) {
        releaseBar->setEnabled(releaseAvailable);
        if (!releaseAvailable)
            releaseBar->setDisplayText(QString::fromUtf8("—"));
    }
    if (detectCombo) {
        detectCombo->setEnabled(detectAvailable);
        if (!detectAvailable) {
            const QSignalBlocker blocker(detectCombo);
            detectCombo->setCurrentIndex(-1);
        }
    }
    if (!controlsAvailable && stateToggle) {
        const QSignalBlocker blocker(stateToggle);
        stateToggle->setChecked(false);
    }
}

void ModernNoiseSuppressorEditor::refreshNoiseSuppressor(
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

    const bool on = readValue(0) == 1;
    {
        const QSignalBlocker blocker(stateToggle);
        stateToggle->setChecked(on);
    }
    if (bufferContains(1)) {
        const int raw = readValue(1);
        const QSignalBlocker blocker(thresholdBar);
        thresholdBar->setValue(raw);
        thresholdBar->setDisplayText(displayValue(1, raw));
    }
    if (bufferContains(2)) {
        const int raw = readValue(2);
        const QSignalBlocker blocker(releaseBar);
        releaseBar->setValue(raw);
        releaseBar->setDisplayText(displayValue(2, raw));
    }
    if (bufferContains(3)) {
        const int raw = readValue(3);
        const QSignalBlocker blocker(detectCombo);
        detectCombo->setCurrentIndex(detectCombo->findData(raw));
    }
    refreshing = false;
    emit stateChanged(true, on);
}
