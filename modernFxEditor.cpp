#include "modernFxEditor.h"

#include "MidiTable.h"
#include "SysxIO.h"
#include "effectArtworkWidget.h"
#include "effectModelBrowser.h"
#include "globalVariables.h"
#include "modernTheme.h"
#include "modernWidgets.h"
#include "parameterBar.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QFont>
#include <QGridLayout>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QStringList>
#include <QVBoxLayout>

#include <algorithm>
#include <initializer_list>

namespace {
const char kStructure[] = "Structure";

class ResponsiveSectionColumns : public QWidget
{
public:
    explicit ResponsiveSectionColumns(QWidget *parent = nullptr)
        : QWidget(parent), grid(new QGridLayout(this))
    {
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(kSpacing);
        grid->setSizeConstraint(QLayout::SetNoConstraint);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }

    void addSection(QWidget *section)
    {
        sections.append(section);
        updateArrangement(true);
    }

    QSize minimumSizeHint() const override
    {
        int minimumWidth = 0;
        int minimumHeight = 0;
        for (QWidget *section : sections) {
            const QSize sectionMinimum = section->minimumSizeHint();
            minimumWidth = qMax(minimumWidth, sectionMinimum.width());
            if (horizontal)
                minimumHeight = qMax(minimumHeight, sectionMinimum.height());
            else
                minimumHeight += sectionMinimum.height();
        }
        if (!horizontal && sections.size() > 1)
            minimumHeight += kSpacing * (sections.size() - 1);
        return QSize(minimumWidth, minimumHeight);
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        updateArrangement(false);
    }

private:
    int horizontalMinimumWidth() const
    {
        int required = 0;
        for (QWidget *section : sections)
            required += qMax(0, section->minimumSizeHint().width());
        if (sections.size() > 1)
            required += kSpacing * (sections.size() - 1);
        return required;
    }

    void updateArrangement(bool force)
    {
        if (sections.isEmpty())
            return;

        const int breakpoint = horizontalMinimumWidth();
        bool useHorizontal = horizontal;
        if (!arranged)
            useHorizontal = width() >= breakpoint;
        else if (horizontal && width() < breakpoint - kHysteresis)
            useHorizontal = false;
        else if (!horizontal && width() > breakpoint + kHysteresis)
            useHorizontal = true;

        if (!force && arranged && useHorizontal == horizontal)
            return;

        while (QLayoutItem *item = grid->takeAt(0))
            delete item;
        for (int index = 0; index < sections.size(); ++index) {
            const int row = useHorizontal ? 0 : index;
            const int column = useHorizontal ? index : 0;
            grid->addWidget(sections.at(index), row, column);
            grid->setRowStretch(index, 0);
            grid->setColumnStretch(index, useHorizontal ? 1 : 0);
        }
        grid->setColumnStretch(0, 1);
        horizontal = useHorizontal;
        arranged = true;
        updateGeometry();
    }

    static constexpr int kSpacing = 12;
    static constexpr int kHysteresis = 8;
    QGridLayout *grid;
    QVector<QWidget *> sections;
    bool horizontal = true;
    bool arranged = false;
};

FxParameterSpec parameter(int bank, const QString &offset,
                          FxControlKind kind, const QString &section,
                          int centerRaw = -1,
                          int continuousMaximum = -1,
                          const QString &labelOverride = QString())
{
    FxParameterSpec spec;
    spec.address = FxAddress::relative(
        bank, offset, kind == FxControlKind::TwoByteSegmentedBar);
    spec.kind = kind;
    spec.section = section;
    spec.centerRaw = centerRaw;
    spec.continuousMaximum = continuousMaximum;
    spec.labelOverride = labelOverride;
    return spec;
}

FxParameterSpec externalParameter(const QString &bank,
                                  const QString &middleByte,
                                  const QString &offset,
                                  FxControlKind kind,
                                  const QString &section,
                                  const QString &labelOverride = QString())
{
    FxParameterSpec spec;
    spec.address = FxAddress::external(
        bank, middleByte, offset,
        kind == FxControlKind::TwoByteSegmentedBar);
    spec.kind = kind;
    spec.section = section;
    spec.labelOverride = labelOverride;
    return spec;
}

FxParameterSpec visibleWhen(FxParameterSpec spec, int controllerBank,
                            const QString &controllerOffset,
                            std::initializer_list<int> rawValues)
{
    spec.condition.enabled = true;
    spec.condition.controller = FxAddress::relative(
        controllerBank, controllerOffset);
    for (int raw : rawValues)
        spec.condition.visibleRawValues.append(raw);
    return spec;
}

QString parameterLabel(const Midi &parameter,
                       const QString &overrideLabel)
{
    if (!overrideLabel.trimmed().isEmpty())
        return overrideLabel.trimmed();
    if (!parameter.customdesc.trimmed().isEmpty())
        return parameter.customdesc.trimmed();
    if (!parameter.desc.trimmed().isEmpty())
        return parameter.desc.trimmed();
    return parameter.name.trimmed();
}

QString enumLabel(const Midi &entry)
{
    if (!entry.name.trimmed().isEmpty())
        return entry.name.trimmed();
    if (!entry.customdesc.trimmed().isEmpty())
        return entry.customdesc.trimmed();
    return entry.desc.trimmed();
}

QVector<int> rhythmicRawValues(const Midi &parameter, bool twoByte)
{
    struct Entry {
        int raw;
        QString display;
    };

    QVector<Entry> entries;
    if (twoByte) {
        for (const Midi &highByte : parameter.level) {
            bool highOk = false;
            const int high = highByte.value.toInt(&highOk, 16);
            if (!highOk)
                continue;
            for (const Midi &lowByte : highByte.level) {
                if (lowByte.value == "range")
                    continue;
                bool recognized = false;
                const QString display =
                    FxPresentation::formatRhythmicDivision(
                        lowByte.name, &recognized);
                bool lowOk = false;
                const int low = lowByte.value.toInt(&lowOk, 16);
                if (recognized && lowOk)
                    entries.append({high * 128 + low, display});
            }
        }
    } else {
        for (const Midi &entry : parameter.level) {
            if (entry.value == "range")
                continue;
            bool recognized = false;
            const QString display =
                FxPresentation::formatRhythmicDivision(
                    entry.name, &recognized);
            bool rawOk = false;
            const int raw = entry.value.toInt(&rawOk, 16);
            if (recognized && rawOk)
                entries.append({raw, display});
        }
    }

    static const QStringList visualOrder = {
        "1/1", "1/1T",
        "1/2", "1/2D", "1/2T",
        "1/4", "1/4D", "1/4T",
        "1/8", "1/8D", "1/8T",
        "1/16", "1/16D"
    };
    std::stable_sort(entries.begin(), entries.end(),
        [](const Entry &left, const Entry &right) {
            const int leftIndex = visualOrder.indexOf(left.display);
            const int rightIndex = visualOrder.indexOf(right.display);
            return leftIndex < rightIndex;
        });

    QVector<int> values;
    values.reserve(entries.size());
    for (const Entry &entry : entries)
        values.append(entry.raw);
    return values;
}
}

namespace FxPresentation {
QString formatRhythmicDivision(const QString &value, bool *recognized)
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
}

FxAddress FxAddress::relative(int bank, const QString &offset,
                              bool twoByte)
{
    FxAddress address;
    address.scope = Scope::Relative;
    address.relativeBank = bank;
    address.offset = offset.toUpper();
    address.twoByte = twoByte;
    return address;
}

FxAddress FxAddress::external(const QString &bank,
                              const QString &middleByte,
                              const QString &offset,
                              bool twoByte)
{
    FxAddress address;
    address.scope = Scope::External;
    address.externalBank = bank.toUpper();
    address.middleByte = middleByte.toUpper();
    address.offset = offset.toUpper();
    address.twoByte = twoByte;
    return address;
}

ModernFxEditor::ModernFxEditor(FxSlot slot, QObject *parent)
    : QObject(parent), fxSlot(slot)
{
    buildEditor();
}

EffectEditorPanel *ModernFxEditor::widget() const
{
    return editor;
}

FxSlot ModernFxEditor::slot() const
{
    return fxSlot;
}

QString ModernFxEditor::translatedBank(int relativeBank) const
{
    if (relativeBank < 0 || relativeBank > 3)
        return QString();
    const int base = fxSlot == FxSlot::FX1 ? 0x02 : 0x06;
    return QString("%1").arg(base + relativeBank, 2, 16,
                             QChar('0')).toUpper();
}

void ModernFxEditor::buildEditor()
{
    const QString slotName = fxSlot == FxSlot::FX1 ? "FX-1" : "FX-2";
    const QColor accent(ModernTheme::activeEffectAccent(slotName));

    editor = new EffectEditorPanel(slotName);
    editor->typeLabel()->hide();
    editor->setRightPanelTitle(slotName + " TYPES");

    artwork = new EffectArtworkWidget;
    artwork->setArtwork(":/assets/effects/pedal_generic.png");
    artwork->setGenericPedalIdentity(
        slotName, QColor(ModernTheme::color(ModernTheme::PrimaryText)),
        QColor(ModernTheme::effectColor(slotName)));
    editor->setArtworkWidget(artwork);

    browser = new EffectModelBrowser;
    browser->setAccentColor(accent);
    browser->setCategoriesCollapsible(true);
    editor->setModelBrowserWidget(browser);

    QVBoxLayout *layout = new QVBoxLayout(editor->parameterArea());
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    QWidget *stateRow = new QWidget;
    stateRowLayout = new QHBoxLayout(stateRow);
    stateRowLayout->setContentsMargins(0, 0, 0, 0);
    stateRowLayout->setSpacing(0);
    EffectToggleControl *stateControl = new EffectToggleControl(tr("State"));
    stateToggle = stateControl->toggle();
    stateToggle->setAccentColor(accent);
    stateRowLayout->addWidget(stateControl, 0, Qt::AlignTop);
    stateRowLayout->addStretch(1);
    layout->addWidget(stateRow);

    ParameterCombo *hiddenTypeControl = new ParameterCombo(tr("Type"));
    hiddenType = hiddenTypeControl->comboBox();
    hiddenTypeControl->hide();
    layout->addWidget(hiddenTypeControl);

    buildTypes();
    buildPages();
    layout->addWidget(algorithmStack, 1);

    connect(browser, &EffectModelBrowser::modelSelected,
            this, [this](int browserIndex) {
        if (refreshing || browserIndex < 0
            || browserIndex >= browserRawValues.size())
            return;
        setFxType(browserRawValues.at(browserIndex), true);
    });
    connect(hiddenType,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int comboIndex) {
        if (refreshing || comboIndex < 0)
            return;
        bool rawOk = false;
        const int raw = hiddenType->itemData(comboIndex).toInt(&rawOk);
        if (rawOk)
            setFxType(raw, true);
    });
    connect(stateToggle, &QAbstractButton::clicked,
            this, [this](bool checked) {
        if (refreshing || !available)
            return;
        writeValue(FxAddress::relative(0, "00"), checked ? 1 : 0);
        if (artwork)
            artwork->setGenericPedalState(true, checked);
        emit stateChanged(true, checked);
    });

    updateControls(false);
}

void ModernFxEditor::addStateRowAction(QWidget *action)
{
    if (action && stateRowLayout)
        stateRowLayout->addWidget(action, 0, Qt::AlignTop);
}

void ModernFxEditor::buildTypes()
{
    const Midi typeMap = MidiTable::Instance()->getMidiMap(
        kStructure, translatedBank(0), "00", "01");
    for (const Midi &item : typeMap.level) {
        if (item.value == "range")
            continue;
        bool rawOk = false;
        const int raw = item.value.toInt(&rawOk, 16);
        if (!rawOk)
            continue;
        TypeEntry type;
        type.raw = raw;
        type.name = item.name.trimmed();
        types.append(type);
        hiddenType->addItem(type.name, raw);
    }

    const QStringList categoryOrder = {
        "FILTER / SYNTH",
        "DYNAMICS / TONE",
        "PITCH",
        "MODULATION",
        "SEQUENCE / UTILITY"
    };
    QStringList browserLabels;
    for (const QString &category : categoryOrder) {
        for (const TypeEntry &type : types) {
            if (categoryForRaw(type.raw) != category)
                continue;
            browserLabels.append(
                QString("(%1) %2").arg(category, type.name));
            browserRawValues.append(type.raw);
        }
    }
    browser->setModels(browserLabels);
}

QVector<ModernFxEditor::AlgorithmSpec>
ModernFxEditor::phaseOneAlgorithms() const
{
    QVector<AlgorithmSpec> specs;
    auto add = [&specs](int raw,
                        std::initializer_list<FxParameterSpec> parameters) {
        AlgorithmSpec algorithm;
        algorithm.raw = raw;
        for (const FxParameterSpec &spec : parameters)
            algorithm.parameters.append(spec);
        specs.append(algorithm);
    };

    add(0x00, {
        parameter(0, "0D", FxControlKind::Combo, "CHARACTER"),
        parameter(0, "0E", FxControlKind::Combo, "CHARACTER"),
        parameter(0, "0F", FxControlKind::Bar, "WAH"),
        parameter(0, "10", FxControlKind::Bar, "WAH"),
        parameter(0, "11", FxControlKind::Bar, "WAH"),
        parameter(0, "12", FxControlKind::Bar, "MIX"),
        parameter(0, "13", FxControlKind::Bar, "MIX")
    });
    add(0x01, {
        parameter(0, "14", FxControlKind::Combo, "CHARACTER"),
        parameter(0, "15", FxControlKind::Bar, "WAH"),
        parameter(0, "16", FxControlKind::Bar, "WAH"),
        parameter(0, "17", FxControlKind::SegmentedBar, "WAH", -1, 0x64),
        parameter(0, "18", FxControlKind::Bar, "WAH"),
        parameter(0, "19", FxControlKind::Bar, "MIX"),
        parameter(0, "1A", FxControlKind::Bar, "MIX")
    });
    add(0x03, {
        parameter(0, "02", FxControlKind::Combo, "COMPRESSOR"),
        parameter(0, "03", FxControlKind::Bar, "COMPRESSOR"),
        parameter(0, "04", FxControlKind::Bar, "COMPRESSOR"),
        parameter(0, "05", FxControlKind::BipolarBar, "COMPRESSOR", 0x32),
        parameter(0, "06", FxControlKind::Bar, "OUTPUT")
    });
    add(0x04, {
        parameter(0, "07", FxControlKind::Combo, "LIMITER"),
        parameter(0, "08", FxControlKind::Bar, "LIMITER"),
        parameter(0, "09", FxControlKind::Bar, "LIMITER"),
        parameter(0, "0A", FxControlKind::Combo, "LIMITER"),
        parameter(0, "0B", FxControlKind::Bar, "LIMITER"),
        parameter(0, "0C", FxControlKind::Bar, "OUTPUT")
    });
    add(0x07, {
        parameter(3, "1F", FxControlKind::Combo, "CHARACTER"),
        parameter(3, "20", FxControlKind::Bar, "TONE"),
        parameter(3, "21", FxControlKind::BipolarBar, "TONE", 0x32),
        parameter(3, "22", FxControlKind::BipolarBar, "TONE", 0x32),
        parameter(3, "23", FxControlKind::Bar, "OUTPUT")
    });
    add(0x08, {
        parameter(3, "24", FxControlKind::Combo, "CHARACTER"),
        parameter(3, "25", FxControlKind::BipolarBar, "TONE", 0x32),
        parameter(3, "26", FxControlKind::BipolarBar, "TONE", 0x32),
        parameter(3, "28", FxControlKind::Bar, "BODY"),
        parameter(3, "27", FxControlKind::Bar, "OUTPUT")
    });
    add(0x09, {
        parameter(0, "3E", FxControlKind::Bar, "ENVELOPE"),
        parameter(0, "3F", FxControlKind::Bar, "ENVELOPE")
    });
    add(0x0A, {
        parameter(1, "2D", FxControlKind::BipolarBar, "CHARACTER", 0x32),
        parameter(1, "2E", FxControlKind::Bar, "RESPONSE"),
        parameter(1, "2F", FxControlKind::Bar, "RESPONSE"),
        parameter(1, "30", FxControlKind::Bar, "RESPONSE"),
        parameter(1, "31", FxControlKind::Bar, "RESPONSE"),
        parameter(1, "32", FxControlKind::Bar, "MIX"),
        parameter(1, "33", FxControlKind::Bar, "MIX")
    });
    add(0x18, {
        parameter(0, "1B", FxControlKind::Bar, "MODULATION"),
        parameter(0, "1C", FxControlKind::SegmentedBar, "MODULATION", -1, 0x64),
        parameter(0, "1D", FxControlKind::Bar, "MODULATION")
    });
    add(0x1A, {
        parameter(0, "37", FxControlKind::SegmentedBar, "MODULATION", -1, 0x64),
        parameter(0, "38", FxControlKind::Bar, "MODULATION"),
        parameter(0, "39", FxControlKind::Bar, "OUTPUT")
    });
    add(0x1E, {
        parameter(0, "3A", FxControlKind::Combo, "CHARACTER"),
        parameter(0, "3B", FxControlKind::Bar, "RING MODULATOR"),
        parameter(0, "3C", FxControlKind::Bar, "MIX"),
        parameter(0, "3D", FxControlKind::Bar, "MIX")
    });
    return specs;
}

QVector<ModernFxEditor::AlgorithmSpec>
ModernFxEditor::phaseTwoAlgorithms() const
{
    QVector<AlgorithmSpec> specs;
    auto add = [&specs](int raw,
                        std::initializer_list<FxParameterSpec> parameters) {
        AlgorithmSpec algorithm;
        algorithm.raw = raw;
        for (const FxParameterSpec &spec : parameters)
            algorithm.parameters.append(spec);
        specs.append(algorithm);
    };

    add(0x02, {
        parameter(3, "30", FxControlKind::Combo, "CHARACTER"),
        parameter(3, "31", FxControlKind::Bar, "WAH"),
        parameter(3, "32", FxControlKind::Bar, "WAH"),
        parameter(3, "33", FxControlKind::Bar, "WAH"),
        parameter(3, "34", FxControlKind::Bar, "MIX"),
        parameter(3, "35", FxControlKind::Bar, "MIX")
    });
    add(0x0B, {
        parameter(1, "3B", FxControlKind::Combo, "CHARACTER"),
        parameter(1, "3C", FxControlKind::Bar, "FILTER"),
        parameter(1, "3D", FxControlKind::Bar, "FILTER"),
        parameter(1, "3E", FxControlKind::Bar, "FILTER"),
        parameter(1, "3F", FxControlKind::Bar, "FILTER"),
        parameter(1, "40", FxControlKind::Bar, "FILTER"),
        parameter(1, "41", FxControlKind::Bar, "MIX"),
        parameter(1, "42", FxControlKind::Bar, "MIX")
    });
    add(0x0D, {
        parameter(1, "34", FxControlKind::BipolarBar,
                  "CHARACTER", 0x32),
        parameter(1, "35", FxControlKind::Bar, "CHARACTER"),
        parameter(1, "36", FxControlKind::Bar, "CHARACTER"),
        parameter(1, "37", FxControlKind::Bar, "CHARACTER"),
        parameter(1, "38", FxControlKind::Bar, "CHARACTER"),
        parameter(1, "39", FxControlKind::Bar, "MIX"),
        parameter(1, "3A", FxControlKind::Bar, "MIX")
    });
    add(0x0E, {
        parameter(1, "15", FxControlKind::Combo, "RANGE"),
        parameter(1, "16", FxControlKind::Bar, "OCTAVE"),
        parameter(1, "17", FxControlKind::Bar, "MIX")
    });
    add(0x12, {
        parameter(3, "1C", FxControlKind::Toggle, "HOLD"),
        parameter(3, "1D", FxControlKind::Bar, "HOLD"),
        parameter(3, "1E", FxControlKind::Bar, "OUTPUT")
    });
    add(0x13, {
        parameter(3, "29", FxControlKind::Combo, "CHARACTER"),
        parameter(3, "2A", FxControlKind::BipolarBar,
                  "TONE", 0x32),
        parameter(3, "2B", FxControlKind::BipolarBar,
                  "TONE", 0x32),
        parameter(3, "2C", FxControlKind::Combo, "TONE"),
        parameter(3, "2D", FxControlKind::BipolarBar,
                  "TONE", 0x32),
        parameter(3, "2E", FxControlKind::BipolarBar,
                  "TONE", 0x32),
        parameter(3, "2F", FxControlKind::Bar, "OUTPUT")
    });
    add(0x15, {
        parameter(0, "47", FxControlKind::Bar, "FILTER 1",
                  -1, -1, "Frequency"),
        parameter(0, "48", FxControlKind::Bar, "FILTER 1",
                  -1, -1, "Depth"),
        parameter(0, "49", FxControlKind::Bar, "FILTER 2",
                  -1, -1, "Frequency"),
        parameter(0, "4A", FxControlKind::Bar, "FILTER 2",
                  -1, -1, "Depth"),
        parameter(0, "4B", FxControlKind::Bar, "FILTER 3",
                  -1, -1, "Frequency"),
        parameter(0, "4C", FxControlKind::Bar, "FILTER 3",
                  -1, -1, "Depth")
    });
    add(0x16, {
        parameter(0, "1E", FxControlKind::Combo, "CHARACTER"),
        parameter(0, "1F", FxControlKind::SegmentedBar, "MODULATION"),
        parameter(0, "20", FxControlKind::Bar, "MODULATION"),
        parameter(0, "21", FxControlKind::Bar, "MODULATION"),
        parameter(0, "22", FxControlKind::Bar, "MODULATION"),
        parameter(0, "23", FxControlKind::StepRateBar, "STEP"),
        parameter(0, "24", FxControlKind::Bar, "MIX"),
        parameter(0, "25", FxControlKind::Bar, "MIX")
    });
    add(0x17, {
        parameter(0, "26", FxControlKind::SegmentedBar, "MODULATION"),
        parameter(0, "27", FxControlKind::Bar, "MODULATION"),
        parameter(0, "28", FxControlKind::Bar, "MODULATION"),
        parameter(0, "29", FxControlKind::Bar, "MODULATION"),
        parameter(0, "2A", FxControlKind::Bar, "STEREO"),
        parameter(0, "2B", FxControlKind::Combo, "FILTER"),
        parameter(0, "2C", FxControlKind::Bar, "MIX"),
        parameter(0, "2D", FxControlKind::Bar, "MIX")
    });
    add(0x19, {
        parameter(1, "18", FxControlKind::Combo, "SPEED"),
        parameter(1, "19", FxControlKind::SegmentedBar,
                  "SLOW", -1, -1, "Rate"),
        parameter(1, "1A", FxControlKind::SegmentedBar,
                  "FAST", -1, -1, "Rate"),
        parameter(1, "1B", FxControlKind::Bar, "TRANSITION"),
        parameter(1, "1C", FxControlKind::Bar, "TRANSITION"),
        parameter(1, "1D", FxControlKind::Bar, "MODULATION")
    });
    add(0x1B, {
        parameter(0, "2E", FxControlKind::Combo, "CHARACTER"),
        parameter(0, "2F", FxControlKind::MappedBar,
                  "POSITION", 0x32),
        parameter(0, "30", FxControlKind::Bar, "MODULATION"),
        parameter(0, "31", FxControlKind::SegmentedBar, "MODULATION"),
        parameter(0, "32", FxControlKind::Bar, "MODULATION")
    });
    add(0x1C, {
        parameter(0, "55", FxControlKind::Combo, "PATTERN"),
        parameter(0, "56", FxControlKind::SegmentedBar, "SEQUENCE"),
        parameter(0, "57", FxControlKind::Bar, "SEQUENCE")
    });
    add(0x1D, {
        parameter(0, "33", FxControlKind::SegmentedBar, "MODULATION"),
        parameter(0, "34", FxControlKind::Bar, "MODULATION"),
        parameter(0, "35", FxControlKind::Toggle, "TRIGGER"),
        parameter(0, "36", FxControlKind::Bar, "TRIGGER")
    });
    add(0x1F, {
        parameter(0, "4D", FxControlKind::Combo, "CHARACTER"),
        parameter(0, "4E", FxControlKind::Combo, "VOWELS"),
        parameter(0, "4F", FxControlKind::Combo, "VOWELS"),
        parameter(0, "50", FxControlKind::Bar, "RESPONSE"),
        parameter(0, "51", FxControlKind::SegmentedBar, "RESPONSE"),
        parameter(0, "52", FxControlKind::Bar, "RESPONSE"),
        parameter(0, "53", FxControlKind::Bar, "RESPONSE"),
        parameter(0, "54", FxControlKind::Bar, "OUTPUT")
    });
    add(0x20, {
        parameter(1, "1E", FxControlKind::Combo, "CROSSOVER"),
        parameter(1, "1F", FxControlKind::SegmentedBar,
                  "LOW", -1, -1, "Rate"),
        parameter(1, "20", FxControlKind::Bar,
                  "LOW", -1, -1, "Depth"),
        parameter(1, "21", FxControlKind::Bar,
                  "LOW", -1, -1, "Pre Delay"),
        parameter(1, "22", FxControlKind::Bar,
                  "LOW", -1, -1, "Level"),
        parameter(1, "23", FxControlKind::SegmentedBar,
                  "HIGH", -1, -1, "Rate"),
        parameter(1, "24", FxControlKind::Bar,
                  "HIGH", -1, -1, "Depth"),
        parameter(1, "25", FxControlKind::Bar,
                  "HIGH", -1, -1, "Pre Delay"),
        parameter(1, "26", FxControlKind::Bar,
                  "HIGH", -1, -1, "Level")
    });
    specs.last().sideBySideSections = QStringList() << "LOW" << "HIGH";
    add(0x21, {
        parameter(1, "27", FxControlKind::TwoByteSegmentedBar, "TIME"),
        parameter(1, "29", FxControlKind::Bar, "REPEATS"),
        parameter(1, "2A", FxControlKind::Combo, "FILTER"),
        parameter(1, "2B", FxControlKind::Bar, "MIX"),
        parameter(1, "2C", FxControlKind::Bar, "MIX")
    });
    return specs;
}

QVector<ModernFxEditor::AlgorithmSpec>
ModernFxEditor::phaseThreeAAlgorithms() const
{
    QVector<AlgorithmSpec> specs;
    auto add = [&specs](int raw,
                        std::initializer_list<FxParameterSpec> parameters) {
        AlgorithmSpec algorithm;
        algorithm.raw = raw;
        for (const FxParameterSpec &spec : parameters)
            algorithm.parameters.append(spec);
        specs.append(algorithm);
    };

    add(0x05, {
        parameter(3, "37", FxControlKind::BipolarBar,
                  "LOW BANDS", 0x0C, -1, "31 Hz"),
        parameter(3, "38", FxControlKind::BipolarBar,
                  "LOW BANDS", 0x0C, -1, "62 Hz"),
        parameter(3, "39", FxControlKind::BipolarBar,
                  "LOW BANDS", 0x0C, -1, "125 Hz"),
        parameter(3, "3A", FxControlKind::BipolarBar,
                  "LOW BANDS", 0x0C, -1, "250 Hz"),
        parameter(3, "3B", FxControlKind::BipolarBar,
                  "LOW BANDS", 0x0C, -1, "500 Hz"),
        parameter(3, "3C", FxControlKind::BipolarBar,
                  "HIGH BANDS", 0x0C, -1, "1 kHz"),
        parameter(3, "3D", FxControlKind::BipolarBar,
                  "HIGH BANDS", 0x0C, -1, "2 kHz"),
        parameter(3, "3E", FxControlKind::BipolarBar,
                  "HIGH BANDS", 0x0C, -1, "4 kHz"),
        parameter(3, "3F", FxControlKind::BipolarBar,
                  "HIGH BANDS", 0x0C, -1, "8 kHz"),
        parameter(3, "40", FxControlKind::BipolarBar,
                  "HIGH BANDS", 0x0C, -1, "16 kHz"),
        parameter(3, "36", FxControlKind::BipolarBar,
                  "OUTPUT", 0x0C, -1, "Level")
    });
    specs.last().sideBySideSections =
        QStringList() << "LOW BANDS" << "HIGH BANDS";

    add(0x06, {
        parameter(0, "58", FxControlKind::Combo, "LOW",
                  -1, -1, "Low Cut"),
        parameter(0, "59", FxControlKind::BipolarBar,
                  "LOW", 0x14, -1, "Low Gain"),
        parameter(0, "5A", FxControlKind::Combo, "LOW-MID",
                  -1, -1, "Frequency"),
        parameter(0, "5B", FxControlKind::Combo, "LOW-MID",
                  -1, -1, "Q"),
        parameter(0, "5C", FxControlKind::BipolarBar,
                  "LOW-MID", 0x14, -1, "Gain"),
        parameter(0, "5D", FxControlKind::Combo, "HIGH-MID",
                  -1, -1, "Frequency"),
        parameter(0, "5E", FxControlKind::Combo, "HIGH-MID",
                  -1, -1, "Q"),
        parameter(0, "5F", FxControlKind::BipolarBar,
                  "HIGH-MID", 0x14, -1, "Gain"),
        parameter(0, "60", FxControlKind::BipolarBar,
                  "HIGH", 0x14, -1, "High Gain"),
        parameter(0, "61", FxControlKind::Combo, "HIGH",
                  -1, -1, "High Cut"),
        parameter(0, "62", FxControlKind::BipolarBar,
                  "OUTPUT", 0x14, -1, "Effect")
    });

    add(0x14, {
        parameter(0, "40", FxControlKind::Combo, "MODE"),
        parameter(0, "41", FxControlKind::Bar, "FEEDBACK"),
        visibleWhen(parameter(0, "43", FxControlKind::Bar,
                              "OSCILLATOR"), 0, "40", {0x00}),
        visibleWhen(parameter(0, "42", FxControlKind::Bar,
                              "OSCILLATOR"), 0, "40", {0x00}),
        visibleWhen(parameter(0, "44", FxControlKind::Bar,
                              "OSCILLATOR"), 0, "40", {0x00}),
        visibleWhen(parameter(0, "45", FxControlKind::SegmentedBar,
                              "VIBRATO"), 0, "40", {0x00}),
        visibleWhen(parameter(0, "46", FxControlKind::Bar,
                              "VIBRATO"), 0, "40", {0x00})
    });
    return specs;
}

QVector<ModernFxEditor::AlgorithmSpec>
ModernFxEditor::phaseThreeB1Algorithms() const
{
    QVector<AlgorithmSpec> specs;
    auto add = [&specs](int raw,
                        std::initializer_list<FxParameterSpec> parameters) {
        AlgorithmSpec algorithm;
        algorithm.raw = raw;
        for (const FxParameterSpec &spec : parameters)
            algorithm.parameters.append(spec);
        specs.append(algorithm);
    };

    add(0x0C, {
        parameter(1, "43", FxControlKind::Combo, "SOURCE", -1, -1,
                  "Wave"),
        parameter(1, "44", FxControlKind::Bar, "SOURCE", -1, -1,
                  "Sensitivity"),
        visibleWhen(parameter(1, "45", FxControlKind::Toggle,
                              "SOURCE", -1, -1, "Chromatic"),
                    1, "43", {0x00, 0x01}),
        visibleWhen(parameter(1, "46", FxControlKind::Combo,
                              "SOURCE", -1, -1, "Oct Shift"),
                    1, "43", {0x00, 0x01}),

        visibleWhen(parameter(1, "47", FxControlKind::Bar,
                              "PWM", -1, -1, "Rate"),
                    1, "43", {0x00}),
        visibleWhen(parameter(1, "48", FxControlKind::Bar,
                              "PWM", -1, -1, "Depth"),
                    1, "43", {0x00}),

        parameter(1, "49", FxControlKind::Bar, "FILTER", -1, -1,
                  "Cut Freq"),
        parameter(1, "4A", FxControlKind::Bar, "FILTER", -1, -1,
                  "Resonance"),
        parameter(1, "4B", FxControlKind::Bar, "FILTER", -1, -1,
                  "Sensitivity"),
        parameter(1, "4C", FxControlKind::Bar, "FILTER", -1, -1,
                  "Decay"),
        parameter(1, "4D", FxControlKind::BipolarBar,
                  "FILTER", 0x32, -1, "Depth"),

        parameter(1, "4E", FxControlKind::ZeroChoiceBar,
                  "ENVELOPE", -1, -1, "Attack"),
        parameter(1, "4F", FxControlKind::Bar, "ENVELOPE", -1, -1,
                  "Release"),
        parameter(1, "50", FxControlKind::Bar, "ENVELOPE", -1, -1,
                  "Velocity"),

        parameter(1, "52", FxControlKind::Bar, "OUTPUT", -1, -1,
                  "Synth Level"),
        parameter(1, "53", FxControlKind::Bar, "OUTPUT", -1, -1,
                  "Direct Level"),
        visibleWhen(parameter(1, "51", FxControlKind::Toggle,
                              "OUTPUT", -1, -1, "Hold"),
                    1, "43", {0x00, 0x01})
    });

    add(0x11, {
        parameter(1, "54", FxControlKind::Combo, "PHRASE", -1, -1,
                  "Phrase"),
        parameter(1, "55", FxControlKind::Toggle, "PHRASE", -1, -1,
                  "Loop"),
        parameter(1, "56", FxControlKind::SegmentedBar,
                  "TIMING", -1, 0x64, "Tempo"),
        parameter(1, "57", FxControlKind::Bar, "DYNAMICS", -1, -1,
                  "Sensitivity"),
        parameter(1, "58", FxControlKind::Bar, "DYNAMICS", -1, -1,
                  "Attack"),
        parameter(1, "59", FxControlKind::Toggle, "CONTROL", -1, -1,
                  "Hold"),
        parameter(1, "5A", FxControlKind::Bar, "MIX", -1, -1,
                  "Effect"),
        parameter(1, "5B", FxControlKind::Bar, "MIX", -1, -1,
                  "Direct")
    });

    return specs;
}

QVector<ModernFxEditor::AlgorithmSpec>
ModernFxEditor::phaseThreeB2Algorithms() const
{
    QVector<AlgorithmSpec> specs;
    auto add = [&specs](int raw,
                        std::initializer_list<FxParameterSpec> parameters) {
        AlgorithmSpec algorithm;
        algorithm.raw = raw;
        for (const FxParameterSpec &spec : parameters)
            algorithm.parameters.append(spec);
        specs.append(algorithm);
    };

    add(0x0F, {
        parameter(1, "06", FxControlKind::Combo, "VOICES", -1, -1,
                  "Voice Mode"),

        parameter(1, "07", FxControlKind::Combo, "VOICE 1", -1, -1,
                  "Mode"),
        parameter(1, "08", FxControlKind::BipolarBar,
                  "VOICE 1", 0x18, -1, "Pitch"),
        parameter(1, "09", FxControlKind::BipolarBar,
                  "VOICE 1", 0x32, -1, "Fine"),
        parameter(1, "0A", FxControlKind::TwoByteSegmentedBar,
                  "VOICE 1", -1, 300, "Pre Delay"),
        parameter(1, "0C", FxControlKind::Bar,
                  "VOICE 1", -1, -1, "Level"),

        visibleWhen(parameter(1, "0D", FxControlKind::Combo,
                              "VOICE 2", -1, -1, "Mode"),
                    1, "06", {0x01, 0x02}),
        visibleWhen(parameter(1, "0E", FxControlKind::BipolarBar,
                              "VOICE 2", 0x18, -1, "Pitch"),
                    1, "06", {0x01, 0x02}),
        visibleWhen(parameter(1, "0F", FxControlKind::BipolarBar,
                              "VOICE 2", 0x32, -1, "Fine"),
                    1, "06", {0x01, 0x02}),
        visibleWhen(parameter(1, "10",
                              FxControlKind::TwoByteSegmentedBar,
                              "VOICE 2", -1, 300, "Pre Delay"),
                    1, "06", {0x01, 0x02}),
        visibleWhen(parameter(1, "12", FxControlKind::Bar,
                              "VOICE 2", -1, -1, "Level"),
                    1, "06", {0x01, 0x02}),

        parameter(1, "13", FxControlKind::Bar,
                  "MIX", -1, -1, "Feedback"),
        parameter(1, "14", FxControlKind::Bar,
                  "MIX", -1, -1, "Direct")
    });

    add(0x10, {
        parameter(0, "63", FxControlKind::Combo, "VOICES", -1, -1,
                  "Voice Mode"),
        externalParameter("0A", "00", "68", FxControlKind::Combo,
                          "VOICES", "Master Key"),

        parameter(0, "64", FxControlKind::Combo,
                  "VOICE 1", -1, -1, "Harmony"),
        parameter(0, "65", FxControlKind::TwoByteSegmentedBar,
                  "VOICE 1", -1, 300, "Pre Delay"),
        parameter(0, "67", FxControlKind::Bar,
                  "VOICE 1", -1, -1, "Level"),

        visibleWhen(parameter(0, "68", FxControlKind::Combo,
                              "VOICE 2", -1, -1, "Harmony"),
                    0, "63", {0x01, 0x02}),
        visibleWhen(parameter(0, "69",
                              FxControlKind::TwoByteSegmentedBar,
                              "VOICE 2", -1, 300, "Pre Delay"),
                    0, "63", {0x01, 0x02}),
        visibleWhen(parameter(0, "6B", FxControlKind::Bar,
                              "VOICE 2", -1, -1, "Level"),
                    0, "63", {0x01, 0x02}),

        parameter(0, "6C", FxControlKind::Bar,
                  "MIX", -1, -1, "Feedback"),
        parameter(0, "6D", FxControlKind::Bar,
                  "MIX", -1, -1, "Direct")
    });

    static const QStringList scaleNotes = {
        "C", "C#", "D", "D#", "E", "F",
        "F#", "G", "G#", "A", "A#", "B"
    };
    for (int note = 0; note < scaleNotes.size(); ++note) {
        specs.last().parameters.append(parameter(
            0, QString("%1").arg(0x6E + note, 2, 16, QChar('0')),
            FxControlKind::Combo, "USER SCALE 1", -1, -1,
            scaleNotes.at(note)));
    }
    for (int note = 0; note < scaleNotes.size(); ++note) {
        const int bank = note < 6 ? 0 : 1;
        const int offset = note < 6 ? 0x7A + note : note - 6;
        specs.last().parameters.append(parameter(
            bank, QString("%1").arg(offset, 2, 16, QChar('0')),
            FxControlKind::Combo, "USER SCALE 2", -1, -1,
            scaleNotes.at(note)));
    }

    return specs;
}

void ModernFxEditor::buildPages()
{
    algorithmStack = new QStackedWidget;
    QVector<AlgorithmSpec> implemented = phaseOneAlgorithms();
    implemented += phaseTwoAlgorithms();
    implemented += phaseThreeAAlgorithms();
    implemented += phaseThreeB1Algorithms();
    implemented += phaseThreeB2Algorithms();
    for (const TypeEntry &type : types) {
        const AlgorithmSpec *algorithm = nullptr;
        for (const AlgorithmSpec &candidate : implemented) {
            if (candidate.raw == type.raw) {
                algorithm = &candidate;
                break;
            }
        }
        algorithmStack->addWidget(createAlgorithmPage(type, algorithm));
        pageRawValues.append(type.raw);
    }
}

QWidget *ModernFxEditor::createAlgorithmPage(
    const TypeEntry &type, const AlgorithmSpec *spec)
{
    if (!spec)
        return createPlaceholderPage(type.name);

    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(7);

    if (spec->sideBySideSections.isEmpty()) {
        QString currentSection;
        for (const FxParameterSpec &parameterSpec : spec->parameters) {
            if (parameterSpec.section != currentSection) {
                currentSection = parameterSpec.section;
                QLabel *section = new QLabel(currentSection);
                section->setObjectName("ParameterSectionTitle");
                layout->addWidget(section);
                if (parameterSpec.condition.enabled) {
                    ControlBinding sectionBinding;
                    sectionBinding.spec.condition =
                        parameterSpec.condition;
                    sectionBinding.container = section;
                    sectionBinding.algorithmRaw = spec->raw;
                    sectionBinding.visualOnly = true;
                    controls.append(sectionBinding);
                }
            }
            layout->addWidget(createParameterControl(
                parameterSpec, spec->raw));
        }
    } else {
        QString currentSection;
        for (const FxParameterSpec &parameterSpec : spec->parameters) {
            if (spec->sideBySideSections.contains(parameterSpec.section))
                continue;
            if (parameterSpec.section != currentSection) {
                currentSection = parameterSpec.section;
                QLabel *section = new QLabel(currentSection);
                section->setObjectName("ParameterSectionTitle");
                layout->addWidget(section);
            }
            layout->addWidget(createParameterControl(
                parameterSpec, spec->raw));
        }

        ResponsiveSectionColumns *columns =
            new ResponsiveSectionColumns;
        for (const QString &sectionName : spec->sideBySideSections) {
            QWidget *column = new QWidget;
            QVBoxLayout *columnLayout = new QVBoxLayout(column);
            columnLayout->setContentsMargins(0, 0, 0, 0);
            columnLayout->setSpacing(7);
            QLabel *section = new QLabel(sectionName);
            section->setObjectName("ParameterSectionTitle");
            columnLayout->addWidget(section);
            for (const FxParameterSpec &parameterSpec : spec->parameters) {
                if (parameterSpec.section == sectionName) {
                    columnLayout->addWidget(createParameterControl(
                        parameterSpec, spec->raw));
                }
            }
            columnLayout->addStretch(1);
            columns->addSection(column);
        }
        layout->addWidget(columns);
    }
    layout->addStretch(1);
    return page;
}

QWidget *ModernFxEditor::createPlaceholderPage(
    const QString &algorithmName)
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 24, 0, 0);
    layout->setSpacing(8);

    QLabel *name = new QLabel(algorithmName.trimmed().toUpper());
    name->setObjectName("ParameterSectionTitle");
    name->setAlignment(Qt::AlignCenter);
    QLabel *pending = new QLabel(tr("Modern editor migration pending"));
    pending->setObjectName("WorkspaceUnavailable");
    pending->setAlignment(Qt::AlignCenter);
    layout->addStretch(1);
    layout->addWidget(name);
    layout->addWidget(pending);
    layout->addStretch(2);
    return page;
}

QWidget *ModernFxEditor::createParameterControl(
    const FxParameterSpec &spec, int algorithmRaw)
{
    const QString bank = bankForAddress(spec.address);
    const Midi parameterMap = MidiTable::Instance()->getMidiMap(
        kStructure, bank, spec.address.middleByte,
        spec.address.offset);
    const QString label = parameterLabel(parameterMap,
                                         spec.labelOverride);
    ControlBinding binding;
    binding.spec = spec;
    binding.algorithmRaw = algorithmRaw;

    if (spec.kind == FxControlKind::Combo) {
        ParameterCombo *control = new ParameterCombo(label);
        QComboBox *combo = control->comboBox();
        for (const Midi &entry : parameterMap.level) {
            if (entry.value == "range")
                continue;
            bool rawOk = false;
            const int raw = entry.value.toInt(&rawOk, 16);
            if (rawOk)
                combo->addItem(enumLabel(entry), raw);
        }
        connect(combo,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this, combo, spec](int index) {
            if (refreshing || !available || index < 0)
                return;
            bool rawOk = false;
            const int raw = combo->itemData(index).toInt(&rawOk);
            if (rawOk)
                writeValue(spec.address, raw);
            applyConditionalRules();
        });
        binding.container = control;
        binding.combo = combo;
    } else if (spec.kind == FxControlKind::Toggle) {
        EffectToggleControl *control = new EffectToggleControl(label);
        ModernToggleSwitch *toggle = control->toggle();
        toggle->setAccentColor(QColor(ModernTheme::activeEffectAccent(
            fxSlot == FxSlot::FX1 ? "FX-1" : "FX-2")));
        connect(toggle, &QAbstractButton::clicked,
                this, [this, spec](bool checked) {
            if (!refreshing && available)
                writeValue(spec.address, checked ? 1 : 0);
            applyConditionalRules();
        });
        binding.container = control;
        binding.toggle = toggle;
    } else if (spec.kind == FxControlKind::StepRateBar
               || spec.kind == FxControlKind::ZeroChoiceBar) {
        QWidget *control = new QWidget;
        QHBoxLayout *controlLayout = new QHBoxLayout(control);
        controlLayout->setContentsMargins(0, 0, 0, 0);
        controlLayout->setSpacing(8);

        const QString zeroDisplay = displayValue(spec.address, 0);
        QPushButton *off = new QPushButton(zeroDisplay.toUpper());
        off->setCheckable(true);
        off->setCursor(Qt::PointingHandCursor);
        off->setFixedSize(qMax(54, zeroDisplay.size() * 9 + 18), 30);
        const QColor accent(ModernTheme::activeEffectAccent(
            fxSlot == FxSlot::FX1 ? "FX-1" : "FX-2"));
        off->setStyleSheet(QString(
            "QPushButton{color:%1;background:%2;border:1px solid %3;"
            "border-radius:5px;font-size:10px;font-weight:700;}"
            "QPushButton:hover{border-color:%4;}"
            "QPushButton:checked{color:%5;background:%6;"
            "border-color:%7;}")
            .arg(ModernTheme::color(ModernTheme::SecondaryText),
                 ModernTheme::color(ModernTheme::ControlBackground),
                 ModernTheme::color(ModernTheme::Border),
                 accent.name(),
                 ModernTheme::color(ModernTheme::PrimaryText),
                 accent.darker(185).name(),
                 accent.name()));

        ParameterBar *bar = new ParameterBar(label);
        bar->setAccentColor(accent);
        MidiTable *midiTable = MidiTable::Instance();
        const int maximum = midiTable->getRange(
            kStructure, bank, spec.address.middleByte,
            spec.address.offset);
        bar->setRange(1, maximum);
        if (spec.kind == FxControlKind::StepRateBar) {
            const QVector<int> rhythmicValues = rhythmicRawValues(
                parameterMap, false);
            const int continuousMaximum = rhythmicValues.isEmpty()
                ? maximum : *std::min_element(
                    rhythmicValues.constBegin(),
                    rhythmicValues.constEnd()) - 1;
            if (!rhythmicValues.isEmpty())
                bar->setSegmentedMapping(
                    continuousMaximum, rhythmicValues, 0.52);
        }

        connect(off, &QAbstractButton::clicked,
                this, [this, bar, spec](bool checked) {
            if (refreshing || !available)
                return;
            if (checked) {
                writeValue(spec.address, 0);
                bar->setDisplayText(displayValue(spec.address, 0));
            } else {
                writeValue(spec.address, bar->value());
                bar->setDisplayText(displayValue(
                    spec.address, bar->value()));
            }
        });
        connect(bar, &QAbstractSlider::sliderPressed,
                this, [this, off, bar, spec]() {
            if (refreshing || !available || !off->isChecked())
                return;
            const QSignalBlocker blocker(off);
            off->setChecked(false);
            writeValue(spec.address, bar->value());
            bar->setDisplayText(displayValue(
                spec.address, bar->value()));
        });
        connect(bar, &QAbstractSlider::valueChanged,
                this, [this, off, bar, spec](int raw) {
            if (refreshing || !available)
                return;
            {
                const QSignalBlocker blocker(off);
                off->setChecked(false);
            }
            writeValue(spec.address, raw);
            bar->setDisplayText(displayValue(spec.address, raw));
        });

        controlLayout->addWidget(off, 0, Qt::AlignBottom);
        controlLayout->addWidget(bar, 1);
        binding.container = control;
        binding.bar = bar;
        binding.offButton = off;
    } else {
        ParameterBar *bar = new ParameterBar(label);
        bar->setAccentColor(QColor(ModernTheme::activeEffectAccent(
            fxSlot == FxSlot::FX1 ? "FX-1" : "FX-2")));
        MidiTable *midiTable = MidiTable::Instance();
        bar->setRange(midiTable->getRangeMinimum(
                          kStructure, bank,
                          spec.address.middleByte,
                          spec.address.offset),
                      midiTable->getRange(
                          kStructure, bank,
                          spec.address.middleByte,
                          spec.address.offset));
        if (spec.kind == FxControlKind::BipolarBar
            || spec.kind == FxControlKind::MappedBar) {
            bar->setCenterValue(spec.centerRaw);
        }
        if (spec.kind == FxControlKind::SegmentedBar
            || spec.kind == FxControlKind::TwoByteSegmentedBar) {
            const QVector<int> rhythmicValues = rhythmicRawValues(
                parameterMap, spec.address.twoByte);
            int continuousMaximum = spec.continuousMaximum;
            if (continuousMaximum < 0 && !rhythmicValues.isEmpty())
                continuousMaximum = *std::min_element(
                    rhythmicValues.constBegin(),
                    rhythmicValues.constEnd()) - 1;
            if (!rhythmicValues.isEmpty())
                bar->setSegmentedMapping(
                    continuousMaximum, rhythmicValues, 0.5);
        }
        connect(bar, &QAbstractSlider::valueChanged,
                this, [this, bar, spec](int raw) {
            if (refreshing || !available)
                return;
            writeValue(spec.address, raw);
            bar->setDisplayText(displayValue(spec.address, raw));
            applyConditionalRules();
        });
        binding.container = bar;
        binding.bar = bar;
    }

    controls.append(binding);
    return binding.container;
}

QString ModernFxEditor::bankForAddress(const FxAddress &address) const
{
    return address.scope == FxAddress::Scope::External
        ? address.externalBank : translatedBank(address.relativeBank);
}

bool ModernFxEditor::bufferContains(const FxAddress &address) const
{
    bool offsetOk = false;
    const int offset = address.offset.toInt(&offsetOk, 16);
    if (!offsetOk)
        return false;

    const SysxData source = SysxIO::Instance()->getFileSource();
    const QString sourceAddress = bankForAddress(address)
        + address.middleByte;
    const int addressIndex = source.address.indexOf(sourceAddress);
    if (addressIndex < 0 || addressIndex >= source.hex.size())
        return false;
    const int lastIndex = sysxDataOffset + offset
        + (address.twoByte ? 1 : 0);
    return lastIndex >= 0
        && lastIndex < source.hex.at(addressIndex).size();
}

bool ModernFxEditor::hasValidBuffer(bool backendConnected,
                                    bool backendHasPatchData) const
{
    SysxIO *sysxIO = SysxIO::Instance();
    if (!backendConnected || !backendHasPatchData
        || !sysxIO->isConnected())
        return false;

    // STATE and TYPE define whether the FX block itself is available.
    // Algorithm parameters are validated individually by the active page;
    // a missing address in an unrelated bank must not disable navigation.
    return bufferContains(FxAddress::relative(0, "00"))
        && bufferContains(FxAddress::relative(0, "01"));
}

int ModernFxEditor::readValue(const FxAddress &address) const
{
    return SysxIO::Instance()->getSourceValue(
        kStructure, bankForAddress(address), address.middleByte,
        address.offset);
}

void ModernFxEditor::writeValue(const FxAddress &address, int raw)
{
    if (!available || !bufferContains(address))
        return;

    SysxIO *sysxIO = SysxIO::Instance();
    const QString bank = bankForAddress(address);
    if (address.twoByte) {
        const QString high = QString("%1").arg(
            raw / 128, 2, 16, QChar('0')).toUpper();
        const QString low = QString("%1").arg(
            raw % 128, 2, 16, QChar('0')).toUpper();
        sysxIO->setFileSource(kStructure, bank, address.middleByte,
                              address.offset, high, low);
    } else {
        const QString value = QString("%1").arg(
            raw, 2, 16, QChar('0')).toUpper();
        sysxIO->setFileSource(kStructure, bank, address.middleByte,
                              address.offset, value);
    }
}

QString ModernFxEditor::displayValue(const FxAddress &address,
                                     int raw) const
{
    const QString value = MidiTable::Instance()->getValue(
        kStructure, bankForAddress(address), address.middleByte,
        address.offset, QString::number(raw, 16).toUpper());
    return FxPresentation::formatRhythmicDivision(value);
}

void ModernFxEditor::setFxType(int raw, bool writeBackend)
{
    const int pageIndex = pageRawValues.indexOf(raw);
    if (pageIndex < 0)
        return;

    if (writeBackend)
        writeValue(FxAddress::relative(0, "01"), raw);

    if (hiddenType) {
        const QSignalBlocker blocker(hiddenType);
        hiddenType->setCurrentIndex(hiddenType->findData(raw));
    }
    if (algorithmStack)
        algorithmStack->setCurrentIndex(pageIndex);
    updateBrowserForRaw(raw);

    const QString typeName = typeNameForRaw(raw);
    if (editor && editor->typeLabel())
        editor->typeLabel()->setText(typeName);
    if (artwork)
        artwork->setTextOverlayText("type", typeName.toUpper());
    if (available)
        refreshControlsForType(raw);
    applyConditionalRules();
}

void ModernFxEditor::updateBrowserForRaw(int raw)
{
    if (browser)
        browser->setCurrentIndex(browserRawValues.indexOf(raw));
}

void ModernFxEditor::updateControls(bool controlsAvailable)
{
    available = controlsAvailable;
    if (browser)
        browser->setEnabled(controlsAvailable);
    if (stateToggle)
        stateToggle->setEnabled(controlsAvailable);
    if (hiddenType)
        hiddenType->setEnabled(controlsAvailable);

    if (!controlsAvailable) {
        if (artwork)
            artwork->setGenericPedalState(false, false);
        if (stateToggle) {
            const QSignalBlocker blocker(stateToggle);
            stateToggle->setChecked(false);
        }
        if (hiddenType) {
            const QSignalBlocker blocker(hiddenType);
            hiddenType->setCurrentIndex(-1);
        }
        if (browser)
            browser->setCurrentIndex(-1);
        if (artwork)
            artwork->setTextOverlayText("type", QString::fromUtf8("—"));
    }

    for (ControlBinding &binding : controls) {
        if (binding.bar) {
            binding.bar->setEnabled(controlsAvailable);
            if (!controlsAvailable)
                binding.bar->setDisplayText(QString::fromUtf8("—"));
        }
        if (binding.combo) {
            binding.combo->setEnabled(controlsAvailable);
            if (!controlsAvailable) {
                const QSignalBlocker blocker(binding.combo);
                binding.combo->setCurrentIndex(-1);
            }
        }
        if (binding.toggle) {
            binding.toggle->setEnabled(controlsAvailable);
            if (!controlsAvailable) {
                const QSignalBlocker blocker(binding.toggle);
                binding.toggle->setChecked(false);
            }
        }
        if (binding.offButton) {
            binding.offButton->setEnabled(controlsAvailable);
            if (!controlsAvailable) {
                const QSignalBlocker blocker(binding.offButton);
                binding.offButton->setChecked(false);
            }
        }
    }
}

void ModernFxEditor::refreshControlsForType(int rawType)
{
    for (ControlBinding &binding : controls) {
        if (binding.algorithmRaw != rawType)
            continue;
        if (binding.visualOnly)
            continue;
        if (!bufferContains(binding.spec.address)) {
            if (binding.container)
                binding.container->setEnabled(false);
            continue;
        }

        if (binding.container)
            binding.container->setEnabled(available);
        const int raw = readValue(binding.spec.address);
        if (binding.bar) {
            const bool isOff = binding.offButton && raw == 0;
            {
                const QSignalBlocker blocker(binding.bar);
                binding.bar->setValue(isOff
                    ? binding.bar->minimum() : raw);
                binding.bar->setDisplayText(isOff
                    ? displayValue(binding.spec.address, 0)
                    : displayValue(binding.spec.address, raw));
            }
            if (binding.offButton) {
                const QSignalBlocker blocker(binding.offButton);
                binding.offButton->setChecked(isOff);
            }
        } else if (binding.combo) {
            const QSignalBlocker blocker(binding.combo);
            binding.combo->setCurrentIndex(
                binding.combo->findData(raw));
        } else if (binding.toggle) {
            const QSignalBlocker blocker(binding.toggle);
            binding.toggle->setChecked(raw == 1);
        }
    }
}

void ModernFxEditor::refreshFx(bool backendConnected,
                               bool backendHasPatchData)
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

    const bool on = readValue(FxAddress::relative(0, "00")) == 1;
    if (stateToggle) {
        const QSignalBlocker blocker(stateToggle);
        stateToggle->setChecked(on);
    }
    if (artwork)
        artwork->setGenericPedalState(true, on);

    const int typeRaw = readValue(FxAddress::relative(0, "01"));
    setFxType(typeRaw, false);
    applyConditionalRules();
    refreshing = false;
    emit stateChanged(true, on);
}

void ModernFxEditor::applyConditionalRules()
{
    for (ControlBinding &binding : controls) {
        if (!binding.container || !binding.spec.condition.enabled)
            continue;
        const FxCondition &condition = binding.spec.condition;
        const bool visible = bufferContains(condition.controller)
            && condition.visibleRawValues.contains(
                readValue(condition.controller));
        binding.container->setVisible(visible);
    }
}

QString ModernFxEditor::categoryForRaw(int raw) const
{
    static const QSet<int> filterSynth = {
        0x00, 0x01, 0x02, 0x0B, 0x0C, 0x1F
    };
    static const QSet<int> dynamicsTone = {
        0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
        0x0A, 0x0D, 0x12, 0x13, 0x14, 0x15
    };
    static const QSet<int> pitch = {0x0E, 0x0F, 0x10};
    static const QSet<int> modulation = {
        0x16, 0x17, 0x18, 0x19, 0x1A, 0x1D, 0x1E, 0x20
    };
    if (filterSynth.contains(raw))
        return "FILTER / SYNTH";
    if (dynamicsTone.contains(raw))
        return "DYNAMICS / TONE";
    if (pitch.contains(raw))
        return "PITCH";
    if (modulation.contains(raw))
        return "MODULATION";
    return "SEQUENCE / UTILITY";
}

QString ModernFxEditor::typeNameForRaw(int raw) const
{
    for (const TypeEntry &type : types) {
        if (type.raw == raw)
            return type.name;
    }
    return QString::fromUtf8("—");
}
