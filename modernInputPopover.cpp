#include "modernInputPopover.h"

#include "MidiTable.h"
#include "SysxIO.h"
#include "modernTheme.h"
#include "parameterBar.h"

#include <QButtonGroup>
#include <QAbstractButton>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace {
const char *const kSystemArea = "System";
const char *const kAddressMsb = "00";
const char *const kAddressLsb = "00";
const char *const kInputSelectAddress = "4D";

QString rawHex(int raw)
{
    return QString("%1").arg(raw, 2, 16, QChar('0')).toUpper();
}
}

ModernInputPopover::ModernInputPopover(QWidget *parent)
    : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    setObjectName("InputPopover");
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFixedSize(440, 244);
    setFocusPolicy(Qt::StrongFocus);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 18);
    root->setSpacing(14);

    QLabel *title = new QLabel("INPUT SETTINGS");
    title->setObjectName("InputTitle");
    root->addWidget(title);

    QFrame *divider = new QFrame;
    divider->setObjectName("InputDivider");
    divider->setFixedHeight(1);
    root->addWidget(divider);

    QWidget *profileSelector = new QWidget;
    profileSelector->setObjectName("InputProfileSelector");
    QHBoxLayout *profileLayout = new QHBoxLayout(profileSelector);
    profileLayout->setContentsMargins(0, 0, 0, 0);
    profileLayout->setSpacing(0);

    profileGroup = new QButtonGroup(this);
    profileGroup->setExclusive(true);
    const QStringList profiles = {"GUITAR 1", "GUITAR 2", "GUITAR 3", "USB IN"};
    for (int profile = 0; profile < profiles.size(); ++profile) {
        QPushButton *button = new QPushButton(profiles.at(profile));
        button->setObjectName("InputProfileButton");
        button->setCheckable(true);
        button->setProperty("segment", profile == 0 ? "first"
                                                     : profile == profiles.size() - 1
                                                         ? "last" : "middle");
        profileGroup->addButton(button, profile);
        profileLayout->addWidget(button, 1);
    }
    connect(profileGroup, &QButtonGroup::idClicked,
            this, &ModernInputPopover::profileSelected);
    root->addWidget(profileSelector);

    QWidget *parameterRow = new QWidget;
    QHBoxLayout *parameterLayout = new QHBoxLayout(parameterRow);
    parameterLayout->setContentsMargins(0, 0, 0, 0);
    parameterLayout->setSpacing(24);

    levelBar = new ParameterBar("LEVEL");
    presBar = new ParameterBar("PRES");
    for (ParameterBar *bar : {levelBar, presBar}) {
        bar->setRange(0x00, 0x28);
        bar->setCenterValue(0x14);
        bar->setAccentColor(ModernTheme::color(ModernTheme::EditorAccent));
        connect(bar, &QAbstractSlider::valueChanged,
                this, &ModernInputPopover::parameterChanged);
        parameterLayout->addWidget(bar, 1);
    }
    root->addWidget(parameterRow);

    setStyleSheet(QString(
        "QFrame#InputPopover { background: %1; border: 1px solid %2; "
        "border-radius: 9px; }"
        "QLabel#InputTitle { color: %3; font-size: 14px; "
        "font-weight: 700; letter-spacing: 0.5px; }"
        "QFrame#InputDivider { background: %2; border: 0; }"
        "QWidget#InputProfileSelector { background: %4; border: 1px solid %2; "
        "border-radius: 6px; }"
        "QPushButton#InputProfileButton { min-height: 30px; color: %5; "
        "background: transparent; border: 0; border-right: 1px solid %2; "
        "border-radius: 0; font-size: 9px; font-weight: 600; }"
        "QPushButton#InputProfileButton[segment=last] { border-right: 0; }"
        "QPushButton#InputProfileButton:hover { color: %3; background: %6; }"
        "QPushButton#InputProfileButton:checked { color: %7; background: %6; }"
        "QPushButton#InputProfileButton:disabled { color: #59636E; }"
    ).arg(ModernTheme::color(ModernTheme::Panel),
          ModernTheme::color(ModernTheme::Border),
          ModernTheme::color(ModernTheme::PrimaryText),
          ModernTheme::color(ModernTheme::ControlBackground),
          ModernTheme::color(ModernTheme::SecondaryText),
          ModernTheme::color(ModernTheme::ElevatedPanel),
          ModernTheme::color(ModernTheme::EditorAccent)));

    setSystemDataReady(false);
}

void ModernInputPopover::setSystemDataReady(bool systemReady)
{
    ready = systemReady;
    for (QAbstractButton *button : profileGroup->buttons())
        button->setEnabled(ready);
    for (ParameterBar *bar : {levelBar, presBar}) {
        bar->setEnabled(ready);
        if (!ready)
            bar->setDisplayText(QString::fromUtf8("—"));
    }
    if (ready)
        refreshFromBackend();
}

bool ModernInputPopover::systemDataReady() const
{
    return ready;
}

void ModernInputPopover::refreshFromBackend()
{
    if (!ready)
        return;

    const int profile = SysxIO::Instance()->getSourceValue(
        kSystemArea, kAddressMsb, kAddressLsb, kInputSelectAddress);
    currentProfile = qBound(0, profile, 3);
    if (QPushButton *button = qobject_cast<QPushButton *>(
            profileGroup->button(currentProfile))) {
        const QSignalBlocker blocker(button);
        button->setChecked(true);
    }
    refreshParameters();
}

void ModernInputPopover::showAnchoredTo(QWidget *anchor)
{
    if (!anchor || !ready)
        return;
    refreshFromBackend();
    const QPoint below = anchor->mapToGlobal(
        QPoint(anchor->width() - width(), anchor->height() + 7));
    move(below);
    show();
    raise();
    setFocus(Qt::PopupFocusReason);
}

void ModernInputPopover::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }
    QFrame::keyPressEvent(event);
}

void ModernInputPopover::profileSelected(int profile)
{
    if (!ready || profile < 0 || profile > 3)
        return;

    SysxIO *sysxIO = SysxIO::Instance();
    const int backendProfile = sysxIO->getSourceValue(
        kSystemArea, kAddressMsb, kAddressLsb, kInputSelectAddress);
    currentProfile = profile;
    if (backendProfile != profile)
        writeValue(kInputSelectAddress, profile);
    refreshParameters();
}

void ModernInputPopover::parameterChanged(int raw)
{
    ParameterBar *bar = qobject_cast<ParameterBar *>(sender());
    if (!bar || !ready)
        return;

    const QString address = parameterAddress(bar == presBar);
    SysxIO *sysxIO = SysxIO::Instance();
    if (sysxIO->getSourceValue(
            kSystemArea, kAddressMsb, kAddressLsb, address) != raw)
        writeValue(address, raw);
    bar->setDisplayText(displayFor(address, raw));
}

QString ModernInputPopover::parameterAddress(bool presence) const
{
    return rawHex(0x40 + currentProfile * 2 + (presence ? 1 : 0));
}

QString ModernInputPopover::displayFor(const QString &address, int raw) const
{
    QString display = MidiTable::Instance()->getValue(
        kSystemArea, kAddressMsb, kAddressLsb, address, rawHex(raw));
    if (raw > 0x14 && !display.trimmed().startsWith('+'))
        display.prepend('+');
    return display;
}

void ModernInputPopover::refreshParameters()
{
    if (!ready)
        return;
    SysxIO *sysxIO = SysxIO::Instance();
    const QList<ParameterBar *> bars = {levelBar, presBar};
    for (int index = 0; index < bars.size(); ++index) {
        ParameterBar *bar = bars.at(index);
        const QString address = parameterAddress(index == 1);
        const int raw = sysxIO->getSourceValue(
            kSystemArea, kAddressMsb, kAddressLsb, address);
        const QSignalBlocker blocker(bar);
        bar->setValue(raw);
        bar->setDisplayText(displayFor(address, raw));
    }
}

void ModernInputPopover::writeValue(const QString &address, int raw)
{
    SysxIO *sysxIO = SysxIO::Instance();
    if (!ready || !sysxIO->isConnected())
        return;
    sysxIO->setFileSource(
        kSystemArea, kAddressMsb, kAddressLsb, address, rawHex(raw));
}
