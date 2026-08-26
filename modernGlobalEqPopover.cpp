#include "modernGlobalEqPopover.h"

#include "MidiTable.h"
#include "SysxIO.h"
#include "modernPopoverShell.h"
#include "modernTheme.h"
#include "modernWidgets.h"
#include "parameterBar.h"

#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace {
const char *const kSystemArea = "System";
const char *const kAddressMsb = "00";
const char *const kAddressLsb = "00";

QString rawHex(int raw)
{
    return QString("%1").arg(raw, 2, 16, QChar('0')).toUpper();
}
}

ModernGlobalEqPopover::ModernGlobalEqPopover(QWidget *parent)
    : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    setObjectName("GlobalEqPopover");
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAutoFillBackground(false);
    setFixedSize(480, 252);
    setFocusPolicy(Qt::StrongFocus);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 18);
    root->setSpacing(14);

    QLabel *title = new QLabel("GLOBAL EQ");
    title->setObjectName("GlobalEqTitle");
    root->addWidget(title);

    QFrame *divider = new QFrame;
    divider->setObjectName("GlobalEqDivider");
    divider->setFixedHeight(1);
    root->addWidget(divider);

    QWidget *gainRow = new QWidget;
    QHBoxLayout *gainLayout = new QHBoxLayout(gainRow);
    gainLayout->setContentsMargins(0, 0, 0, 0);
    gainLayout->setSpacing(12);
    lowGain = createGain("Low Gain", "48");
    midGain = createGain("Mid Gain", "49");
    highGain = createGain("High Gain", "4C");
    gainLayout->addWidget(lowGain, 1);
    gainLayout->addWidget(midGain, 1);
    gainLayout->addWidget(highGain, 1);
    root->addWidget(gainRow);

    QWidget *selectorRow = new QWidget;
    QHBoxLayout *selectorLayout = new QHBoxLayout(selectorRow);
    selectorLayout->setContentsMargins(0, 0, 0, 0);
    selectorLayout->setSpacing(18);
    selectorLayout->addWidget(
        createSelector("Mid Frequency", "4B", &midFrequency), 1);
    selectorLayout->addWidget(createSelector("Mid Q", "4A", &midQ), 1);
    root->addWidget(selectorRow);

    setStyleSheet(QString(
        "QFrame#GlobalEqPopover { background: transparent; border: 0; }"
        "QLabel#GlobalEqTitle { color: %2; font-size: 14px; "
        "font-weight: 700; letter-spacing: 0.5px; }"
        "QFrame#GlobalEqDivider { background: %1; border: 0; }"
    ).arg(ModernTheme::color(ModernTheme::Border),
          ModernTheme::color(ModernTheme::PrimaryText)));

    setSystemDataReady(false);
}

ParameterBar *ModernGlobalEqPopover::createGain(
    const QString &label, const QString &address)
{
    ParameterBar *bar = new ParameterBar(label);
    bar->setProperty("address", address);
    bar->setMinimumWidth(120);
    bar->setMaximumWidth(145);
    bar->setRange(MidiTable::Instance()->getRangeMinimum(
                      kSystemArea, kAddressMsb, kAddressLsb, address),
                  MidiTable::Instance()->getRange(
                      kSystemArea, kAddressMsb, kAddressLsb, address));
    bar->setCenterValue(0x0C);
    bar->setAccentColor(ModernTheme::color(ModernTheme::EditorAccent));
    connect(bar, &QAbstractSlider::valueChanged,
            this, &ModernGlobalEqPopover::gainChanged);
    return bar;
}

QWidget *ModernGlobalEqPopover::createSelector(
    const QString &label, const QString &address, QComboBox **comboOut)
{
    ParameterCombo *container = new ParameterCombo(label);
    container->setMaximumWidth(QWIDGETSIZE_MAX);
    QComboBox *combo = container->comboBox();
    combo->setProperty("address", address);

    const Midi parameter = MidiTable::Instance()->getMidiMap(
        kSystemArea, kAddressMsb, kAddressLsb, address);
    for (const Midi &item : parameter.level) {
        if (item.value == "range")
            continue;
        bool rawOk = false;
        const int raw = item.value.toInt(&rawOk, 16);
        if (!rawOk)
            continue;
        const QString display = item.desc.isEmpty() ? item.name : item.desc;
        combo->addItem(display, raw);
    }
    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ModernGlobalEqPopover::selectorChanged);
    *comboOut = combo;
    return container;
}

void ModernGlobalEqPopover::setSystemDataReady(bool systemReady)
{
    ready = systemReady;
    for (ParameterBar *bar : {lowGain, midGain, highGain}) {
        if (!bar)
            continue;
        bar->setEnabled(ready);
        if (!ready)
            bar->setDisplayText(QString::fromUtf8("—"));
    }
    for (QComboBox *combo : {midFrequency, midQ}) {
        if (!combo)
            continue;
        combo->setEnabled(ready);
        if (!ready) {
            const QSignalBlocker blocker(combo);
            combo->setCurrentIndex(-1);
        }
    }
    if (ready)
        refreshFromBackend();
}

bool ModernGlobalEqPopover::systemDataReady() const
{
    return ready;
}

void ModernGlobalEqPopover::refreshFromBackend()
{
    if (!ready)
        return;

    SysxIO *sysxIO = SysxIO::Instance();
    for (ParameterBar *bar : {lowGain, midGain, highGain}) {
        const QString address = bar->property("address").toString();
        const int raw = sysxIO->getSourceValue(
            kSystemArea, kAddressMsb, kAddressLsb, address);
        const QSignalBlocker blocker(bar);
        bar->setValue(raw);
        bar->setDisplayText(gainDisplay(address, raw));
    }
    for (QComboBox *combo : {midFrequency, midQ}) {
        const QString address = combo->property("address").toString();
        const int raw = sysxIO->getSourceValue(
            kSystemArea, kAddressMsb, kAddressLsb, address);
        const int index = combo->findData(raw);
        const QSignalBlocker blocker(combo);
        combo->setCurrentIndex(index);
    }
}

void ModernGlobalEqPopover::showAnchoredTo(QWidget *anchor)
{
    if (!anchor || !ready)
        return;
    refreshFromBackend();
    adjustSize();
    const QPoint below = anchor->mapToGlobal(
        QPoint(anchor->width() - width(), anchor->height() + 7));
    move(below);
    show();
    raise();
    setFocus(Qt::PopupFocusReason);
}

void ModernGlobalEqPopover::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }
    QFrame::keyPressEvent(event);
}

void ModernGlobalEqPopover::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    ModernPopoverShell::paint(this);
}

void ModernGlobalEqPopover::gainChanged(int raw)
{
    ParameterBar *bar = qobject_cast<ParameterBar *>(sender());
    if (!bar || !ready)
        return;
    const QString address = bar->property("address").toString();
    writeValue(address, raw);
    bar->setDisplayText(gainDisplay(address, raw));
}

void ModernGlobalEqPopover::selectorChanged(int index)
{
    QComboBox *combo = qobject_cast<QComboBox *>(sender());
    if (!combo || !ready || index < 0)
        return;
    bool rawOk = false;
    const int raw = combo->itemData(index).toInt(&rawOk);
    if (rawOk)
        writeValue(combo->property("address").toString(), raw);
}

void ModernGlobalEqPopover::writeValue(const QString &address, int raw)
{
    SysxIO *sysxIO = SysxIO::Instance();
    if (!ready || !sysxIO->isConnected())
        return;
    sysxIO->setFileSource(
        kSystemArea, kAddressMsb, kAddressLsb, address, rawHex(raw));
}

QString ModernGlobalEqPopover::gainDisplay(
    const QString &address, int raw) const
{
    QString display = MidiTable::Instance()->getValue(
        kSystemArea, kAddressMsb, kAddressLsb, address, rawHex(raw));
    if (raw > 0x0C && !display.trimmed().startsWith('+'))
        display.prepend('+');
    return display;
}
