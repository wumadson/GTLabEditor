#include "assignTargetValueEditor.h"

#include "MidiTable.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace {
const QString kStructure = "Structure";
const QString kMiddleByte = "00";
const QString kTargetCatalogBank = "0B";
const QString kTargetCatalogAddress = "21";

QString hexValue(int value)
{
    return QString("%1").arg(value, 2, 16, QChar('0')).toUpper();
}

QString numericToken(const QString &text)
{
    const QRegularExpression expression("[-+]?\\d+(?:[.,]\\d+)?");
    return expression.match(text).captured(0).replace(',', '.');
}

QString rhythmAbbreviation(const QString &display)
{
    const QString key = display.simplified().toLower();
    if (key == "whole note") return "1/1";
    if (key == "doted half note") return "1/2D";
    if (key == "whole note triplet") return "1/1T";
    if (key == "half note") return "1/2";
    if (key == "doted quarter note") return "1/4D";
    if (key == "half note triplet") return "1/2T";
    if (key == "quarter note") return "1/4";
    if (key == "doted eighth note") return "1/8D";
    if (key == "quarter note triplet") return "1/4T";
    if (key == "eighth note") return "1/8";
    if (key == "doted sixteenth note") return "1/16D";
    if (key == "eighth note triplet") return "1/8T";
    if (key == "sixteenth note") return "1/16";
    return QString();
}
}

class TargetValueSpinBox : public QSpinBox
{
public:
    explicit TargetValueSpinBox(QWidget *parent = nullptr) : QSpinBox(parent)
    {
        setObjectName("AssignTargetValueSpinBox");
        setKeyboardTracking(false);
        setButtonSymbols(QAbstractSpinBox::NoButtons);
        setAlignment(Qt::AlignCenter);
        setFixedSize(130, 36);
    }

    void setDisplays(const QVector<QString> &newDisplays, int rawMinimum)
    {
        displays = newDisplays;
        displayMinimum = rawMinimum;
    }

protected:
    QString textFromValue(int value) const override
    {
        const int index = value - displayMinimum;
        return index >= 0 && index < displays.size()
            ? displays.at(index) : QString::number(value);
    }

    int valueFromText(const QString &text) const override
    {
        const QString candidate = text.trimmed();
        for (int index = 0; index < displays.size(); ++index) {
            if (displays.at(index).compare(candidate, Qt::CaseInsensitive) == 0)
                return displayMinimum + index;
        }
        const QString candidateNumber = numericToken(candidate);
        if (!candidateNumber.isEmpty()) {
            int match = -1;
            for (int index = 0; index < displays.size(); ++index) {
                if (numericToken(displays.at(index)) == candidateNumber) {
                    if (match >= 0)
                        return value();
                    match = displayMinimum + index;
                }
            }
            if (match >= 0)
                return match;
        }
        return value();
    }

    QValidator::State validate(QString &input, int &) const override
    {
        const QString candidate = input.trimmed();
        if (candidate.isEmpty() || candidate == "+" || candidate == "-")
            return QValidator::Intermediate;
        const QString candidateNumber = numericToken(candidate);
        for (const QString &display : displays) {
            if (display.compare(candidate, Qt::CaseInsensitive) == 0)
                return QValidator::Acceptable;
            if (display.startsWith(candidate, Qt::CaseInsensitive))
                return QValidator::Intermediate;
            const QString displayNumber = numericToken(display);
            if (!candidateNumber.isEmpty()
                && displayNumber.startsWith(candidateNumber))
                return displayNumber == candidateNumber
                    ? QValidator::Acceptable : QValidator::Intermediate;
        }
        return QValidator::Invalid;
    }

    void wheelEvent(QWheelEvent *event) override { event->ignore(); }

private:
    QVector<QString> displays;
    int displayMinimum = 0;
};

AssignTargetValueEditor::AssignTargetValueEditor(
    const QString &label, QWidget *parent) : QWidget(parent)
{
    setMinimumSize(130, 64);
    setMaximumWidth(216);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    title = new QLabel(label.toUpper());
    title->setObjectName("ParameterLabel");
    stack = new QStackedWidget;
    stack->setFixedHeight(36);
    selector = new QComboBox;
    selector->setObjectName("AssignTargetValueSelector");
    selector->setFixedSize(130, 36);
    selector->setMaxVisibleItems(12);
    selector->view()->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    spinBox = new TargetValueSpinBox;
    hybridPage = new QWidget;
    hybridPage->setFixedWidth(130);
    QVBoxLayout *hybridLayout = new QVBoxLayout(hybridPage);
    hybridLayout->setContentsMargins(0, 0, 0, 0);
    hybridLayout->setSpacing(4);
    QHBoxLayout *modeLayout = new QHBoxLayout;
    modeLayout->setContentsMargins(0, 0, 0, 0);
    modeLayout->setSpacing(4);
    timeButton = new QPushButton("TIME");
    rhythmButton = new QPushButton("RHYTHM");
    for (QPushButton *button : {timeButton, rhythmButton}) {
        button->setObjectName("AssignTargetValueMode");
        button->setCheckable(true);
        button->setFixedSize(63, 22);
        modeLayout->addWidget(button);
    }
    hybridValueStack = new QStackedWidget;
    hybridValueStack->setFixedHeight(36);
    hybridSpinBox = new TargetValueSpinBox;
    rhythmSelector = new QComboBox;
    rhythmSelector->setObjectName("AssignTargetValueSelector");
    rhythmSelector->setEditable(false);
    rhythmSelector->setFixedSize(130, 36);
    rhythmSelector->setMaxVisibleItems(13);
    rhythmSelector->view()->setVerticalScrollMode(
        QAbstractItemView::ScrollPerPixel);
    hybridValueStack->addWidget(hybridSpinBox);
    hybridValueStack->addWidget(rhythmSelector);
    hybridLayout->addLayout(modeLayout);
    hybridLayout->addWidget(hybridValueStack);
    stack->addWidget(selector);
    stack->addWidget(spinBox);
    stack->addWidget(hybridPage);
    layout->addWidget(title);
    layout->addWidget(stack, 0, Qt::AlignLeft);

    connect(selector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                if (!updating && index >= 0 && valueEdited)
                    valueEdited(configuredTargetId,
                                selector->itemData(index).toInt());
            });
    connect(spinBox, &QSpinBox::editingFinished, this, [this]() {
        if (!updating && valueEdited)
            valueEdited(configuredTargetId, spinBox->value());
    });
    connect(hybridSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) {
                if (!updating)
                    hybridTimeDirty = true;
            });
    connect(hybridSpinBox, &QSpinBox::editingFinished, this, [this]() {
        if (!updating && hybridTimeDirty && valueEdited)
            valueEdited(configuredTargetId, hybridSpinBox->value());
        hybridTimeDirty = false;
    });
    connect(rhythmSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                if (!updating && index >= 0
                    && rhythmSelector->itemData(index).isValid()
                    && valueEdited)
                    valueEdited(configuredTargetId,
                                rhythmSelector->itemData(index).toInt());
            });
    connect(timeButton, &QPushButton::clicked, this, [this]() {
        timeButton->setChecked(true);
        rhythmButton->setChecked(false);
        hybridValueStack->setCurrentWidget(hybridSpinBox);
        hybridTimeDirty = false;
        hybridSpinBox->setFocus();
    });
    connect(rhythmButton, &QPushButton::clicked, this, [this]() {
        rhythmButton->setChecked(true);
        timeButton->setChecked(false);
        hybridValueStack->setCurrentWidget(rhythmSelector);
        rhythmSelector->setFocus();
        rhythmSelector->showPopup();
    });

    setStyleSheet(
        "QComboBox#AssignTargetValueSelector,QSpinBox#AssignTargetValueSpinBox{"
        "background:rgba(15,25,34,235);color:#F2F4F6;"
        "border:1px solid #2B3945;border-radius:6px;padding:0 10px;"
        "font-size:12px;}"
        "QComboBox#AssignTargetValueSelector:hover,"
        "QSpinBox#AssignTargetValueSpinBox:hover{border-color:#3C4D5A;"
        "background:rgba(18,31,42,240);}"
        "QComboBox#AssignTargetValueSelector:focus,"
        "QSpinBox#AssignTargetValueSpinBox:focus{border-color:#00AEEF;}"
        "QComboBox#AssignTargetValueSelector:disabled,"
        "QSpinBox#AssignTargetValueSpinBox:disabled{color:#69747E;"
        "background:#0B1117;border-color:#202B34;}"
        "QPushButton#AssignTargetValueMode{background:#0B1117;color:#7F8B96;"
        "border:1px solid #26333D;border-radius:5px;font-size:9px;}"
        "QPushButton#AssignTargetValueMode:checked{color:#00AEEF;"
        "background:#10283D;border-color:#246080;}"
        "QComboBox#AssignTargetValueSelector::drop-down{border:none;width:22px;}"
        "QComboBox QAbstractItemView{background:#0B1117;color:#F2F4F6;"
        "border:1px solid #34414C;selection-background-color:#12324D;"
        "outline:0px;}"
        "QScrollBar:vertical{background:#0B1117;width:8px;margin:2px;}"
        "QScrollBar::handle:vertical{background:#3A4651;min-height:28px;"
        "border-radius:4px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0px;}"
        "QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical{"
        "background:transparent;}");
}

QString AssignTargetValueEditor::originalDisplayForRaw(int raw) const
{
    const QString display = MidiTable::Instance()->getValue(
        kStructure, targetBank, kMiddleByte, targetAddress,
        hexValue(raw)).trimmed();
    return display.isEmpty() ? QString::number(raw) : display;
}

QString AssignTargetValueEditor::displayForRaw(int raw) const
{
    const QString original = originalDisplayForRaw(raw);
    const QString rhythm = rhythmAbbreviation(original);
    return rhythm.isEmpty() ? original : rhythm;
}

void AssignTargetValueEditor::configureTarget(int targetId)
{
    configuredTargetId = targetId;
    targetBank.clear();
    targetAddress.clear();
    selector->clear();
    rhythmSelector->clear();
    if (targetId < 0 || targetId > 618)
        return;

    MidiTable *table = MidiTable::Instance();
    const Midi target = table->getMidiMap(
        kStructure, kTargetCatalogBank, kMiddleByte, kTargetCatalogAddress,
        hexValue(targetId / 128), hexValue(targetId % 128));
    targetBank = target.desc.trimmed();
    targetAddress = target.customdesc.trimmed();
    if (targetBank.isEmpty() || targetAddress.isEmpty())
        return;

    minimum = table->getRangeMinimum(
        kStructure, targetBank, kMiddleByte, targetAddress);
    maximum = table->getRange(
        kStructure, targetBank, kMiddleByte, targetAddress);
    const Midi parameter = table->getMidiMap(
        kStructure, targetBank, kMiddleByte, targetAddress);
    bool hasContinuousRange = false;
    for (const Midi &entry : parameter.level)
        hasContinuousRange = hasContinuousRange || entry.value == "range";
    selectorMode = !hasContinuousRange
        && !table->isData(kStructure, targetBank, kMiddleByte, targetAddress)
        && maximum - minimum <= 127;

    QVector<QString> displays;
    QVector<QString> numericDisplays;
    int numericMinimum = minimum;
    int numericMaximum = minimum;
    bool hasNumeric = false;
    bool hasRhythm = false;
    rhythmSelector->addItem("SELECT RHYTHM");
    displays.reserve(maximum - minimum + 1);
    for (int raw = minimum; raw <= maximum; ++raw) {
        const QString originalDisplay = originalDisplayForRaw(raw);
        const QString display = displayForRaw(raw);
        displays.append(display);
        const QString rhythm = rhythmAbbreviation(originalDisplay);
        if (!rhythm.isEmpty()) {
            hasRhythm = true;
            rhythmSelector->addItem(display, raw);
            rhythmSelector->setItemData(rhythmSelector->count() - 1,
                                        originalDisplay, Qt::ToolTipRole);
        } else {
            if (!hasNumeric)
                numericMinimum = raw;
            hasNumeric = true;
            numericMaximum = raw;
            numericDisplays.append(display);
        }
        if (selectorMode)
            selector->addItem(display, raw);
    }
    hybridMode = hasNumeric && hasRhythm
        && numericDisplays.size() == numericMaximum - numericMinimum + 1;
    spinBox->setDisplays(displays, minimum);
    spinBox->setRange(minimum, maximum);
    if (hybridMode) {
        hybridSpinBox->setDisplays(numericDisplays, numericMinimum);
        hybridSpinBox->setRange(numericMinimum, numericMaximum);
        stack->setFixedHeight(62);
        setMinimumHeight(90);
        stack->setCurrentWidget(hybridPage);
    } else {
        stack->setFixedHeight(36);
        setMinimumHeight(64);
        stack->setCurrentWidget(selectorMode
            ? static_cast<QWidget *>(selector)
            : static_cast<QWidget *>(spinBox));
    }
}

void AssignTargetValueEditor::setTargetValue(
    int targetId, int raw, bool available)
{
    updating = true;
    if (configuredTargetId != targetId)
        configureTarget(targetId);
    const bool valid = available && targetId >= 0 && targetId <= 618
        && !targetBank.isEmpty() && !targetAddress.isEmpty()
        && raw >= minimum && raw <= maximum;
    setEnabled(valid);
    if (valid) {
        if (hybridMode) {
            const int rhythmIndex = rhythmSelector->findData(raw);
            const bool rhythm = rhythmIndex >= 0;
            timeButton->setChecked(!rhythm);
            rhythmButton->setChecked(rhythm);
            hybridValueStack->setCurrentWidget(rhythm
                ? static_cast<QWidget *>(rhythmSelector)
                : static_cast<QWidget *>(hybridSpinBox));
            if (rhythm) {
                const QSignalBlocker blocker(rhythmSelector);
                rhythmSelector->setCurrentIndex(rhythmIndex);
                rhythmSelector->setToolTip(originalDisplayForRaw(raw));
            } else {
                const QSignalBlocker selectorBlocker(rhythmSelector);
                rhythmSelector->setCurrentIndex(0);
                rhythmSelector->setToolTip(QString());
                const QSignalBlocker blocker(hybridSpinBox);
                hybridSpinBox->setValue(raw);
            }
            hybridTimeDirty = false;
        } else if (selectorMode) {
            const int index = selector->findData(raw);
            const QSignalBlocker blocker(selector);
            selector->setCurrentIndex(index);
        } else {
            const QSignalBlocker blocker(spinBox);
            spinBox->setValue(raw);
        }
    }
    updating = false;
}
