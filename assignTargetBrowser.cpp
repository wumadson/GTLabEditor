#include "assignTargetBrowser.h"

#include "MidiTable.h"

#include <QAbstractItemView>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

namespace {
const QString kStructure = "Structure";
const QString kCatalogBank = "0B";
const QString kMiddleByte = "00";
const QString kCatalogAddress = "21";

class TargetBrowserFrame final : public QFrame
{
public:
    explicit TargetBrowserFrame(Qt::WindowFlags flags)
        : QFrame(nullptr, flags)
    {
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF frameRect = QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0);
        QPainterPath framePath;
        framePath.addRoundedRect(frameRect, 8.0, 8.0);
        painter.fillPath(framePath, QColor("#111820"));
        painter.setPen(QPen(QColor("#526675"), 1.0));
        painter.drawPath(framePath);
    }
};

QString hexValue(int value)
{
    return QString("%1").arg(value, 2, 16, QChar('0')).toUpper();
}

QLabel *detailLabel(const QString &text)
{
    QLabel *label = new QLabel(text);
    label->setObjectName("AssignTargetDetailLabel");
    return label;
}

QLabel *detailValue()
{
    QLabel *label = new QLabel(QString::fromUtf8("—"));
    label->setObjectName("AssignTargetDetailValue");
    label->setWordWrap(true);
    return label;
}
}

AssignTargetBrowser::AssignTargetBrowser(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(130, 64);
    setMaximumWidth(216);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    QLabel *title = new QLabel("TARGET");
    title->setObjectName("ParameterLabel");
    field = new QPushButton(QString::fromUtf8("—  ▾"));
    field->setObjectName("AssignTargetField");
    field->setMinimumHeight(36);
    field->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(title);
    layout->addWidget(field);

    browser = new TargetBrowserFrame(Qt::Popup | Qt::FramelessWindowHint
                                               | Qt::NoDropShadowWindowHint);
    browser->setObjectName("AssignTargetBrowserPopup");
    browser->setAttribute(Qt::WA_TranslucentBackground);
    browser->setAutoFillBackground(false);
    browser->setFixedSize(720, 460);
    QVBoxLayout *browserLayout = new QVBoxLayout(browser);
    browserLayout->setContentsMargins(12, 12, 12, 12);
    browserLayout->setSpacing(9);
    QLabel *heading = new QLabel("TARGET BROWSER");
    heading->setObjectName("AssignTargetBrowserHeading");
    browserLayout->addWidget(heading);
    search = new QLineEdit;
    search->setPlaceholderText("Search target...");
    search->setClearButtonEnabled(true);
    browserLayout->addWidget(search);

    QHBoxLayout *content = new QHBoxLayout;
    content->setContentsMargins(0, 0, 0, 0);
    content->setSpacing(8);
    categories = new QListWidget;
    categories->setObjectName("AssignTargetCategories");
    categories->setFixedWidth(145);
    categories->setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    results = new QListWidget;
    results->setObjectName("AssignTargetResults");
    results->setMinimumWidth(285);
    results->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    content->addWidget(categories);
    content->addWidget(results, 1);

    QFrame *details = new QFrame;
    details->setObjectName("AssignTargetDetails");
    details->setFixedWidth(230);
    QGridLayout *detailsLayout = new QGridLayout(details);
    detailsLayout->setContentsMargins(10, 10, 10, 10);
    detailsLayout->setHorizontalSpacing(8);
    detailsLayout->setVerticalSpacing(8);
    const QStringList labels = {
        "TARGET ID", "CATEGORY", "TYPE", "RANGE", "CURRENT MIN", "CURRENT MAX"
    };
    QList<QLabel **> values = {
        &idValue, &categoryValue, &typeValue,
        &rangeValue, &minimumValue, &maximumValue
    };
    for (int row = 0; row < labels.size(); ++row) {
        detailsLayout->addWidget(detailLabel(labels.at(row)), row, 0,
                                 Qt::AlignLeft | Qt::AlignTop);
        *values.at(row) = detailValue();
        detailsLayout->addWidget(*values.at(row), row, 1,
                                 Qt::AlignLeft | Qt::AlignTop);
    }
    detailsLayout->setColumnStretch(1, 1);
    detailsLayout->setRowStretch(labels.size(), 1);
    QHBoxLayout *actions = new QHBoxLayout;
    actions->setContentsMargins(0, 0, 0, 0);
    actions->setSpacing(6);
    cancelButton = new QPushButton("CANCEL");
    cancelButton->setObjectName("AssignTargetCancel");
    applyButton = new QPushButton("APPLY TARGET");
    applyButton->setObjectName("AssignTargetApply");
    actions->addWidget(cancelButton);
    actions->addWidget(applyButton, 1);
    detailsLayout->addLayout(actions, labels.size() + 1, 0, 1, 2);
    content->addWidget(details);
    browserLayout->addLayout(content, 1);

    loadCatalog();
    rebuildCategories();
    connect(field, &QPushButton::clicked, this, [this]() { openBrowser(); });
    connect(search, &QLineEdit::textChanged,
            this, [this]() { rebuildResults(); });
    connect(categories, &QListWidget::currentRowChanged,
            this, [this](int) {
                if (search->text().isEmpty())
                    rebuildResults();
            });
    connect(results, &QListWidget::itemClicked,
            this, [this](QListWidgetItem *item) { previewItem(item); });
    connect(results, &QListWidget::itemActivated,
            this, [this](QListWidgetItem *item) { previewItem(item); });
    connect(cancelButton, &QPushButton::clicked,
            this, [this]() { cancelPreview(); });
    connect(applyButton, &QPushButton::clicked,
            this, [this]() { applyPreview(); });
    search->installEventFilter(this);
    categories->installEventFilter(this);
    results->installEventFilter(this);
    browser->installEventFilter(this);

    setStyleSheet(
        "QPushButton#AssignTargetField{background:rgba(15,25,34,235);"
        "color:#F2F4F6;border:1px solid #2B3945;border-radius:6px;"
        "padding:0 10px;text-align:left;font-size:12px;}"
        "QPushButton#AssignTargetField:hover{border-color:#3C4D5A;"
        "background:rgba(18,31,42,240);}"
        "QPushButton#AssignTargetField:focus{border-color:#00AEEF;}"
        "QFrame#AssignTargetBrowserPopup{background:transparent;border:none;}"
        "QLabel#AssignTargetBrowserHeading{color:#F2F4F6;font-size:11px;"
        "font-weight:600;letter-spacing:0.7px;}"
        "QLineEdit{background:#0B1117;color:#F2F4F6;border:1px solid #27313A;"
        "border-radius:5px;padding:6px 8px;selection-background-color:#12324D;}"
        "QLineEdit:focus{border-color:#00AEEF;}"
        "QListWidget{background:#0B1117;color:#DCE2E7;border:1px solid #27313A;"
        "border-radius:5px;outline:0px;}"
        "QListWidget#AssignTargetCategories{padding-bottom:4px;}"
        "QListWidget::item{height:27px;padding:0 8px;}"
        "QListWidget::item:hover{background:#151D26;}"
        "QListWidget::item:selected{background:#12324D;color:#F2F4F6;}"
        "QFrame#AssignTargetDetails{background:#0B1117;border:1px solid #27313A;"
        "border-radius:6px;}"
        "QLabel#AssignTargetDetailLabel{color:#7F8B96;font-size:8px;"
        "font-weight:600;letter-spacing:0.4px;}"
        "QLabel#AssignTargetDetailValue{color:#F2F4F6;font-size:10px;}"
        "QPushButton#AssignTargetCancel,QPushButton#AssignTargetApply{"
        "min-height:28px;border-radius:5px;font-size:9px;font-weight:600;}"
        "QPushButton#AssignTargetCancel{background:#0B1117;color:#9AA5B1;"
        "border:1px solid #27313A;}"
        "QPushButton#AssignTargetApply{background:#10263B;color:#F2F4F6;"
        "border:1px solid #00AEEF;}"
        "QPushButton#AssignTargetApply:disabled{color:#5F6871;"
        "background:#0B1117;border-color:#27313A;}"
        "QScrollBar:vertical{background:#0B1117;width:8px;margin:2px;}"
        "QScrollBar::handle:vertical{background:#3A4651;min-height:28px;"
        "border-radius:4px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0px;}"
        "QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical{"
        "background:transparent;}");
    browser->setStyleSheet(styleSheet());
    updateActions();
}

AssignTargetBrowser::~AssignTargetBrowser()
{
    delete browser;
}

void AssignTargetBrowser::loadCatalog()
{
    targets.clear();
    targets.reserve(619);
    MidiTable *table = MidiTable::Instance();
    for (int id = 0; id <= 618; ++id) {
        const QString high = hexValue(id / 128);
        const QString low = hexValue(id % 128);
        const Midi entry = table->getMidiMap(
            kStructure, kCatalogBank, kMiddleByte,
            kCatalogAddress, high, low);
        Target target;
        target.id = id;
        target.name = entry.name.trimmed();
        target.bank = entry.desc.trimmed();
        target.address = entry.customdesc.trimmed();
        target.category = categoryFor(target);
        target.type = isAction(target) ? "ACTION" : "PARAMETER";
        if (!target.bank.isEmpty() && !target.address.isEmpty()) {
            target.minimum = table->getRangeMinimum(
                kStructure, target.bank, kMiddleByte, target.address);
            target.maximum = table->getRange(
                kStructure, target.bank, kMiddleByte, target.address);
            target.rangeValid = target.minimum >= 0
                && target.maximum >= target.minimum
                && target.maximum <= 0x3FFF;
        }
        target.domain = rangeFor(target);
        targets.append(target);
        if (!categoryOrder.contains(target.category))
            categoryOrder.append(target.category);
    }
}

QString AssignTargetBrowser::categoryFor(const Target &target) const
{
    const QString name = target.name.toUpper();
    const QString prefix = name.section(':', 0, 0).trimmed();
    if (isAction(target)) return "ACTIONS";
    if (prefix == "PRE" || prefix == "CHANNEL" || prefix == "DYNAMIC SENSE")
        return "PREAMP / COMMON";
    if (prefix == "PRE_A") return "PREAMP / A";
    if (prefix == "PRE_B") return "PREAMP / B";
    if (prefix == "DD") return "DELAY";
    if (prefix == "CE") return "CHORUS";
    if (prefix == "RV") return "REVERB";
    if (prefix == "PDL FX" || prefix == "PDL_FX") return "PDL FX";
    if (prefix == "NS_1") return "NS1";
    if (prefix == "NS_2") return "NS2";
    return prefix.isEmpty() ? "OTHER" : prefix;
}

QString AssignTargetBrowser::displayNameFor(const Target &target) const
{
    const int separator = target.name.indexOf(':');
    return separator >= 0
        ? target.name.mid(separator + 1).trimmed()
        : target.name;
}

bool AssignTargetBrowser::isAction(const Target &target) const
{
    return target.bank.compare("0A", Qt::CaseInsensitive) == 0
        && (target.address.compare("7D", Qt::CaseInsensitive) == 0
            || target.address.compare("7E", Qt::CaseInsensitive) == 0
            || target.address.compare("7F", Qt::CaseInsensitive) == 0);
}

QString AssignTargetBrowser::rangeFor(const Target &target) const
{
    if (target.bank.isEmpty() || target.address.isEmpty())
        return QString::fromUtf8("—");
    MidiTable *table = MidiTable::Instance();
    const int minimum = table->getRangeMinimum(
        kStructure, target.bank, kMiddleByte, target.address);
    const int maximum = table->getRange(
        kStructure, target.bank, kMiddleByte, target.address);
    const QString minimumDisplay = table->getValue(
        kStructure, target.bank, kMiddleByte, target.address,
        hexValue(minimum)).trimmed();
    const QString maximumDisplay = table->getValue(
        kStructure, target.bank, kMiddleByte, target.address,
        hexValue(maximum)).trimmed();
    return QString("%1  …  %2")
        .arg(minimumDisplay.isEmpty() ? QString::number(minimum) : minimumDisplay)
        .arg(maximumDisplay.isEmpty() ? QString::number(maximum) : maximumDisplay);
}

void AssignTargetBrowser::setCurrentTarget(
    int targetId, const QString &minimum, const QString &maximum,
    int assignIndex, int loadedBank, int loadedPatch)
{
    const bool contextChanged = contextAssign != assignIndex
        || contextBank != loadedBank || contextPatch != loadedPatch;
    const bool targetChanged = currentTargetId != targetId;
    currentTargetId = targetId;
    currentMinimum = minimum;
    currentMaximum = maximum;
    contextAssign = assignIndex;
    contextBank = loadedBank;
    contextPatch = loadedPatch;
    if (contextChanged || targetChanged || !browser->isVisible()) {
        previewTargetId = currentTargetId;
        if (browser->isVisible() && (contextChanged || targetChanged))
            browser->hide();
    }
    if (targetId >= 0 && targetId < targets.size())
        field->setText(targets.at(targetId).name + QString::fromUtf8("  ▾"));
    else
        field->setText(QString::fromUtf8("—  ▾"));
    updateActions();
}

void AssignTargetBrowser::cancelPreview()
{
    previewTargetId = currentTargetId;
    browser->hide();
    updateActions();
}

void AssignTargetBrowser::openBrowser()
{
    search->clear();
    previewTargetId = currentTargetId;
    openedAssign = contextAssign;
    openedBank = contextBank;
    openedPatch = contextPatch;
    selectCurrentTarget();
    updateActions();
    const QPoint below = field->mapToGlobal(QPoint(0, field->height() + 5));
    QScreen *screen = QGuiApplication::screenAt(below);
    const QRect available = screen ? screen->availableGeometry()
                                   : QRect(below, browser->size());
    const int x = qBound(available.left() + 6, below.x(),
                         available.right() - browser->width() - 6);
    int y = below.y();
    if (y + browser->height() > available.bottom() - 6)
        y = field->mapToGlobal(QPoint(0, -browser->height() - 5)).y();
    y = qBound(available.top() + 6, y,
               available.bottom() - browser->height() - 6);
    browser->move(x, y);
    browser->show();
    browser->raise();
    search->setFocus();
}

void AssignTargetBrowser::rebuildCategories()
{
    categories->clear();
    for (const QString &category : categoryOrder) {
        QListWidgetItem *item = new QListWidgetItem(category, categories);
        item->setData(Qt::UserRole, category);
    }
}

void AssignTargetBrowser::rebuildResults()
{
    const QString query = search->text().trimmed();
    const QString category = categories->currentItem()
        ? categories->currentItem()->data(Qt::UserRole).toString() : QString();
    results->clear();
    QListWidgetItem *selected = nullptr;
    for (const Target &target : targets) {
        if (query.isEmpty() && !category.isEmpty() && target.category != category)
            continue;
        if (!query.isEmpty()) {
            const QString searchable = target.name + " " + target.category;
            if (!searchable.contains(query, Qt::CaseInsensitive))
                continue;
        }
        const QString displayName = displayNameFor(target);
        const QString itemText = query.isEmpty()
            ? displayName
            : QString("%1  ·  %2").arg(target.category, displayName);
        QListWidgetItem *item = new QListWidgetItem(itemText, results);
        item->setData(Qt::UserRole, target.id);
        if (target.type == "ACTION")
            item->setToolTip("ACTION");
        if (target.id == currentTargetId)
            selected = item;
    }
    if (selected) {
        results->setCurrentItem(selected);
        results->scrollToItem(selected, QAbstractItemView::PositionAtCenter);
    } else if (results->count() > 0) {
        results->setCurrentRow(0);
    }
    previewItem(results->currentItem());
}

void AssignTargetBrowser::previewItem(QListWidgetItem *item)
{
    if (!item)
        return;
    const int id = item->data(Qt::UserRole).toInt();
    if (id >= 0 && id < targets.size()) {
        previewTargetId = id;
        showDetails(targets.at(id));
        updateActions();
    }
}

void AssignTargetBrowser::updateActions()
{
    if (!applyButton || !cancelButton)
        return;
    const bool changed = previewTargetId >= 0
        && previewTargetId < targets.size()
        && previewTargetId != currentTargetId;
    applyButton->setVisible(changed);
    cancelButton->setVisible(changed);
    applyButton->setEnabled(changed && targets.at(previewTargetId).rangeValid);
}

void AssignTargetBrowser::applyPreview()
{
    if (previewTargetId < 0 || previewTargetId >= targets.size()
        || previewTargetId == currentTargetId)
        return;
    const Target &target = targets.at(previewTargetId);
    if (!target.rangeValid || !targetApplied)
        return;
    targetApplied(target.id, target.minimum, target.maximum,
                  currentTargetId, openedAssign, openedBank, openedPatch);
}

void AssignTargetBrowser::showDetails(const Target &target)
{
    idValue->setText(QString::number(target.id));
    categoryValue->setText(target.category);
    typeValue->setText(target.type);
    rangeValue->setText(target.domain);
    minimumValue->setText(currentMinimum.isEmpty()
        ? QString::fromUtf8("—") : currentMinimum);
    maximumValue->setText(currentMaximum.isEmpty()
        ? QString::fromUtf8("—") : currentMaximum);
}

void AssignTargetBrowser::selectCurrentTarget()
{
    if (currentTargetId < 0 || currentTargetId >= targets.size()) {
        if (categories->count() > 0)
            categories->setCurrentRow(0);
        rebuildResults();
        return;
    }
    const Target &current = targets.at(currentTargetId);
    for (int row = 0; row < categories->count(); ++row) {
        if (categories->item(row)->data(Qt::UserRole).toString()
            == current.category) {
            categories->setCurrentRow(row);
            break;
        }
    }
    rebuildResults();
    showDetails(current);
}

bool AssignTargetBrowser::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Escape) {
            cancelPreview();
            return true;
        }
        if (watched == search && (key->key() == Qt::Key_Down
                                  || key->key() == Qt::Key_Up)) {
            results->setFocus();
            if (results->count() > 0)
                results->setCurrentRow(qMax(0, results->currentRow()));
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
