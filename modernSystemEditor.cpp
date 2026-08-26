#include "modernSystemEditor.h"

#include "MidiTable.h"
#include "SysxIO.h"
#include "globalVariables.h"
#include "modernTheme.h"
#include "modernWidgets.h"
#include "parameterBar.h"

#include <QComboBox>
#include <QButtonGroup>
#include <QAbstractButton>
#include <QDebug>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {
QString rawHex(int raw)
{
    return QString::number(raw, 16).rightJustified(2, '0').toUpper();
}

QGridLayout *sectionGrid(QWidget *section)
{
    return section ? section->findChild<QGridLayout *>("SystemSectionGrid")
                   : nullptr;
}

void addControlToGrid(QGridLayout *grid, QWidget *control)
{
    if (!grid || !control)
        return;
    const int index = grid->count();
    grid->addWidget(control, index / 2, index % 2);
}

bool systemParameter(const QString &bank, const QString &page,
                     const QString &address, Midi *parameter)
{
    if (!parameter)
        return false;
    const Midi system = MidiTable::Instance()->getMidiMap("System");
    const int bankIndex = system.id.indexOf(bank);
    if (bankIndex < 0 || bankIndex >= system.level.size())
        return false;
    const Midi bankMap = system.level.at(bankIndex);
    const int pageIndex = bankMap.id.indexOf(page);
    if (pageIndex < 0 || pageIndex >= bankMap.level.size())
        return false;
    const Midi pageMap = bankMap.level.at(pageIndex);
    const int addressIndex = pageMap.id.indexOf(address);
    if (addressIndex < 0 || addressIndex >= pageMap.level.size())
        return false;
    *parameter = pageMap.level.at(addressIndex);
    return !parameter->level.isEmpty();
}

}

ModernSystemEditor::ModernSystemEditor(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("SystemWorkspace");
    auto *title = new QLabel(tr("SYSTEM"), this);
    title->setObjectName("SystemWorkspaceTitle");
    availability = new QLabel(tr("SYSTEM DATA UNAVAILABLE"), this);
    availability->setObjectName("SystemAvailability");

    navigation = new QListWidget(this);
    navigation->setObjectName("SystemNavigation");
    navigation->setFixedWidth(166);
    navigation->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    navigation->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    navigation->setFocusPolicy(Qt::NoFocus);
    navigation->addItems({tr("INPUT"), tr("OUTPUT"), tr("GLOBAL EQ"),
                          tr("LCD"), tr("USB"), tr("PLAY OPTION"),
                          tr("PHRASE LOOP"), tr("CONTROLLERS"),
                          tr("CATEGORY NAMES")});

    pages = new QStackedWidget(this);
    pages->setObjectName("SystemPages");
    pages->addWidget(createInputPage());
    pages->addWidget(createOutputPage());
    pages->addWidget(createGlobalEqPage());
    pages->addWidget(createLcdPage());
    pages->addWidget(createUsbPage());
    pages->addWidget(createPlayOptionPage());
    pages->addWidget(createPhraseLoopPage());
    pages->addWidget(createControllersPage());
    pages->addWidget(createCategoryNamesPage());
    connect(navigation, &QListWidget::currentRowChanged,
            pages, &QStackedWidget::setCurrentIndex);

    auto *contentFrame = new QFrame(this);
    contentFrame->setObjectName("SystemContentFrame");
    contentFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *frameLayout = new QVBoxLayout(contentFrame);
    frameLayout->setContentsMargins(14, 12, 14, 14);
    frameLayout->setSpacing(10);
    auto *headerLayout = new QVBoxLayout;
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(2);
    headerLayout->addWidget(title);
    headerLayout->addWidget(availability);
    frameLayout->addLayout(headerLayout);
    auto *content = new QHBoxLayout;
    content->setContentsMargins(0, 0, 0, 0);
    content->setSpacing(18);
    navigation->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    pages->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    content->addWidget(navigation);
    content->addWidget(pages, 1);
    frameLayout->addLayout(content, 1);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 4, 0, 0);
    root->setSpacing(0);
    root->addWidget(contentFrame, 1);

    const QString background = ModernTheme::color(ModernTheme::ApplicationBackground);
    const QString panel = ModernTheme::color(ModernTheme::Panel);
    const QString elevated = ModernTheme::color(ModernTheme::ElevatedPanel);
    const QString control = ModernTheme::color(ModernTheme::ControlBackground);
    const QString border = ModernTheme::color(ModernTheme::Border);
    const QString primary = ModernTheme::color(ModernTheme::PrimaryText);
    const QString secondary = ModernTheme::color(ModernTheme::SecondaryText);
    const QString disabled = ModernTheme::color(ModernTheme::DisabledText);
    const QString accent = ModernTheme::color(ModernTheme::AccentCyan);
    setStyleSheet(QString(
        "QWidget#SystemWorkspace{background:%1;color:%2;}"
        "QLabel#SystemWorkspaceTitle{color:%2;font-size:20px;font-weight:700;letter-spacing:1px;}"
        "QLabel#SystemAvailability{color:%7;font-size:9px;font-weight:600;letter-spacing:.7px;}"
        "QFrame#SystemContentFrame{background:#060708;border:1px solid %5;border-radius:10px;}"
        "QListWidget#SystemNavigation{background:%4;border:1px solid %5;border-radius:8px;"
        "padding:9px 7px;outline:none;}"
        "QListWidget#SystemNavigation::item{color:%6;height:34px;padding-left:10px;"
        "border:1px solid transparent;border-radius:6px;font-size:10px;font-weight:600;}"
        "QListWidget#SystemNavigation::item:hover{background:#151D26;color:%2;}"
        "QListWidget#SystemNavigation::item:selected{background:#10263B;"
        "border-color:%8;color:%2;}"
        "QStackedWidget#SystemPages{background:#060708;border:0;}"
        "QScrollArea#SystemPageScroll{background:#060708;border:none;}"
        "QWidget#SystemPageViewport{background:#060708;}"
        "QWidget#SystemPage{background:#060708;}"
        "QLabel#SystemPageTitle{color:%2;font-size:15px;font-weight:700;letter-spacing:.8px;}"
        "QFrame#SystemSection{background:%3;border:1px solid %5;border-radius:8px;}"
        "QLabel#SystemSectionTitle{color:%6;font-size:9px;font-weight:700;letter-spacing:.8px;}"
        "QLabel#SystemFieldLabel{color:%6;font-size:9px;font-weight:600;letter-spacing:.4px;}"
        "QLabel#SystemNote{color:%6;font-size:10px;}"
        "QWidget#SystemInputSelector{background:%4;border:1px solid %5;border-radius:6px;}"
        "QPushButton#SystemInputProfileButton{min-height:32px;color:%6;background:transparent;"
        "border:0;border-right:1px solid %5;border-radius:0;font-size:9px;font-weight:600;}"
        "QPushButton#SystemInputProfileButton[segment=first]{border-top-left-radius:5px;"
        "border-bottom-left-radius:5px;}"
        "QPushButton#SystemInputProfileButton[segment=last]{border-right:0;"
        "border-top-right-radius:5px;border-bottom-right-radius:5px;}"
        "QPushButton#SystemInputProfileButton:hover{color:%2;background:#151D26;}"
        "QPushButton#SystemInputProfileButton:focus{color:%2;background:#151D26;"
        "border:1px solid %8;}"
        "QPushButton#SystemInputProfileButton:checked{color:%2;background:#10263B;}"
        "QPushButton#SystemInputProfileButton:checked:focus{color:%2;background:#10263B;"
        "border-color:%8;}"
        "QPushButton#SystemInputProfileButton:disabled{color:%7;}"
        "QWidget#SystemWorkspace QComboBox{min-height:32px;padding:0 9px;color:%2;"
        "background:%4;border:1px solid %5;border-radius:6px;}"
        "QWidget#SystemWorkspace QComboBox:hover{background:#151D26;border-color:#34414C;}"
        "QWidget#SystemWorkspace QComboBox:focus{border-color:%8;}"
        "QWidget#SystemWorkspace QComboBox:disabled{color:%7;background:%1;border-color:%5;}"
        "QWidget#SystemWorkspace QComboBox::drop-down{border:0;width:24px;}"
        "QWidget#SystemWorkspace QComboBox QAbstractItemView{background:%3;color:%2;"
        "border:1px solid #34414C;selection-background-color:#12324D;"
        "selection-color:%2;outline:0;padding:3px;}"
        "QLineEdit#SystemCategoryEditor{color:%2;background:%4;border:1px solid %5;"
        "border-radius:6px;padding:0 9px;min-height:32px;font-size:11px;font-weight:600;}"
        "QLineEdit#SystemCategoryEditor:focus{border-color:%8;}"
        "QLineEdit#SystemCategoryEditor:disabled{color:%7;background:%1;}"
        "QScrollBar:vertical{width:8px;background:transparent;margin:2px;}"
        "QScrollBar::handle:vertical{background:%5;border-radius:4px;min-height:30px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}"
    ).arg(background, primary, panel, control, border, secondary, disabled,
          accent));
    navigation->setCurrentRow(0);
    refresh(false, false);
}

QWidget *ModernSystemEditor::createPage(const QString &title)
{
    auto *content = new QWidget;
    content->setObjectName("SystemPage");
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(18, 2, 12, 12);
    layout->setSpacing(10);
    auto *heading = new QLabel(title, content);
    heading->setObjectName("SystemPageTitle");
    layout->addWidget(heading);
    auto *scroll = new QScrollArea;
    scroll->setObjectName("SystemPageScroll");
    scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setWidget(content);
    scroll->viewport()->setObjectName("SystemPageViewport");
    return scroll;
}

QWidget *ModernSystemEditor::createSection(QWidget *page,
                                            QVBoxLayout *pageLayout,
                                            const QString &title)
{
    auto *section = new QFrame(page);
    section->setObjectName("SystemSection");
    auto *layout = new QVBoxLayout(section);
    layout->setContentsMargins(14, 12, 14, 14);
    layout->setSpacing(8);
    auto *heading = new QLabel(title, section);
    heading->setObjectName("SystemSectionTitle");
    layout->addWidget(heading);
    auto *grid = new QGridLayout;
    grid->setObjectName("SystemSectionGrid");
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(10);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    layout->addLayout(grid);
    pageLayout->addWidget(section);
    return section;
}

void ModernSystemEditor::addSelector(QWidget *section, const QString &label,
                                     const QString &bank, const QString &page,
                                     const QString &address)
{
    auto *container = new ParameterCombo(label, section);
    container->setMaximumWidth(QWIDGETSIZE_MAX);
    QComboBox *combo = container->comboBox();
    Midi catalog;
    bool catalogAvailable = systemParameter(bank, page, address, &catalog);
    bool directCatalog = catalogAvailable;
    if (directCatalog) {
        for (const Midi &item : catalog.level) {
            bool rawOk = false;
            item.value.toInt(&rawOk, 16);
            if (!rawOk || !item.level.isEmpty()) {
                directCatalog = false;
                break;
            }
        }
    }
    if (directCatalog) {
        // The legacy combo follows the catalog entries directly. This is
        // required for 00:00:72/74/76/78, whose DATA children are values,
        // not the nested DATA range expected by MidiTable::getRange().
        for (const Midi &item : catalog.level) {
            bool rawOk = false;
            const int raw = item.value.toInt(&rawOk, 16);
            QString display = item.customdesc;
            if (display.isEmpty())
                display = item.desc;
            if (display.isEmpty())
                display = item.name;
            if (rawOk && !display.isEmpty())
                combo->addItem(display, raw);
        }
    } else if (catalogAvailable) {
        const Midi &last = catalog.level.last();
        const bool safeRange = !last.type.contains("DATA")
            || !last.level.isEmpty();
        if (safeRange) {
            const int minimum = MidiTable::Instance()->getRangeMinimum(
                "System", bank, page, address);
            const int maximum = MidiTable::Instance()->getRange(
                "System", bank, page, address);
            catalogAvailable = minimum <= maximum;
            for (int raw = minimum; catalogAvailable && raw <= maximum; ++raw) {
                const QString display = displayForRaw(bank, page, address, raw);
                if (!display.isEmpty() && display != QString::fromUtf8("—"))
                    combo->addItem(display, raw);
            }
        } else {
            catalogAvailable = false;
        }
    }
    catalogAvailable = catalogAvailable && combo->count() > 0;
    if (!catalogAvailable) {
        qWarning().noquote() << "SYSTEM selector unavailable:"
                             << label << "System" << bank << page << address;
        combo->addItem(QString::fromUtf8("—"), -1);
    }
    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, combo, bank, page, address](int index) {
        if (!ready || index < 0)
            return;
        bool ok = false;
        const int raw = combo->itemData(index).toInt(&ok);
        if (ok)
            writeValue(bank, page, address, raw);
    });
    addControlToGrid(sectionGrid(section), container);
    fields.append({FieldKind::Selector, container, combo, nullptr,
                   bank, page, address, catalogAvailable});
}

ParameterBar *ModernSystemEditor::addBar(
    QWidget *section, const QString &label, const QString &bank,
    const QString &page, const QString &address)
{
    auto *bar = new ParameterBar(label, section);
    Midi catalog;
    bool catalogAvailable = systemParameter(bank, page, address, &catalog);
    if (catalogAvailable) {
        const Midi &last = catalog.level.last();
        catalogAvailable = !last.type.contains("DATA")
            || !last.level.isEmpty();
    }
    if (catalogAvailable) {
        const int minimum = MidiTable::Instance()->getRangeMinimum(
            "System", bank, page, address);
        const int maximum = MidiTable::Instance()->getRange(
            "System", bank, page, address);
        catalogAvailable = minimum <= maximum;
        if (catalogAvailable)
            bar->setRange(minimum, maximum);
    }
    if (!catalogAvailable) {
        qWarning().noquote() << "SYSTEM range unavailable:"
                             << label << "System" << bank << page << address;
        bar->setRange(0, 0);
        bar->setDisplayText(QString::fromUtf8("—"));
    }
    bar->setAccentColor(ModernTheme::color(ModernTheme::EditorAccent));
    connect(bar, &QAbstractSlider::valueChanged,
            this, [this, bar, bank, page, address](int raw) {
        if (!ready)
            return;
        writeValue(bank, page, address, raw);
        bar->setDisplayText(displayForRaw(bank, page, address, raw));
    });
    addControlToGrid(sectionGrid(section), bar);
    fields.append({FieldKind::Bar, bar, nullptr, bar, bank, page, address,
                   catalogAvailable});
    return bar;
}

void ModernSystemEditor::addControllerSection(
    QWidget *pageWidget, QVBoxLayout *layout, const QString &name,
    const QString &scopePage, const QString &scopeAddress,
    const QString &detailBase)
{
    bool baseOk = false;
    const int base = detailBase.toInt(&baseOk, 16);
    QWidget *section = createSection(pageWidget, layout, name);
    if (!baseOk) {
        qWarning().noquote() << "SYSTEM controller base unavailable:"
                             << name << detailBase;
        return;
    }
    const auto addressAt = [base](int offset) {
        return rawHex(base + offset);
    };
    addSelector(section, tr("Scope"), "00", scopePage, scopeAddress);
    addSelector(section, tr("Setting"), "00", "01", addressAt(0x00));
    addSelector(section, tr("Function"), "00", "01", addressAt(0x01));
    addBar(section, tr("Target Min"), "00", "01", addressAt(0x03));
    addBar(section, tr("Target Max"), "00", "01", addressAt(0x05));
    addSelector(section, tr("Source Mode"), "00", "01", addressAt(0x07));
    ParameterBar *rangeLow = addBar(
        section, tr("Active Range Low"), "00", "01", addressAt(0x08));
    ParameterBar *rangeHigh = addBar(
        section, tr("Active Range High"), "00", "01", addressAt(0x09));
    if (rangeLow && rangeHigh) {
        activeRanges.append({rangeLow, rangeHigh,
                             rangeLow->minimum(), rangeHigh->maximum()});
        connect(rangeLow, &QAbstractSlider::valueChanged, this,
                &ModernSystemEditor::updateActiveRangeConstraints);
        connect(rangeHigh, &QAbstractSlider::valueChanged, this,
                &ModernSystemEditor::updateActiveRangeConstraints);
    }
}

QWidget *ModernSystemEditor::createPlayOptionPage()
{
    QWidget *scroll = createPage(tr("PLAY OPTION"));
    QWidget *page = qobject_cast<QScrollArea *>(scroll)->widget();
    auto *layout = qobject_cast<QVBoxLayout *>(page->layout());
    QWidget *patch = createSection(page, layout, tr("PATCH / BANK"));
    addSelector(patch, tr("Preamp Mode"), "00", "00", "10");
    addSelector(patch, tr("Patch Change Mode"), "00", "00", "11");
    addSelector(patch, tr("Bank Change Mode"), "00", "00", "12");
    addSelector(patch, tr("Bank Extent Min"), "00", "00", "13");
    addSelector(patch, tr("Bank Extent Max"), "00", "00", "15");
    QWidget *pedal = createSection(page, layout, tr("PEDAL"));
    addSelector(pedal, tr("EXP Pedal Hold"), "00", "00", "17");
    addSelector(pedal, tr("Pedal Indication"), "00", "00", "18");
    addSelector(pedal, tr("Number Pedal Control"), "00", "00", "70");
    QWidget *panel = createSection(page, layout, tr("PANEL"));
    addSelector(panel, tr("Dial Function"), "00", "00", "71");
    addSelector(panel, tr("Knob P1 Target"), "00", "00", "72");
    addSelector(panel, tr("Knob P2 Target"), "00", "00", "74");
    addSelector(panel, tr("Knob P3 Target"), "00", "00", "76");
    addSelector(panel, tr("Knob P4 Target"), "00", "00", "78");
    auto *note = new QLabel(
        tr("Patch and bank options affect the GT-10's physical switching behavior."),
        page);
    note->setObjectName("SystemNote");
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addStretch(1);
    return scroll;
}

QWidget *ModernSystemEditor::createInputPage()
{
    QWidget *scroll = createPage(tr("INPUT"));
    QWidget *page = qobject_cast<QScrollArea *>(scroll)->widget();
    auto *layout = qobject_cast<QVBoxLayout *>(page->layout());
    QWidget *source = createSection(page, layout, tr("INPUT SOURCE"));
    auto *selector = new QWidget(source);
    selector->setObjectName("SystemInputSelector");
    auto *selectorLayout = new QHBoxLayout(selector);
    selectorLayout->setContentsMargins(1, 1, 1, 1);
    selectorLayout->setSpacing(0);
    inputProfileGroup = new QButtonGroup(this);
    inputProfileGroup->setExclusive(true);
    const QStringList profiles = {tr("GUITAR 1"), tr("GUITAR 2"),
                                  tr("GUITAR 3"), tr("USB IN")};
    for (int profile = 0; profile < profiles.size(); ++profile) {
        auto *button = new QPushButton(profiles.at(profile), selector);
        button->setObjectName("SystemInputProfileButton");
        button->setCheckable(true);
        button->setProperty("segment", profile == 0 ? "first"
            : profile == profiles.size() - 1 ? "last" : "middle");
        inputProfileGroup->addButton(button, profile);
        selectorLayout->addWidget(button, 1);
    }
    connect(inputProfileGroup, &QButtonGroup::idClicked,
            this, &ModernSystemEditor::selectInputProfile);
    addControlToGrid(sectionGrid(source), selector);

    QWidget *parameters = createSection(page, layout, tr("INPUT PARAMETERS"));
    inputLevel = new ParameterBar(tr("LEVEL"), parameters);
    inputPresence = new ParameterBar(tr("PRES"), parameters);
    for (ParameterBar *bar : {inputLevel, inputPresence}) {
        bar->setRange(0x00, 0x28);
        bar->setCenterValue(0x14);
        bar->setValueReadoutFollowsHandle(true);
        bar->setAccentColor(ModernTheme::color(ModernTheme::EditorAccent));
        connect(bar, &QAbstractSlider::valueChanged, this,
                [this, bar](int raw) {
            if (!ready)
                return;
            const int offset = 0x40 + currentInputProfile * 2
                + (bar == inputPresence ? 1 : 0);
            const QString address = rawHex(offset);
            writeValue("00", "00", address, raw);
            bar->setDisplayText(displayForRaw("00", "00", address, raw));
        });
        addControlToGrid(sectionGrid(parameters), bar);
    }
    layout->addStretch(1);
    return scroll;
}

QWidget *ModernSystemEditor::createGlobalEqPage()
{
    QWidget *scroll = createPage(tr("GLOBAL EQ"));
    QWidget *page = qobject_cast<QScrollArea *>(scroll)->widget();
    auto *layout = qobject_cast<QVBoxLayout *>(page->layout());
    QWidget *low = createSection(page, layout, tr("LOW"));
    ParameterBar *lowGain = addBar(low, tr("Gain"), "00", "00", "48");
    QWidget *mid = createSection(page, layout, tr("MID"));
    ParameterBar *midGain = addBar(mid, tr("Gain"), "00", "00", "49");
    addSelector(mid, tr("Frequency"), "00", "00", "4B");
    addSelector(mid, tr("Q"), "00", "00", "4A");
    QWidget *high = createSection(page, layout, tr("HIGH"));
    ParameterBar *highGain = addBar(high, tr("Gain"), "00", "00", "4C");
    for (ParameterBar *bar : {lowGain, midGain, highGain})
        if (bar)
            bar->setCenterValue(0x0C);
    layout->addStretch(1);
    return scroll;
}

QWidget *ModernSystemEditor::createOutputPage()
{
    QWidget *scroll = createPage(tr("OUTPUT"));
    QWidget *page = qobject_cast<QScrollArea *>(scroll)->widget();
    auto *layout = qobject_cast<QVBoxLayout *>(page->layout());
    QWidget *global = createSection(page, layout, tr("GLOBAL / MAIN OUTPUT"));
    ParameterBar *nsThreshold = addBar(
        global, tr("Global NS Threshold"), "00", "00", "50");
    addBar(global, tr("Global Reverb Level"), "00", "00", "51");
    addSelector(global, tr("Main Out Level"), "00", "00", "52");
    if (nsThreshold)
        nsThreshold->setCenterValue(0x14);
    layout->addStretch(1);
    return scroll;
}

QWidget *ModernSystemEditor::createControllersPage()
{
    QWidget *scroll = createPage(tr("SYSTEM CONTROLLERS"));
    QWidget *page = qobject_cast<QScrollArea *>(scroll)->widget();
    auto *layout = qobject_cast<QVBoxLayout *>(page->layout());
    addControllerSection(page, layout, tr("EXP1"), "00", "7A", "10");
    addControllerSection(page, layout, tr("EXP SW"), "00", "7B", "20");
    addControllerSection(page, layout, tr("CTL1"), "00", "7C", "30");
    addControllerSection(page, layout, tr("CTL2"), "00", "7D", "40");
    addControllerSection(page, layout, tr("EXP2"), "00", "7E", "50");
    addControllerSection(page, layout, tr("CTL3"), "00", "7F", "60");
    addControllerSection(page, layout, tr("CTL4"), "01", "00", "70");
    auto *note = new QLabel(
        tr("These settings belong to the GT-10 System controller layer."),
        page);
    note->setObjectName("SystemNote");
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addStretch(1);
    return scroll;
}

QWidget *ModernSystemEditor::createPhraseLoopPage()
{
    QWidget *scroll = createPage(tr("PHRASE LOOP"));
    QWidget *page = qobject_cast<QScrollArea *>(scroll)->widget();
    auto *layout = qobject_cast<QVBoxLayout *>(page->layout());
    QWidget *configuration = createSection(page, layout, tr("SYSTEM CONFIGURATION"));
    addSelector(configuration, tr("Mode"), "00", "00", "60");
    addSelector(configuration, tr("Record Mode"), "00", "00", "61");
    addSelector(configuration, tr("Pedal Mode"), "00", "00", "62");
    addSelector(configuration, tr("Clear Pedal"), "00", "00", "63");
    addBar(configuration, tr("Play Level"), "00", "00", "64");
    auto *note = new QLabel(
        tr("Runtime REC, PLAY and OVERDUB states are not part of the known System buffer."),
        page);
    note->setObjectName("SystemNote");
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addStretch(1);
    return scroll;
}

QWidget *ModernSystemEditor::createLcdPage()
{
    QWidget *scroll = createPage(tr("LCD"));
    QWidget *page = qobject_cast<QScrollArea *>(scroll)->widget();
    auto *layout = qobject_cast<QVBoxLayout *>(page->layout());
    QWidget *display = createSection(page, layout, tr("DISPLAY"));
    addBar(display, tr("LCD Contrast"), "00", "00", "00");
    layout->addStretch(1);
    return scroll;
}

QWidget *ModernSystemEditor::createUsbPage()
{
    QWidget *scroll = createPage(tr("USB"));
    QWidget *page = qobject_cast<QScrollArea *>(scroll)->widget();
    auto *layout = qobject_cast<QVBoxLayout *>(page->layout());
    QWidget *usb = createSection(page, layout, tr("GT-10 USB SYSTEM"));
    addSelector(usb, tr("USB Driver Mode"), "00", "00", "20");
    addSelector(usb, tr("USB Monitor Command"), "00", "00", "21");
    addBar(usb, tr("USB Digital Out Level"), "00", "00", "22");
    addBar(usb, tr("USB Mix Level"), "00", "00", "23");
    auto *note = new QLabel(
        tr("USB input profile settings are available in INPUT."), page);
    note->setObjectName("SystemNote");
    layout->addWidget(note);
    layout->addStretch(1);
    return scroll;
}

QWidget *ModernSystemEditor::createCategoryNamesPage()
{
    QWidget *scroll = createPage(tr("CATEGORY NAMES"));
    QWidget *page = qobject_cast<QScrollArea *>(scroll)->widget();
    auto *layout = qobject_cast<QVBoxLayout *>(page->layout());
    QWidget *section = createSection(page, layout, tr("USER CATEGORIES"));
    QGridLayout *grid = sectionGrid(section);
    const QRegularExpression validCharacters(QStringLiteral("[\\x20-\\x7D]{0,8}"));
    for (int index = 0; index < 10; ++index) {
        auto *field = new QWidget(section);
        auto *fieldLayout = new QVBoxLayout(field);
        fieldLayout->setContentsMargins(0, 0, 0, 0);
        fieldLayout->setSpacing(8);
        auto *label = new QLabel(tr("CATEGORY %1").arg(index + 1), field);
        label->setObjectName("SystemFieldLabel");
        auto *editor = new QLineEdit(field);
        editor->setObjectName("SystemCategoryEditor");
        editor->setValidator(new QRegularExpressionValidator(validCharacters, editor));
        fieldLayout->addWidget(label);
        fieldLayout->addWidget(editor);
        const QString address = rawHex(index * 8);
        categories.append({editor, address});
        connect(editor, &QLineEdit::editingFinished,
                this, [this, index]() { commitCategoryName(index); });
        addControlToGrid(grid, field);
    }
    auto *note = new QLabel(
        tr("Names use the GT-10 legacy 8-byte character set."), page);
    note->setObjectName("SystemNote");
    layout->addWidget(note);
    layout->addStretch(1);
    return scroll;
}

bool ModernSystemEditor::containsValue(const QString &bank,
                                       const QString &page,
                                       const QString &address) const
{
    bool ok = false;
    const int offset = address.toInt(&ok, 16);
    if (!ok)
        return false;
    const SysxData source = SysxIO::Instance()->getSystemSource();
    const int block = source.address.indexOf(bank + page);
    return block >= 0 && block < source.hex.size()
        && source.hex.at(block).size() > sysxDataOffset + offset;
}

int ModernSystemEditor::rawValue(const QString &bank, const QString &page,
                                 const QString &address) const
{
    if (!containsValue(bank, page, address))
        return -1;
    bool offsetOk = false;
    const int offset = address.toInt(&offsetOk, 16);
    const SysxData source = SysxIO::Instance()->getSystemSource();
    const int block = source.address.indexOf(bank + page);
    bool valueOk = false;
    const int raw = source.hex.at(block).at(
        sysxDataOffset + offset).toInt(&valueOk, 16);
    return offsetOk && valueOk ? raw : -1;
}

QString ModernSystemEditor::displayForRaw(
    const QString &bank, const QString &page, const QString &address,
    int raw) const
{
    Midi catalog;
    if (!systemParameter(bank, page, address, &catalog))
        return QString::fromUtf8("—");
    bool directCatalog = true;
    for (const Midi &item : catalog.level) {
        bool rawOk = false;
        const int itemRaw = item.value.toInt(&rawOk, 16);
        if (!rawOk || !item.level.isEmpty()) {
            directCatalog = false;
            break;
        }
        if (itemRaw == raw) {
            QString display = item.customdesc;
            if (display.isEmpty())
                display = item.desc;
            if (display.isEmpty())
                display = item.name;
            display = display.trimmed();
            return display.isEmpty() ? QString::fromUtf8("—") : display;
        }
    }
    if (directCatalog)
        return QString::fromUtf8("—");
    const Midi &last = catalog.level.last();
    if (last.type.contains("DATA") && last.level.isEmpty())
        return QString::fromUtf8("—");
    QString display = MidiTable::Instance()->getValue(
        "System", bank, page, address, rawHex(raw)).trimmed();
    bool positiveGain = false;
    const int numericAddress = address.toInt(nullptr, 16);
    if (bank == "00" && page == "00") {
        positiveGain = (((numericAddress >= 0x40 && numericAddress <= 0x47)
                         || numericAddress == 0x50)
                        && raw > 0x14)
            || ((numericAddress == 0x48 || numericAddress == 0x49
                 || numericAddress == 0x4C) && raw > 0x0C);
    }
    if (positiveGain && !display.startsWith('+'))
        display.prepend('+');
    return display.isEmpty() ? QString::fromUtf8("—") : display;
}

void ModernSystemEditor::selectInputProfile(int profile)
{
    if (!ready || profile < 0 || profile > 3)
        return;
    currentInputProfile = profile;
    writeValue("00", "00", "4D", profile);
    refreshInputPage();
}

void ModernSystemEditor::refreshInputPage()
{
    if (!inputProfileGroup || !inputLevel || !inputPresence)
        return;
    const bool selectorAvailable = ready && containsValue("00", "00", "4D");
    if (selectorAvailable)
        currentInputProfile = qBound(0, rawValue("00", "00", "4D"), 3);
    for (QAbstractButton *button : inputProfileGroup->buttons())
        button->setEnabled(selectorAvailable);
    if (QAbstractButton *button = inputProfileGroup->button(currentInputProfile)) {
        const QSignalBlocker blocker(button);
        button->setChecked(selectorAvailable);
    }
    for (ParameterBar *bar : {inputLevel, inputPresence}) {
        const int offset = 0x40 + currentInputProfile * 2
            + (bar == inputPresence ? 1 : 0);
        const QString address = rawHex(offset);
        const bool available = selectorAvailable
            && containsValue("00", "00", address);
        bar->setEnabled(available);
        const QSignalBlocker blocker(bar);
        if (available) {
            const int raw = rawValue("00", "00", address);
            bar->setValue(raw);
            bar->setDisplayText(displayForRaw("00", "00", address, raw));
        } else {
            bar->setDisplayText(QString::fromUtf8("—"));
        }
    }
}

QString ModernSystemEditor::categoryName(const QString &address) const
{
    bool ok = false;
    const int offset = address.toInt(&ok, 16);
    if (!ok)
        return QString();
    const SysxData source = SysxIO::Instance()->getSystemSource();
    const int block = source.address.indexOf("0002");
    if (block < 0 || block >= source.hex.size()
        || source.hex.at(block).size() < sysxDataOffset + offset + 8)
        return QString();
    QByteArray bytes;
    bytes.reserve(8);
    for (int index = 0; index < 8; ++index) {
        bool byteOk = false;
        const int raw = source.hex.at(block).at(
            sysxDataOffset + offset + index).toInt(&byteOk, 16);
        if (!byteOk)
            return QString();
        bytes.append(static_cast<char>(raw));
    }
    return QString::fromLatin1(bytes).trimmed();
}

void ModernSystemEditor::writeValue(const QString &bank, const QString &page,
                                    const QString &address, int raw)
{
    SysxIO *sysxIO = SysxIO::Instance();
    if (!ready || !sysxIO->isConnected()
        || !containsValue(bank, page, address)
        || rawValue(bank, page, address) == raw)
        return;
    sysxIO->setFileSource("System", bank, page, address, rawHex(raw));
}

void ModernSystemEditor::commitCategoryName(int index)
{
    if (!ready || index < 0 || index >= categories.size())
        return;
    const CategoryField &field = categories.at(index);
    if (!field.editor || !field.editor->hasAcceptableInput())
        return;
    QByteArray bytes = field.editor->text().toLatin1();
    if (bytes.size() > 8)
        return;
    bytes.append(QByteArray(8 - bytes.size(), ' '));
    QList<QString> hexData;
    hexData.reserve(8);
    for (char byte : bytes)
        hexData.append(rawHex(static_cast<unsigned char>(byte)));

    bool offsetOk = false;
    const int offset = field.address.toInt(&offsetOk, 16);
    const SysxData source = SysxIO::Instance()->getSystemSource();
    const int block = source.address.indexOf("0002");
    if (!offsetOk || block < 0 || block >= source.hex.size()
        || source.hex.at(block).size() < sysxDataOffset + offset + 8)
        return;
    QList<QString> currentBytes;
    for (int byte = 0; byte < 8; ++byte)
        currentBytes.append(source.hex.at(block).at(
            sysxDataOffset + offset + byte).toUpper());
    if (currentBytes == hexData)
        return;
    SysxIO *sysxIO = SysxIO::Instance();
    if (!sysxIO->isConnected())
        return;
    sysxIO->setFileSource("System", "00", "02", field.address, hexData);
}

void ModernSystemEditor::updateActiveRangeConstraints()
{
    for (const ActiveRangePair &pair : activeRanges) {
        if (!pair.low || !pair.high)
            continue;
        const QSignalBlocker lowBlocker(pair.low);
        const QSignalBlocker highBlocker(pair.high);
        pair.low->setRange(pair.minimum,
                           qMax(pair.minimum, pair.high->value() - 1));
        pair.high->setRange(qMin(pair.maximum, pair.low->value() + 1),
                            pair.maximum);
    }
}

void ModernSystemEditor::refresh(bool backendConnected, bool systemDataReady)
{
    ready = backendConnected && systemDataReady
        && containsValue("00", "00", "10")
        && containsValue("00", "02", "4F");
    availability->setText(ready ? tr("EDITABLE · GT-10 SYSTEM BUFFER")
                                : tr("SYSTEM DATA UNAVAILABLE"));
    for (const ActiveRangePair &pair : activeRanges) {
        if (!pair.low || !pair.high)
            continue;
        const QSignalBlocker lowBlocker(pair.low);
        const QSignalBlocker highBlocker(pair.high);
        pair.low->setRange(pair.minimum, pair.maximum);
        pair.high->setRange(pair.minimum, pair.maximum);
    }
    for (const Field &field : fields) {
        const bool available = ready && field.catalogAvailable
            && containsValue(field.bank, field.page, field.address);
        field.control->setEnabled(available);
        const int raw = available
            ? rawValue(field.bank, field.page, field.address) : -1;
        if (field.kind == FieldKind::Selector && field.combo) {
            const QSignalBlocker blocker(field.combo);
            field.combo->setCurrentIndex(raw >= 0 ? field.combo->findData(raw) : -1);
        } else if (field.kind == FieldKind::Bar && field.bar) {
            const QSignalBlocker blocker(field.bar);
            if (raw >= 0) {
                field.bar->setValue(raw);
                field.bar->setDisplayText(displayForRaw(
                    field.bank, field.page, field.address, raw));
            } else {
                field.bar->setDisplayText(QString::fromUtf8("—"));
            }
        }
    }
    if (ready)
        updateActiveRangeConstraints();
    refreshInputPage();
    for (const CategoryField &field : categories) {
        const QSignalBlocker blocker(field.editor);
        field.editor->setEnabled(ready);
        field.editor->setText(ready ? categoryName(field.address)
                                    : QString::fromUtf8("—"));
    }
}
