#include "modernPedalFxEditor.h"

#include "MidiTable.h"
#include "SysxIO.h"
#include "effectModelBrowser.h"
#include "globalVariables.h"
#include "effectArtworkWidget.h"
#include "modernTheme.h"
#include "modernWidgets.h"
#include "parameterBar.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {
const QString kStructure = "Structure";
const QString kBank = "0A";
const QString kMiddleByte = "00";

const QStringList kModeNames = {
    "OFF", "FOOT VOLUME", "PEDAL BEND", "WAH", "PB/FV", "WAH/FV"
};

QString enumLabel(const Midi &entry)
{
    if (!entry.customdesc.trimmed().isEmpty())
        return entry.customdesc.trimmed();
    if (!entry.desc.trimmed().isEmpty())
        return entry.desc.trimmed();
    return entry.name.trimmed();
}

QLabel *sectionTitle(const QString &text)
{
    QLabel *label = new QLabel(text);
    label->setObjectName("ParameterSectionTitle");
    return label;
}

class ResponsivePedalControlRow final : public QWidget
{
public:
    explicit ResponsivePedalControlRow(QWidget *parent = nullptr)
        : QWidget(parent), columns(0)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        grid = new QGridLayout(this);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(10);
        grid->setVerticalSpacing(8);
    }

    void addControl(QWidget *control)
    {
        controls.append(control);
        updateLayout();
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        updateLayout();
    }

private:
    void updateLayout()
    {
        constexpr int breakpoint = 470;
        constexpr int hysteresis = 8;
        int nextColumns = columns;
        if (nextColumns == 0)
            nextColumns = width() >= breakpoint ? 3 : 2;
        else if (nextColumns == 3 && width() < breakpoint - hysteresis)
            nextColumns = 2;
        else if (nextColumns == 2 && width() > breakpoint + hysteresis)
            nextColumns = 3;
        if (nextColumns == columns && grid->count() == controls.size())
            return;

        columns = nextColumns;
        while (grid->count() > 0)
            delete grid->takeAt(0);
        for (int index = 0; index < controls.size(); ++index) {
            const int row = columns == 3 ? 0 : index / 2;
            const int column = columns == 3 ? index : index % 2;
            grid->addWidget(controls.at(index), row, column,
                            Qt::AlignLeft | Qt::AlignTop);
        }
        for (int column = 0; column < 3; ++column)
            grid->setColumnStretch(column, column < columns ? 1 : 0);
        updateGeometry();
    }

    QGridLayout *grid;
    QVector<QWidget *> controls;
    int columns;
};
}

ModernPedalFxEditor::ModernPedalFxEditor(QObject *parent)
    : QObject(parent)
{
    buildEditor();
}

EffectEditorPanel *ModernPedalFxEditor::widget() const
{
    return editor;
}

PedalEditorContext ModernPedalFxEditor::context() const
{
    return editorContext;
}

void ModernPedalFxEditor::buildEditor()
{
    const QColor accent(ModernTheme::activeEffectAccent("PEDAL FX"));
    editor = new EffectEditorPanel(tr("PEDAL / EXP"));
    editor->typeLabel()->hide();
    editor->setRightPanelTitle("P.FX MODES");

    artwork = new EffectArtworkWidget;
    artwork->setArtwork(":/assets/pedals/exp_generic.png");
    editor->setArtworkWidget(artwork);

    browser = new EffectModelBrowser;
    browser->setAccentColor(accent);
    browser->setModels(kModeNames);
    modeRawValues = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
    editor->setModelBrowserWidget(browser);

    QVBoxLayout *layout = new QVBoxLayout(editor->parameterArea());
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    QWidget *stateRow = new QWidget;
    QHBoxLayout *stateLayout = new QHBoxLayout(stateRow);
    stateLayout->setContentsMargins(0, 0, 0, 0);
    EffectToggleControl *stateControl = new EffectToggleControl(tr("State"));
    stateToggle = stateControl->toggle();
    stateToggle->setAccentColor(accent);
    stateLayout->addWidget(stateControl, 0, Qt::AlignTop);
    stateLayout->addStretch(1);
    layout->addWidget(stateRow);

    layout->addWidget(sectionTitle("CONTROL"));
    ResponsivePedalControlRow *controlColumns =
        new ResponsivePedalControlRow;
    controlColumns->addControl(createCombo("EXP Switch Function", "46"));
    controlColumns->addControl(createCombo("CTL1 Function", "47"));
    controlColumns->addControl(createCombo("CTL2 Function", "48"));
    layout->addWidget(controlColumns);

    modeStack = new QStackedWidget;
    for (int mode = 0; mode < kModeNames.size(); ++mode)
        modeStack->addWidget(createModePage(mode));
    layout->addWidget(modeStack, 1);

    connect(browser, &EffectModelBrowser::modelSelected,
            this, [this](int index) {
        if (!refreshing && index >= 0 && index < modeRawValues.size())
            setMode(modeRawValues.at(index), true);
    });
    connect(stateToggle, &QAbstractButton::clicked,
            this, [this](bool checked) {
        if (refreshing || !available)
            return;
        writeValue("40", checked ? 1 : 0);
        stateOn = checked;
        emitActivity();
    });

    updateControls(false);
}

QWidget *ModernPedalFxEditor::createModePage(int mode)
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(7);

    if (mode == 0) {
        QLabel *message = new QLabel(
            "No pedal algorithm is active in the current Mode.");
        message->setObjectName("WorkspaceUnavailable");
        message->setAlignment(Qt::AlignCenter);
        message->setWordWrap(true);
        layout->addStretch(1);
        layout->addWidget(message);
        layout->addStretch(2);
        return page;
    }

    if (mode == 1)
        layout->addWidget(createFootVolumeSection());
    else if (mode == 2)
        layout->addWidget(createPedalBendSection());
    else if (mode == 3)
        layout->addWidget(createWahSection());
    else if (mode == 4) {
        layout->addWidget(createPedalBendSection());
        layout->addWidget(createFootVolumeSection());
    } else if (mode == 5) {
        layout->addWidget(createWahSection());
        layout->addWidget(createFootVolumeSection());
    }
    layout->addStretch(1);
    return page;
}

QWidget *ModernPedalFxEditor::createCombo(const QString &label,
                                           const QString &address)
{
    ParameterCombo *control = new ParameterCombo(label);
    QComboBox *combo = control->comboBox();
    const Midi map = MidiTable::Instance()->getMidiMap(
        kStructure, kBank, kMiddleByte, address);
    for (const Midi &entry : map.level) {
        if (entry.value == "range")
            continue;
        bool ok = false;
        const int raw = entry.value.toInt(&ok, 16);
        if (ok)
            combo->addItem(enumLabel(entry), raw);
    }
    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, combo, address](int index) {
        if (refreshing || !available || index < 0)
            return;
        bool ok = false;
        const int raw = combo->itemData(index).toInt(&ok);
        if (!ok)
            return;
        writeValue(address, raw);
        for (Binding &peer : bindings) {
            if (peer.combo && peer.combo != combo
                && peer.address == address) {
                const QSignalBlocker blocker(peer.combo);
                peer.combo->setCurrentIndex(peer.combo->findData(raw));
            }
        }
        if (address == "49")
            updateCustomWahVisibility();
    });
    Binding binding;
    binding.address = address;
    binding.container = control;
    binding.combo = combo;
    bindings.append(binding);
    return control;
}

QWidget *ModernPedalFxEditor::createBar(const QString &label,
                                         const QString &address,
                                         int centerRaw)
{
    ParameterBar *bar = new ParameterBar(label);
    bar->setAccentColor(QColor(
        ModernTheme::activeEffectAccent("PEDAL FX")));
    MidiTable *table = MidiTable::Instance();
    bar->setRange(table->getRangeMinimum(
                      kStructure, kBank, kMiddleByte, address),
                  table->getRange(
                      kStructure, kBank, kMiddleByte, address));
    if (centerRaw >= 0)
        bar->setCenterValue(centerRaw);
    connect(bar, &QAbstractSlider::valueChanged,
            this, [this, bar, address](int raw) {
        if (refreshing || !available)
            return;
        writeValue(address, raw);
        const QString text = displayValue(address, raw);
        bar->setDisplayText(text);
        for (Binding &peer : bindings) {
            if (peer.bar && peer.bar != bar && peer.address == address) {
                const QSignalBlocker blocker(peer.bar);
                peer.bar->setValue(raw);
                peer.bar->setDisplayText(text);
            }
        }
    });
    Binding binding;
    binding.address = address;
    binding.container = bar;
    binding.bar = bar;
    bindings.append(binding);
    return bar;
}

QWidget *ModernPedalFxEditor::createFootVolumeSection()
{
    QFrame *section = new QFrame;
    section->setObjectName("PedalFootVolumeSection");
    QVBoxLayout *layout = new QVBoxLayout(section);
    layout->setContentsMargins(8, 7, 8, 7);
    layout->setSpacing(7);
    layout->addWidget(sectionTitle("FOOT VOLUME"));
    layout->addWidget(createBar("Level", "5A"));
    layout->addWidget(createBar("Min", "5B"));
    layout->addWidget(createBar("Max", "5C"));
    layout->addWidget(createCombo("Curve", "5D"));
    footVolumeSections.append(section);
    return section;
}

QWidget *ModernPedalFxEditor::createPedalBendSection()
{
    QWidget *section = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(7);
    layout->addWidget(sectionTitle("PEDAL BEND"));
    layout->addWidget(createBar("Pitch Min", "54", 0x18));
    layout->addWidget(createBar("Pitch Max", "55", 0x18));
    layout->addWidget(createBar("Position", "56"));
    layout->addWidget(createBar("Effect", "57"));
    layout->addWidget(createBar("Direct", "58"));
    return section;
}

QWidget *ModernPedalFxEditor::createWahSection()
{
    QWidget *section = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(7);
    layout->addWidget(sectionTitle("WAH"));
    layout->addWidget(createCombo("Wah Type", "49"));
    layout->addWidget(createBar("Position", "4A"));
    layout->addWidget(createBar("Min", "4B"));
    layout->addWidget(createBar("Max", "4C"));
    layout->addWidget(createBar("Effect", "4D"));
    layout->addWidget(createBar("Direct", "4E"));
    layout->addWidget(createCustomWahSection());
    return section;
}

QWidget *ModernPedalFxEditor::createCustomWahSection()
{
    QWidget *section = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(7);
    layout->addWidget(sectionTitle("CUSTOM WAH"));
    layout->addWidget(createCombo("Custom Type", "4F"));
    layout->addWidget(createBar("Q", "50", 0x05));
    layout->addWidget(createBar("Range Low", "51", 0x05));
    layout->addWidget(createBar("Range High", "52", 0x05));
    layout->addWidget(createBar("Presence", "53", 0x05));
    section->hide();
    customWahSections.append(section);
    return section;
}

bool ModernPedalFxEditor::bufferContains(const QString &address) const
{
    bool ok = false;
    const int offset = address.toInt(&ok, 16);
    if (!ok)
        return false;
    const SysxData source = SysxIO::Instance()->getFileSource();
    const int addressIndex = source.address.indexOf(kBank + kMiddleByte);
    if (addressIndex < 0 || addressIndex >= source.hex.size())
        return false;
    const int index = sysxDataOffset + offset;
    return index >= 0 && index < source.hex.at(addressIndex).size();
}

bool ModernPedalFxEditor::hasValidBuffer(bool backendConnected,
                                         bool backendHasPatchData) const
{
    return backendConnected && backendHasPatchData
        && SysxIO::Instance()->isConnected()
        && bufferContains("40") && bufferContains("45");
}

int ModernPedalFxEditor::readValue(const QString &address) const
{
    return SysxIO::Instance()->getSourceValue(
        kStructure, kBank, kMiddleByte, address);
}

void ModernPedalFxEditor::writeValue(const QString &address, int raw)
{
    if (!available || !bufferContains(address))
        return;
    SysxIO::Instance()->setFileSource(
        kStructure, kBank, kMiddleByte, address,
        QString("%1").arg(raw, 2, 16, QChar('0')).toUpper());
}

QString ModernPedalFxEditor::displayValue(const QString &address,
                                           int raw) const
{
    return MidiTable::Instance()->getValue(
        kStructure, kBank, kMiddleByte, address,
        QString::number(raw, 16).toUpper());
}

void ModernPedalFxEditor::setMode(int raw, bool writeBackend)
{
    if (raw < 0 || raw >= kModeNames.size())
        return;
    if (writeBackend)
        writeValue("45", raw);
    currentMode = raw;
    if (browser)
        browser->setCurrentIndex(modeRawValues.indexOf(raw));
    if (modeStack)
        modeStack->setCurrentIndex(raw);
    if (modeDisplay)
        modeDisplay->setText(kModeNames.at(raw));
    refreshBindings();
    updateCustomWahVisibility();
    updateContextPresentation();
    emitActivity();
}

void ModernPedalFxEditor::updateControls(bool controlsAvailable)
{
    available = controlsAvailable;
    if (browser)
        browser->setEnabled(controlsAvailable);
    if (stateToggle)
        stateToggle->setEnabled(controlsAvailable);
    for (Binding &binding : bindings) {
        const bool bindingAvailable = controlsAvailable
            && bufferContains(binding.address);
        if (binding.container)
            binding.container->setEnabled(bindingAvailable);
        if (!bindingAvailable && binding.bar)
            binding.bar->setDisplayText(QString::fromUtf8("—"));
        if (!bindingAvailable && binding.combo) {
            const QSignalBlocker blocker(binding.combo);
            binding.combo->setCurrentIndex(-1);
        }
    }
    if (!controlsAvailable) {
        if (stateToggle) {
            const QSignalBlocker blocker(stateToggle);
            stateToggle->setChecked(false);
        }
        if (browser)
            browser->setCurrentIndex(-1);
        if (modeDisplay)
            modeDisplay->setText(QString::fromUtf8("—"));
    }
}

void ModernPedalFxEditor::refreshBindings()
{
    if (!available)
        return;
    for (Binding &binding : bindings) {
        const bool bindingAvailable = bufferContains(binding.address);
        if (binding.container)
            binding.container->setEnabled(bindingAvailable);
        if (!bindingAvailable)
            continue;
        const int raw = readValue(binding.address);
        if (binding.bar) {
            const QSignalBlocker blocker(binding.bar);
            binding.bar->setValue(raw);
            binding.bar->setDisplayText(displayValue(binding.address, raw));
        } else if (binding.combo) {
            const QSignalBlocker blocker(binding.combo);
            binding.combo->setCurrentIndex(binding.combo->findData(raw));
        }
    }
}

void ModernPedalFxEditor::updateCustomWahVisibility()
{
    const bool custom = available && bufferContains("49")
        && readValue("49") == 0x06;
    for (QWidget *section : customWahSections) {
        if (section)
            section->setVisible(custom);
    }
}

void ModernPedalFxEditor::setContext(PedalEditorContext newContext)
{
    editorContext = newContext;
    updateContextPresentation();
}

void ModernPedalFxEditor::updateContextPresentation()
{
    const bool footVolumeInMode = currentMode == 1
        || currentMode == 4 || currentMode == 5;
    if (contextMessage) {
        if (editorContext == PedalEditorContext::General)
        contextMessage->setText(tr("GENERAL PEDAL FX CONTEXT"));
        else if (footVolumeInMode)
        contextMessage->setText(tr("FOOT VOLUME SECTION ACTIVE"));
        else
            contextMessage->setText(
                "FOOT VOLUME IS NOT ACTIVE IN THE CURRENT MODE");
    }
    const QColor accent(ModernTheme::activeEffectAccent("FOOT VOLUME"));
    for (QWidget *section : footVolumeSections) {
        if (!section)
            continue;
        const bool highlighted = editorContext == PedalEditorContext::FootVolume
            && footVolumeInMode;
        section->setStyleSheet(highlighted
            ? QString("QFrame#PedalFootVolumeSection{border:1px solid %1;"
                      "border-radius:6px;background:rgba(123,133,141,18);}")
                  .arg(accent.name())
            : "QFrame#PedalFootVolumeSection{border:1px solid transparent;}");
    }
}

void ModernPedalFxEditor::emitActivity()
{
    const bool pedalFxActive = available && stateOn
        && (currentMode == 2 || currentMode == 3
            || currentMode == 4 || currentMode == 5);
    const bool footVolumeActive = available && stateOn
        && (currentMode == 1 || currentMode == 4 || currentMode == 5);
    emit activityChanged(available, pedalFxActive, footVolumeActive);
}

void ModernPedalFxEditor::refreshPedalFx(bool backendConnected,
                                          bool backendHasPatchData)
{
    refreshing = true;
    const bool valid = hasValidBuffer(backendConnected,
                                      backendHasPatchData);
    updateControls(valid);
    if (!valid) {
        stateOn = false;
        currentMode = 0;
        updateContextPresentation();
        refreshing = false;
        emit activityChanged(false, false, false);
        return;
    }

    stateOn = readValue("40") == 1;
    if (stateToggle) {
        const QSignalBlocker blocker(stateToggle);
        stateToggle->setChecked(stateOn);
    }
    setMode(readValue("45"), false);
    refreshBindings();
    updateCustomWahVisibility();
    updateContextPresentation();
    refreshing = false;
    emitActivity();
}
