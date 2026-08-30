#include "modernPedalboardEditor.h"

#include "MidiTable.h"
#include "SysxIO.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QScreen>
#include <QVBoxLayout>

#include <functional>

namespace {
QString stateBadgeText(ModernPedalboardModel::LogicalState state)
{
    switch (state) {
    case ModernPedalboardModel::LogicalState::Momentary:
        return QObject::tr("MOMENTARY");
    case ModernPedalboardModel::LogicalState::Unknown: return QString();
    default: return QString();
    }
}

QString scopeText(ModernPedalboardModel::DataScope scope)
{
    return scope == ModernPedalboardModel::DataScope::System
        ? QObject::tr("SYSTEM") : QObject::tr("PATCH");
}
}

class PedalboardFunctionSelector final : public QFrame
{
public:
    struct Item { int raw; QString label; };

    PedalboardFunctionSelector()
        : QFrame(nullptr, Qt::Popup | Qt::FramelessWindowHint)
    {
        setObjectName("PedalboardFunctionPopover");
        setFixedSize(330, 390);
        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(10, 10, 10, 10);
        layout->setSpacing(8);

        title = new QLabel;
        title->setObjectName("PedalboardFunctionPopoverTitle");
        layout->addWidget(title);

        search = new QLineEdit;
        search->setPlaceholderText(QObject::tr("Search function..."));
        search->setClearButtonEnabled(true);
        layout->addWidget(search);

        results = new QListWidget;
        results->setObjectName("PedalboardFunctionResults");
        results->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        layout->addWidget(results, 1);

        connect(search, &QLineEdit::textChanged,
                this, [this]() { rebuildResults(); });
        connect(search, &QLineEdit::returnPressed,
                this, [this]() { choose(results->currentItem()); });
        connect(results, &QListWidget::itemActivated,
                this, [this](QListWidgetItem *item) { choose(item); });
        connect(results, &QListWidget::itemClicked,
                this, [this](QListWidgetItem *item) { choose(item); });
        search->installEventFilter(this);
        results->installEventFilter(this);

        setStyleSheet(
            "QFrame#PedalboardFunctionPopover{background:#111820;"
            "border:1px solid #34414C;border-radius:7px;}"
            "QLabel#PedalboardFunctionPopoverTitle{color:#F2F4F6;"
            "font-size:11px;font-weight:700;letter-spacing:0.7px;}"
            "QLineEdit{background:#0B1117;color:#F2F4F6;"
            "border:1px solid #27313A;border-radius:5px;padding:6px 8px;"
            "selection-background-color:#12324D;}"
            "QLineEdit:focus{border-color:#00AEEF;}"
            "QListWidget#PedalboardFunctionResults{background:#0B1117;"
            "color:#F2F4F6;border:1px solid #27313A;border-radius:5px;"
            "outline:0px;}"
            "QListWidget#PedalboardFunctionResults::item{height:28px;"
            "padding:0 8px;}"
            "QListWidget#PedalboardFunctionResults::item:hover{"
            "background:#151D26;}"
            "QListWidget#PedalboardFunctionResults::item:selected{"
            "background:#12324D;color:#F2F4F6;}"
            "QScrollBar:vertical{background:#0B1117;width:8px;margin:2px;}"
            "QScrollBar::handle:vertical{background:#3A4651;min-height:28px;"
            "border-radius:4px;}"
            "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{"
            "height:0px;}"
            "QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical{"
            "background:transparent;}");
    }

    void openFor(QWidget *anchor, const QString &context,
                 const QVector<Item> &newItems, int raw,
                 const std::function<void(int)> &callback)
    {
        if (!anchor || newItems.isEmpty())
            return;
        items = newItems;
        currentRaw = raw;
        selected = callback;
        title->setText(QObject::tr("%1 FUNCTION").arg(context.toUpper()));
        search->clear();
        rebuildResults();

        const QPoint below = anchor->mapToGlobal(
            QPoint(0, anchor->height() + 5));
        QScreen *screen = QGuiApplication::screenAt(below);
        const QRect available = screen ? screen->availableGeometry()
                                       : QRect(below, size());
        const int x = qBound(available.left() + 6, below.x(),
                             available.right() - width() - 6);
        int y = below.y();
        if (y + height() > available.bottom() - 6)
            y = anchor->mapToGlobal(QPoint(0, -height() - 5)).y();
        y = qBound(available.top() + 6, y,
                   available.bottom() - height() - 6);
        move(x, y);
        show();
        raise();
        search->setFocus();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *key = static_cast<QKeyEvent *>(event);
            if (key->key() == Qt::Key_Escape) {
                hide();
                return true;
            }
            if (watched == search && (key->key() == Qt::Key_Down
                                      || key->key() == Qt::Key_Up)) {
                results->setFocus();
                int row = results->currentRow();
                if (row < 0)
                    row = 0;
                else if (key->key() == Qt::Key_Down)
                    row = qMin(row + 1, results->count() - 1);
                else
                    row = qMax(row - 1, 0);
                if (results->count() > 0)
                    results->setCurrentRow(row);
                return true;
            }
        }
        return QFrame::eventFilter(watched, event);
    }

private:
    void rebuildResults()
    {
        const QString query = search->text().trimmed();
        results->clear();
        QListWidgetItem *current = nullptr;
        for (const Item &entry : items) {
            if (!query.isEmpty()
                && !entry.label.contains(query, Qt::CaseInsensitive))
                continue;
            QListWidgetItem *item = new QListWidgetItem(
                entry.raw == currentRaw
                    ? QString::fromUtf8("✓  ") + entry.label
                    : QStringLiteral("    ") + entry.label,
                results);
            item->setData(Qt::UserRole, entry.raw);
            if (entry.raw == currentRaw)
                current = item;
        }
        if (current) {
            results->setCurrentItem(current);
            results->scrollToItem(current, QAbstractItemView::PositionAtCenter);
        } else if (results->count() > 0) {
            results->setCurrentRow(0);
        }
    }

    void choose(QListWidgetItem *item)
    {
        if (!item)
            return;
        const int raw = item->data(Qt::UserRole).toInt();
        hide();
        const std::function<void(int)> callback = selected;
        selected = {};
        if (callback)
            callback(raw);
    }

    QLabel *title = nullptr;
    QLineEdit *search = nullptr;
    QListWidget *results = nullptr;
    QVector<Item> items;
    int currentRaw = -1;
    std::function<void(int)> selected;
};

class PedalboardStateLed final : public QFrame
{
public:
    explicit PedalboardStateLed(QWidget *parent = nullptr)
        : QFrame(parent)
    {
        setObjectName("PedalboardStateLed");
        setFixedSize(10, 10);
    }

    void setLogicalState(ModernPedalboardModel::LogicalState state)
    {
        QString background;
        QString border;
        switch (state) {
        case ModernPedalboardModel::LogicalState::On:
            background = "#F0444B";
            border = "#FF777C";
            break;
        case ModernPedalboardModel::LogicalState::Off:
            background = "#54171B";
            border = "#772127";
            break;
        case ModernPedalboardModel::LogicalState::Momentary:
        case ModernPedalboardModel::LogicalState::Unknown:
            background = "#343D47";
            border = "#505C68";
            break;
        }
        setStyleSheet(QString(
            "QFrame#PedalboardStateLed{background:%1;border:1px solid %2;"
            "border-radius:5px;}").arg(background, border));
    }
};

class PedalboardModuleWidget final : public QFrame
{
public:
    explicit PedalboardModuleWidget(QWidget *parent = nullptr)
        : QFrame(parent)
    {
        setObjectName("PedalboardCard");
        setAttribute(Qt::WA_Hover);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        setMinimumWidth(116);

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setSizeConstraint(QLayout::SetMinimumSize);
        layout->setContentsMargins(13, 11, 13, 10);
        layout->setSpacing(6);

        QHBoxLayout *header = new QHBoxLayout;
        header->setContentsMargins(0, 0, 0, 0);
        header->setSpacing(8);
        physicalLabel = new QLabel;
        physicalLabel->setObjectName("PedalboardPhysicalLabel");
        header->addWidget(physicalLabel);
        header->addStretch(1);
        stateLed = new PedalboardStateLed;
        header->addWidget(stateLed, 0, Qt::AlignVCenter);
        layout->addLayout(header);

        functionLabel = new QLabel(QString::fromUtf8("—"));
        functionLabel->setObjectName("PedalboardFunctionLabel");
        functionLabel->setSizePolicy(QSizePolicy::Ignored,
                                     QSizePolicy::Preferred);
        functionLabel->setMinimumHeight(20);
        layout->addWidget(functionLabel);

        assignsLabel = new QLabel;
        assignsLabel->setObjectName("PedalboardAssignsLabel");
        assignsLabel->setSizePolicy(QSizePolicy::Ignored,
                                    QSizePolicy::Preferred);
        assignsLabel->setVisible(false);
        layout->addWidget(assignsLabel);

        layout->addStretch(1);

        QHBoxLayout *footer = new QHBoxLayout;
        footer->setContentsMargins(0, 0, 0, 0);
        footer->setSpacing(6);
        scopeLabel = new QLabel;
        scopeLabel->setObjectName("PedalboardScopeBadge");
        scopeLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        footer->addWidget(scopeLabel);

        stateBadge = new QLabel;
        stateBadge->setObjectName("PedalboardStateBadge");
        stateBadge->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        stateBadge->setVisible(false);
        footer->addWidget(stateBadge);
        footer->addStretch(1);

        navigationLabel = new QLabel(QObject::tr("CONTROL ASSIGN  ›"));
        navigationLabel->setObjectName("PedalboardNavigationLabel");
        navigationLabel->setVisible(false);
        footer->addWidget(navigationLabel);
        layout->addLayout(footer);
    }

    void setControlState(const ModernPedalboardModel::ControlState &state)
    {
        physicalLabel->setText(state.label);
        functionLabel->setText(state.functionName);
        functionLabel->setToolTip(state.functionName == QString::fromUtf8("—")
            ? QString() : state.functionName);
        stateLed->setLogicalState(state.state);
        scopeLabel->setText(scopeText(state.scope));

        const QString stateText = stateBadgeText(state.state);
        stateBadge->setText(stateText);
        stateBadge->setVisible(!stateText.isEmpty());

        QStringList assigns;
        for (int number : state.relatedAssigns)
            assigns.append(QString("A%1").arg(number));
        const QString assignText = assigns.join("   ");
        assignsLabel->setText(assignText);
        assignsLabel->setToolTip(assignText);
        assignsLabel->setVisible(!assigns.isEmpty());

        navigationLabel->setVisible(
            state.editor == ModernPedalboardModel::NavigationTarget::ControlAssign);
        setProperty("dataValid", state.dataValid);
        editable = state.scope == ModernPedalboardModel::DataScope::System
            && state.dataValid;
        navigable = state.editor
                == ModernPedalboardModel::NavigationTarget::ControlAssign
            && state.dataValid;
        setCursor(editable || navigable
            ? Qt::PointingHandCursor : Qt::ArrowCursor);
        setToolTip(editable ? QObject::tr("%1 FUNCTION").arg(state.label)
                            : navigable ? QObject::tr("OPEN CONTROL ASSIGN")
                                        : state.dataValid ? QString()
                                          : QObject::tr("Data unavailable"));
        style()->unpolish(this);
        style()->polish(this);
    }

    std::function<void()> activated;

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if ((editable || navigable) && event->button() == Qt::LeftButton
            && rect().contains(event->pos()) && activated) {
            activated();
            event->accept();
            return;
        }
        QFrame::mouseReleaseEvent(event);
    }

private:
    QLabel *physicalLabel = nullptr;
    QLabel *functionLabel = nullptr;
    QLabel *assignsLabel = nullptr;
    QLabel *scopeLabel = nullptr;
    QLabel *stateBadge = nullptr;
    QLabel *navigationLabel = nullptr;
    PedalboardStateLed *stateLed = nullptr;
    bool editable = false;
    bool navigable = false;
};

ModernPedalboardEditor::ModernPedalboardEditor(QObject *parent)
    : QObject(parent)
{
    functionSelector = new PedalboardFunctionSelector;
    buildEditor();
}

ModernPedalboardEditor::~ModernPedalboardEditor()
{
    delete functionSelector;
}

QWidget *ModernPedalboardEditor::widget() const
{
    return editor;
}

void ModernPedalboardEditor::buildEditor()
{
    QFrame *root = new QFrame;
    root->setObjectName("ModernPedalboardEditor");
    QVBoxLayout *layout = new QVBoxLayout(root);
    layout->setSizeConstraint(QLayout::SetMinimumSize);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(14);

    QLabel *title = new QLabel(tr("PEDALBOARD"));
    title->setObjectName("PedalboardTitle");
    layout->addWidget(title);

    for (int index = 0; index < 9; ++index) {
        PedalboardModuleWidget *module = new PedalboardModuleWidget;
        if (index < 6) {
            module->activated = [this, index]() {
                openFunctionSelector(index);
            };
        } else {
            module->activated = [this, index]() {
                navigateDirectControl(index);
            };
        }
        moduleWidgets.append(module);
    }

    QGridLayout *upperRow = new QGridLayout;
    upperRow->setContentsMargins(0, 0, 0, 0);
    upperRow->setHorizontalSpacing(12);
    for (int column = 0; column < 4; ++column) {
        upperRow->addWidget(moduleWidgets.at(column), 0, column);
        upperRow->setColumnStretch(column, 1);
    }
    upperRow->setRowStretch(0, 0);
    layout->addLayout(upperRow);

    QGridLayout *lowerRow = new QGridLayout;
    lowerRow->setContentsMargins(0, 0, 0, 0);
    lowerRow->setHorizontalSpacing(12);
    const int lowerIndexes[] = {6, 7, 5, 4, 8};
    for (int column = 0; column < 5; ++column) {
        lowerRow->addWidget(moduleWidgets.at(lowerIndexes[column]), 0, column);
        lowerRow->setColumnStretch(column, 1);
    }
    lowerRow->setRowStretch(0, 0);
    layout->addLayout(lowerRow);
    layout->addStretch(1);

    root->setStyleSheet(
        "QFrame#ModernPedalboardEditor{background:#08090B;"
        "border:1px solid #24272C;border-radius:4px;}"
        "QLabel#PedalboardTitle{color:#F2F4F6;font-size:16px;"
        "font-weight:700;letter-spacing:1px;}"
        "QFrame#PedalboardCard{background:#111923;"
        "border:1px solid #2B3845;border-radius:8px;}"
        "QFrame#PedalboardCard:hover{background:#131D28;"
        "border-color:#35495B;}"
        "QFrame#PedalboardCard[dataValid=\"false\"]{background:#0D131A;"
        "border-color:#222C35;}"
        "QLabel#PedalboardPhysicalLabel{color:#F2F4F6;font-size:10px;"
        "font-weight:700;letter-spacing:0.8px;border:none;}"
        "QLabel#PedalboardFunctionLabel{color:#D5DBE1;font-size:13px;"
        "font-weight:600;border:none;}"
        "QLabel#PedalboardAssignsLabel{color:#67A9CA;font-size:9px;"
        "font-weight:600;letter-spacing:0.5px;border:none;}"
        "QLabel#PedalboardScopeBadge,QLabel#PedalboardStateBadge{"
        "color:#84909C;font-size:8px;font-weight:600;letter-spacing:0.6px;"
        "background:#0C1218;border:1px solid #26313C;border-radius:3px;"
        "padding:2px 5px;}"
        "QLabel#PedalboardNavigationLabel{color:#6F9DB5;font-size:8px;"
        "font-weight:600;letter-spacing:0.3px;border:none;}"
        );
    editor = root;
}

void ModernPedalboardEditor::refresh(bool backendConnected,
                                     bool backendHasPatchData)
{
    this->backendConnected = backendConnected;
    this->backendHasPatchData = backendHasPatchData;
    model.refresh(backendConnected, backendHasPatchData);
    updateModules();

    const int loadedPatch = SysxIO::Instance()->getLoadedPatch();
    available = backendConnected && backendHasPatchData
        && loadedPatch >= 1 && loadedPatch <= 4;
    currentPatchFoot = available ? loadedPatch : 0;
    emit summaryChanged();
}

void ModernPedalboardEditor::updateModules()
{
    const int itemCount = qMin(model.count(), moduleWidgets.size());
    for (int index = 0; index < itemCount; ++index)
        moduleWidgets.at(index)->setControlState(model.control(index));
}

void ModernPedalboardEditor::openFunctionSelector(int index)
{
    if (!functionSelector || index < 0 || index >= 6
        || index >= model.count() || !SysxIO::Instance()->isConnected())
        return;

    const ModernPedalboardModel::ControlState &control = model.control(index);
    if (!control.dataValid
        || control.scope != ModernPedalboardModel::DataScope::System)
        return;

    const QString address = control.functionAddress.section(' ', 2, 2);
    const Midi catalog = MidiTable::Instance()->getMidiMap(
        "System", "00", "02", address);
    QVector<PedalboardFunctionSelector::Item> items;
    items.reserve(catalog.level.size());
    for (const Midi &entry : catalog.level) {
        bool ok = false;
        const int raw = entry.value.toInt(&ok, 16);
        const QString label = (!entry.customdesc.trimmed().isEmpty()
            ? entry.customdesc : !entry.desc.trimmed().isEmpty()
                ? entry.desc : entry.name).trimmed();
        if (ok && !label.isEmpty())
            items.append({raw, label});
    }
    functionSelector->openFor(
        moduleWidgets.at(index), control.label, items, control.functionRaw,
        [this, index](int raw) { functionSelected(index, raw); });
}

void ModernPedalboardEditor::functionSelected(int index, int raw)
{
    if (index < 0 || index >= 6 || index >= model.count()
        || !backendConnected || !SysxIO::Instance()->isConnected())
        return;
    const ModernPedalboardModel::ControlState &control = model.control(index);
    if (!control.dataValid || control.functionRaw == raw
        || control.scope != ModernPedalboardModel::DataScope::System)
        return;

    const QString address = control.functionAddress.section(' ', 2, 2);
    SysxIO::Instance()->setFileSource(
        "System", "00", "02", address,
        QString("%1").arg(raw, 2, 16, QChar('0')).toUpper());
    model.refresh(backendConnected, backendHasPatchData);
    updateModules();
    emit summaryChanged();
}

void ModernPedalboardEditor::navigateDirectControl(int index)
{
    if (index < 6 || index >= model.count())
        return;
    const ModernPedalboardModel::ControlState &control = model.control(index);
    if (!control.dataValid
        || control.editor
            != ModernPedalboardModel::NavigationTarget::ControlAssign)
        return;
    emit openControlAssignRequested(
        control.functionAddress.section(' ', 2, 2));
}

bool ModernPedalboardEditor::summaryAvailable() const
{
    return available;
}

int ModernPedalboardEditor::activePatchFoot() const
{
    return currentPatchFoot;
}

QVector<ModernPedalboardModel::LogicalState>
ModernPedalboardEditor::summaryStates() const
{
    using ControlId = ModernPedalboardModel::ControlId;
    using LogicalState = ModernPedalboardModel::LogicalState;
    const ControlId order[] = {
        ControlId::Foot1, ControlId::Foot2, ControlId::Foot3,
        ControlId::Foot4, ControlId::Ctl1, ControlId::Ctl2,
        ControlId::BankDown, ControlId::BankUp, ControlId::ExpSwitch
    };

    QVector<LogicalState> states;
    states.reserve(9);
    for (ControlId id : order) {
        const ModernPedalboardModel::ControlState *item = model.control(id);
        states.append(item ? item->state : LogicalState::Unknown);
    }
    return states;
}
