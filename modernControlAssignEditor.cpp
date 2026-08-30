#include "modernControlAssignEditor.h"

#include "assignTargetBrowser.h"
#include "assignTargetValueEditor.h"
#include "MidiTable.h"
#include "SysxIO.h"
#include "globalVariables.h"
#include "modernWidgets.h"

#include <QAbstractItemView>
#include <QButtonGroup>
#include <QEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScreen>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <functional>

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

QString sourceDisplayLabel(int raw, const QString &original)
{
    if (raw == 0x09)
        return QStringLiteral("INPUT LEVEL");
    if (raw >= 0x0A && raw <= 0x28)
        return QString("CC#%1").arg(raw - 0x09, 2, 10, QChar('0'));
    if (raw >= 0x29 && raw <= 0x48)
        return QString("CC#%1").arg(raw - 0x29 + 64, 2, 10, QChar('0'));
    return original;
}

}

class AssignSourceSelector : public QWidget
{
public:
    struct Source { int raw; QString label; QString original; int category; };

    explicit AssignSourceSelector(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMinimumSize(130, 64);
        setMaximumWidth(216);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);
        QLabel *title = new QLabel(QObject::tr("SOURCE"));
        title->setObjectName("ParameterLabel");
        field = new QPushButton;
        field->setObjectName("AssignSourceField");
        field->setMinimumHeight(36);
        field->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        layout->addWidget(title);
        layout->addWidget(field);
        connect(field, &QPushButton::clicked, this, [this]() { openPopover(); });

        popover = new QFrame(nullptr, Qt::Popup | Qt::FramelessWindowHint);
        popover->setObjectName("AssignSourcePopover");
        popover->setFixedSize(340, 340);
        QVBoxLayout *popupLayout = new QVBoxLayout(popover);
        popupLayout->setContentsMargins(10, 10, 10, 10);
        popupLayout->setSpacing(8);
        search = new QLineEdit;
        search->setPlaceholderText(QObject::tr("Search source..."));
        search->setClearButtonEnabled(true);
        popupLayout->addWidget(search);
        QHBoxLayout *filters = new QHBoxLayout;
        filters->setContentsMargins(0, 0, 0, 0);
        filters->setSpacing(5);
        categoryGroup = new QButtonGroup(popover);
        categoryGroup->setExclusive(true);
        const QStringList names = {
            QObject::tr("PHYSICAL"), QObject::tr("GENERATORS"),
            QObject::tr("MIDI CC")
        };
        for (int category = 0; category < names.size(); ++category) {
            QPushButton *button = new QPushButton(names.at(category));
            button->setObjectName("AssignSourceCategory");
            button->setCheckable(true);
            categoryGroup->addButton(button, category);
            categoryButtons.append(button);
            filters->addWidget(button);
            connect(button, &QPushButton::clicked, this, [this, category]() {
                activeCategory = category;
                search->clear();
                rebuildResults();
            });
        }
        popupLayout->addLayout(filters);
        results = new QListWidget;
        results->setObjectName("AssignSourceResults");
        results->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        popupLayout->addWidget(results, 1);

        connect(search, &QLineEdit::textChanged, this,
                [this]() { rebuildResults(); });
        connect(search, &QLineEdit::returnPressed, this,
                [this]() { chooseItem(results->currentItem()); });
        connect(results, &QListWidget::itemActivated, this,
                [this](QListWidgetItem *item) { chooseItem(item); });
        connect(results, &QListWidget::itemClicked, this,
                [this](QListWidgetItem *item) { chooseItem(item); });
        search->installEventFilter(this);
        results->installEventFilter(this);
        popover->installEventFilter(this);
        setStyleSheet(
            "QPushButton#AssignSourceField{background:#0B1117;color:#F2F4F6;"
            "border:1px solid #27313A;border-radius:6px;padding:0 10px;"
            "text-align:left;font-size:12px;}"
            "QPushButton#AssignSourceField:focus,QPushButton#AssignSourceField:checked{"
            "border-color:#00AEEF;}"
            "QFrame#AssignSourcePopover{background:#111820;border:1px solid #34414C;"
            "border-radius:7px;}"
            "QLineEdit{background:#0B1117;color:#F2F4F6;border:1px solid #27313A;"
            "border-radius:5px;padding:6px 8px;selection-background-color:#12324D;}"
            "QLineEdit:focus{border-color:#00AEEF;}"
            "QPushButton#AssignSourceCategory{min-height:25px;background:#0B1117;"
            "color:#8E99A4;border:1px solid #27313A;border-radius:4px;font-size:9px;}"
            "QPushButton#AssignSourceCategory:checked{color:#F2F4F6;"
            "border-color:#00AEEF;background:#10263B;}"
            "QListWidget#AssignSourceResults{background:#0B1117;color:#F2F4F6;"
            "border:1px solid #27313A;border-radius:5px;outline:0px;}"
            "QListWidget#AssignSourceResults::item{height:27px;padding:0 8px;}"
            "QListWidget#AssignSourceResults::item:hover{background:#151D26;}"
            "QListWidget#AssignSourceResults::item:selected{background:#12324D;"
            "color:#F2F4F6;}"
            "QScrollBar:vertical{background:#0B1117;width:8px;margin:2px;}"
            "QScrollBar::handle:vertical{background:#3A4651;min-height:28px;"
            "border-radius:4px;}"
            "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0px;}"
            "QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical{background:transparent;}");
        popover->setStyleSheet(styleSheet());
    }

    ~AssignSourceSelector() override { delete popover; }

    void addSource(int raw, const QString &label, const QString &original)
    {
        sources.append({raw, label, original, raw <= 0x06 ? 0 : raw <= 0x09 ? 1 : 2});
    }
    void setCurrentRaw(int raw)
    {
        currentRaw = raw;
        for (const Source &source : sources)
            if (source.raw == raw) {
                field->setText(source.label + QString::fromUtf8("  ▾"));
                return;
            }
        field->setText(QString::fromUtf8("—  ▾"));
    }

    std::function<void(int)> sourceSelected;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *key = static_cast<QKeyEvent *>(event);
            if (key->key() == Qt::Key_Escape) {
                popover->hide();
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
        return QWidget::eventFilter(watched, event);
    }

private:
    void openPopover()
    {
        activeCategory = currentRaw <= 0x06 ? 0 : currentRaw <= 0x09 ? 1 : 2;
        categoryButtons.at(activeCategory)->setChecked(true);
        search->clear();
        rebuildResults();
        const QPoint below = field->mapToGlobal(QPoint(0, field->height() + 5));
        QScreen *screen = QGuiApplication::screenAt(below);
        const QRect available = screen ? screen->availableGeometry() : QRect(below, popover->size());
        int x = qBound(available.left() + 6, below.x(),
                       available.right() - popover->width() - 6);
        int y = below.y();
        if (y + popover->height() > available.bottom() - 6)
            y = field->mapToGlobal(QPoint(0, -popover->height() - 5)).y();
        y = qBound(available.top() + 6, y,
                   available.bottom() - popover->height() - 6);
        popover->move(x, y);
        popover->show();
        popover->raise();
        search->setFocus();
    }
    void rebuildResults()
    {
        const QString query = search->text().trimmed();
        results->clear();
        QListWidgetItem *selected = nullptr;
        for (const Source &source : sources) {
            if (query.isEmpty() && source.category != activeCategory)
                continue;
            if (!query.isEmpty()) {
                QString searchable = source.label + " " + source.original;
                searchable.replace("CTRL", "CTL", Qt::CaseInsensitive);
                if (!searchable.contains(query, Qt::CaseInsensitive))
                    continue;
            }
            QListWidgetItem *item = new QListWidgetItem(
                source.raw == currentRaw ? QString::fromUtf8("✓  ") + source.label
                                         : QStringLiteral("    ") + source.label,
                results);
            item->setData(Qt::UserRole, source.raw);
            item->setToolTip(source.original);
            if (source.raw == currentRaw)
                selected = item;
        }
        if (selected) {
            results->setCurrentItem(selected);
            results->scrollToItem(selected, QAbstractItemView::PositionAtCenter);
        } else if (results->count() > 0) {
            results->setCurrentRow(0);
        }
    }
    void chooseItem(QListWidgetItem *item)
    {
        if (!item)
            return;
        const int raw = item->data(Qt::UserRole).toInt();
        popover->hide();
        setCurrentRaw(raw);
        if (sourceSelected)
            sourceSelected(raw);
    }

    QPushButton *field = nullptr;
    QFrame *popover = nullptr;
    QLineEdit *search = nullptr;
    QListWidget *results = nullptr;
    QButtonGroup *categoryGroup = nullptr;
    QVector<QPushButton *> categoryButtons;
    QVector<Source> sources;
    int currentRaw = 0;
    int activeCategory = 0;
};

class DirectControlFunctionSelector : public QWidget
{
public:
    struct Function { int raw; QString label; int category; };

    explicit DirectControlFunctionSelector(const QString &label,
                                           QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(130, 64);
        setMaximumWidth(216);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);
        QLabel *title = new QLabel(label.toUpper());
        title->setObjectName("ControlLabel");
        field = new QPushButton;
        field->setObjectName("DirectControlFunctionField");
        field->setMinimumHeight(36);
        field->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        layout->addWidget(title);
        layout->addWidget(field);
        connect(field, &QPushButton::clicked, this, [this]() { openPopover(); });

        popover = new QFrame(nullptr, Qt::Popup | Qt::FramelessWindowHint);
        popover->setObjectName("DirectControlFunctionPopover");
        popover->setFixedSize(370, 350);
        QVBoxLayout *popupLayout = new QVBoxLayout(popover);
        popupLayout->setContentsMargins(10, 10, 10, 10);
        popupLayout->setSpacing(8);
        search = new QLineEdit;
        search->setPlaceholderText(QObject::tr("Search function..."));
        search->setClearButtonEnabled(true);
        popupLayout->addWidget(search);

        QGridLayout *filters = new QGridLayout;
        filters->setContentsMargins(0, 0, 0, 0);
        filters->setHorizontalSpacing(5);
        filters->setVerticalSpacing(5);
        categoryGroup = new QButtonGroup(popover);
        categoryGroup->setExclusive(true);
        const QStringList names = {
            QObject::tr("GENERAL"), QObject::tr("EFFECTS"),
            QObject::tr("PERFORMANCE"), QObject::tr("MIDI"),
            QObject::tr("PATCH"), QObject::tr("LED")
        };
        for (int category = 0; category < names.size(); ++category) {
            QPushButton *button = new QPushButton(names.at(category));
            button->setObjectName("DirectControlFunctionCategory");
            button->setCheckable(true);
            categoryGroup->addButton(button, category);
            categoryButtons.append(button);
            filters->addWidget(button, category / 3, category % 3);
            connect(button, &QPushButton::clicked, this, [this, category]() {
                activeCategory = category;
                search->clear();
                rebuildResults();
            });
        }
        popupLayout->addLayout(filters);
        results = new QListWidget;
        results->setObjectName("DirectControlFunctionResults");
        results->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        popupLayout->addWidget(results, 1);

        connect(search, &QLineEdit::textChanged,
                this, [this]() { rebuildResults(); });
        connect(search, &QLineEdit::returnPressed,
                this, [this]() { chooseItem(results->currentItem()); });
        connect(results, &QListWidget::itemActivated,
                this, [this](QListWidgetItem *item) { chooseItem(item); });
        connect(results, &QListWidget::itemClicked,
                this, [this](QListWidgetItem *item) { chooseItem(item); });
        search->installEventFilter(this);
        results->installEventFilter(this);
        popover->installEventFilter(this);

        setStyleSheet(
            "QPushButton#DirectControlFunctionField{background:rgba(15,25,34,235);"
            "color:#F2F4F6;border:1px solid #2B3945;border-radius:6px;"
            "padding:0 10px;text-align:left;font-size:12px;}"
            "QPushButton#DirectControlFunctionField:hover{border-color:#3C4D5A;"
            "background:rgba(18,31,42,240);}"
            "QPushButton#DirectControlFunctionField:focus{border-color:#00AEEF;}"
            "QFrame#DirectControlFunctionPopover{background:#111820;"
            "border:1px solid #34414C;border-radius:7px;}"
            "QLineEdit{background:#0B1117;color:#F2F4F6;border:1px solid #27313A;"
            "border-radius:5px;padding:6px 8px;selection-background-color:#12324D;}"
            "QLineEdit:focus{border-color:#00AEEF;}"
            "QPushButton#DirectControlFunctionCategory{min-height:25px;"
            "background:#0B1117;color:#8E99A4;border:1px solid #27313A;"
            "border-radius:4px;font-size:9px;}"
            "QPushButton#DirectControlFunctionCategory:checked{color:#F2F4F6;"
            "border-color:#00AEEF;background:#10263B;}"
            "QListWidget#DirectControlFunctionResults{background:#0B1117;"
            "color:#F2F4F6;border:1px solid #27313A;border-radius:5px;outline:0px;}"
            "QListWidget#DirectControlFunctionResults::item{height:27px;padding:0 8px;}"
            "QListWidget#DirectControlFunctionResults::item:hover{background:#151D26;}"
            "QListWidget#DirectControlFunctionResults::item:selected{"
            "background:#12324D;color:#F2F4F6;}"
            "QScrollBar:vertical{background:#0B1117;width:8px;margin:2px;}"
            "QScrollBar::handle:vertical{background:#3A4651;min-height:28px;"
            "border-radius:4px;}"
            "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0px;}"
            "QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical{"
            "background:transparent;}");
        popover->setStyleSheet(styleSheet());
        setCurrentRaw(-1);
    }

    ~DirectControlFunctionSelector() override { delete popover; }

    void addFunction(int raw, const QString &label)
    {
        int category = 0;
        if (raw >= 0x05 && raw <= 0x10)
            category = 1;
        else if (raw >= 0x11 && raw <= 0x18)
            category = 2;
        else if (raw >= 0x19 && raw <= 0x1A)
            category = 3;
        else if (raw >= 0x1B && raw <= 0x22)
            category = 4;
        else if (raw >= 0x23)
            category = 5;
        functions.append({raw, label, category});
    }

    void setCurrentRaw(int raw)
    {
        currentRaw = raw;
        for (const Function &function : functions) {
            if (function.raw == raw) {
                field->setText(function.label + QString::fromUtf8("  ▾"));
                return;
            }
        }
        field->setText(QString::fromUtf8("—  ▾"));
    }

    QString currentLabel() const
    {
        for (const Function &function : functions)
            if (function.raw == currentRaw)
                return function.label;
        return QString();
    }

    void focusForNavigation()
    {
        field->setFocus(Qt::OtherFocusReason);
    }

    std::function<void(int)> functionSelected;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *key = static_cast<QKeyEvent *>(event);
            if (key->key() == Qt::Key_Escape) {
                popover->hide();
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
        return QWidget::eventFilter(watched, event);
    }

private:
    int categoryForRaw(int raw) const
    {
        for (const Function &function : functions)
            if (function.raw == raw)
                return function.category;
        return 0;
    }

    void openPopover()
    {
        for (int category = 0; category < categoryButtons.size(); ++category) {
            bool categoryAvailable = false;
            for (const Function &function : functions) {
                if (function.category == category) {
                    categoryAvailable = true;
                    break;
                }
            }
            categoryButtons.at(category)->setVisible(categoryAvailable);
        }
        activeCategory = categoryForRaw(currentRaw);
        categoryButtons.at(activeCategory)->setChecked(true);
        search->clear();
        rebuildResults();
        const QPoint below = field->mapToGlobal(QPoint(0, field->height() + 5));
        QScreen *screen = QGuiApplication::screenAt(below);
        const QRect available = screen ? screen->availableGeometry()
                                       : QRect(below, popover->size());
        const int x = qBound(available.left() + 6, below.x(),
                             available.right() - popover->width() - 6);
        int y = below.y();
        if (y + popover->height() > available.bottom() - 6)
            y = field->mapToGlobal(QPoint(0, -popover->height() - 5)).y();
        y = qBound(available.top() + 6, y,
                   available.bottom() - popover->height() - 6);
        popover->move(x, y);
        popover->show();
        popover->raise();
        search->setFocus();
    }

    void rebuildResults()
    {
        const QString query = search->text().trimmed();
        results->clear();
        QListWidgetItem *selected = nullptr;
        for (const Function &function : functions) {
            if (query.isEmpty() && function.category != activeCategory)
                continue;
            if (!query.isEmpty()
                && !function.label.contains(query, Qt::CaseInsensitive))
                continue;
            QListWidgetItem *item = new QListWidgetItem(
                function.raw == currentRaw
                    ? QString::fromUtf8("✓  ") + function.label
                    : QStringLiteral("    ") + function.label,
                results);
            item->setData(Qt::UserRole, function.raw);
            if (function.raw == currentRaw)
                selected = item;
        }
        if (selected) {
            results->setCurrentItem(selected);
            results->scrollToItem(selected, QAbstractItemView::PositionAtCenter);
        } else if (results->count() > 0) {
            results->setCurrentRow(0);
        }
    }

    void chooseItem(QListWidgetItem *item)
    {
        if (!item)
            return;
        const int raw = item->data(Qt::UserRole).toInt();
        popover->hide();
        setCurrentRaw(raw);
        if (functionSelected)
            functionSelected(raw);
    }

    QPushButton *field = nullptr;
    QFrame *popover = nullptr;
    QLineEdit *search = nullptr;
    QListWidget *results = nullptr;
    QButtonGroup *categoryGroup = nullptr;
    QVector<QPushButton *> categoryButtons;
    QVector<Function> functions;
    int currentRaw = -1;
    int activeCategory = 0;
};

class AssignModeSelector : public QWidget
{
public:
    struct Option { int raw; QString label; };

    explicit AssignModeSelector(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMinimumSize(130, 64);
        setMaximumWidth(216);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);
        QLabel *title = new QLabel(QObject::tr("SOURCE MODE"));
        title->setObjectName("ParameterLabel");
        field = new QPushButton;
        field->setObjectName("AssignModeField");
        field->setFixedSize(130, 36);
        layout->addWidget(title);
        layout->addWidget(field);

        popover = new QFrame(nullptr, Qt::Popup | Qt::FramelessWindowHint);
        popover->setObjectName("AssignModePopover");
        popover->setFixedSize(180, 82);
        QVBoxLayout *popupLayout = new QVBoxLayout(popover);
        popupLayout->setContentsMargins(6, 6, 6, 6);
        popupLayout->setSpacing(3);
        list = new QListWidget;
        list->setObjectName("AssignModeResults");
        list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        popupLayout->addWidget(list);

        connect(field, &QPushButton::clicked, this, [this]() { openPopover(); });
        connect(list, &QListWidget::itemActivated,
                this, [this](QListWidgetItem *item) { chooseItem(item); });
        connect(list, &QListWidget::itemClicked,
                this, [this](QListWidgetItem *item) { chooseItem(item); });
        list->installEventFilter(this);
        popover->installEventFilter(this);
        setStyleSheet(
            "QPushButton#AssignModeField{background:rgba(15,25,34,235);"
            "color:#F2F4F6;border:1px solid #2B3945;border-radius:6px;"
            "padding:0 10px;text-align:left;font-size:12px;}"
            "QPushButton#AssignModeField:hover{border-color:#3C4D5A;"
            "background:rgba(18,31,42,240);}"
            "QPushButton#AssignModeField:focus{border-color:#00AEEF;}"
            "QFrame#AssignModePopover{background:#111820;border:1px solid #34414C;"
            "border-radius:7px;}"
            "QListWidget#AssignModeResults{background:#0B1117;color:#F2F4F6;"
            "border:1px solid #27313A;border-radius:5px;outline:0px;}"
            "QListWidget#AssignModeResults::item{height:30px;padding:0 8px;}"
            "QListWidget#AssignModeResults::item:hover{background:#151D26;}"
            "QListWidget#AssignModeResults::item:selected{background:#12324D;"
            "color:#F2F4F6;}");
        popover->setStyleSheet(styleSheet());
        setCurrentRaw(-1);
    }

    ~AssignModeSelector() override { delete popover; }

    void addOption(int raw, const QString &label)
    {
        options.append({raw, label});
    }

    void setCurrentRaw(int raw)
    {
        currentRaw = raw;
        for (const Option &option : options) {
            if (option.raw == raw) {
                field->setText(option.label + QString::fromUtf8("  ▾"));
                return;
            }
        }
        field->setText(QString::fromUtf8("—  ▾"));
    }

    std::function<void(int)> modeSelected;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *key = static_cast<QKeyEvent *>(event);
            if (key->key() == Qt::Key_Escape) {
                popover->hide();
                return true;
            }
        }
        return QWidget::eventFilter(watched, event);
    }

private:
    void openPopover()
    {
        list->clear();
        QListWidgetItem *selected = nullptr;
        for (const Option &option : options) {
            QListWidgetItem *item = new QListWidgetItem(
                option.raw == currentRaw
                    ? QString::fromUtf8("✓  ") + option.label
                    : QStringLiteral("    ") + option.label,
                list);
            item->setData(Qt::UserRole, option.raw);
            if (option.raw == currentRaw)
                selected = item;
        }
        if (selected)
            list->setCurrentItem(selected);
        else if (list->count() > 0)
            list->setCurrentRow(0);

        const QPoint below = field->mapToGlobal(QPoint(0, field->height() + 5));
        QScreen *screen = QGuiApplication::screenAt(below);
        const QRect available = screen ? screen->availableGeometry()
                                       : QRect(below, popover->size());
        const int x = qBound(available.left() + 6, below.x(),
                             available.right() - popover->width() - 6);
        int y = below.y();
        if (y + popover->height() > available.bottom() - 6)
            y = field->mapToGlobal(QPoint(0, -popover->height() - 5)).y();
        y = qBound(available.top() + 6, y,
                   available.bottom() - popover->height() - 6);
        popover->move(x, y);
        popover->show();
        popover->raise();
        list->setFocus();
    }

    void chooseItem(QListWidgetItem *item)
    {
        if (!item)
            return;
        const int raw = item->data(Qt::UserRole).toInt();
        popover->hide();
        setCurrentRaw(raw);
        if (modeSelected)
            modeSelected(raw);
    }

    QPushButton *field = nullptr;
    QFrame *popover = nullptr;
    QListWidget *list = nullptr;
    QVector<Option> options;
    int currentRaw = -1;
};

class AssignConditionalSelector : public QWidget
{
public:
    struct Option { int raw; QString label; };

    explicit AssignConditionalSelector(const QString &label,
                                       QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(130, 64);
        setMaximumWidth(216);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);
        QLabel *title = new QLabel(label.toUpper());
        title->setObjectName("ParameterLabel");
        field = new QPushButton;
        field->setObjectName("AssignConditionalField");
        field->setMinimumHeight(36);
        field->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        layout->addWidget(title);
        layout->addWidget(field);

        popover = new QFrame(nullptr, Qt::Popup | Qt::FramelessWindowHint);
        popover->setObjectName("AssignConditionalPopover");
        QVBoxLayout *popupLayout = new QVBoxLayout(popover);
        popupLayout->setContentsMargins(8, 8, 8, 8);
        popupLayout->setSpacing(6);
        search = new QLineEdit;
        search->setPlaceholderText(QObject::tr("Search..."));
        search->setClearButtonEnabled(true);
        popupLayout->addWidget(search);
        list = new QListWidget;
        list->setObjectName("AssignConditionalResults");
        list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        popupLayout->addWidget(list, 1);

        connect(field, &QPushButton::clicked, this, [this]() { openPopover(); });
        connect(search, &QLineEdit::textChanged,
                this, [this]() { rebuildResults(); });
        connect(search, &QLineEdit::returnPressed,
                this, [this]() { chooseItem(list->currentItem()); });
        connect(list, &QListWidget::itemActivated,
                this, [this](QListWidgetItem *item) { chooseItem(item); });
        connect(list, &QListWidget::itemClicked,
                this, [this](QListWidgetItem *item) { chooseItem(item); });
        search->installEventFilter(this);
        list->installEventFilter(this);
        popover->installEventFilter(this);

        setStyleSheet(
            "QPushButton#AssignConditionalField{background:rgba(15,25,34,235);"
            "color:#F2F4F6;border:1px solid #2B3945;border-radius:6px;"
            "padding:0 10px;text-align:left;font-size:12px;}"
            "QPushButton#AssignConditionalField:hover{border-color:#3C4D5A;"
            "background:rgba(18,31,42,240);}"
            "QPushButton#AssignConditionalField:focus{border-color:#00AEEF;}"
            "QFrame#AssignConditionalPopover{background:#111820;"
            "border:1px solid #34414C;border-radius:7px;}"
            "QLineEdit{background:#0B1117;color:#F2F4F6;border:1px solid #27313A;"
            "border-radius:5px;padding:6px 8px;selection-background-color:#12324D;}"
            "QLineEdit:focus{border-color:#00AEEF;}"
            "QListWidget#AssignConditionalResults{background:#0B1117;"
            "color:#F2F4F6;border:1px solid #27313A;border-radius:5px;outline:0px;}"
            "QListWidget#AssignConditionalResults::item{height:27px;padding:0 8px;}"
            "QListWidget#AssignConditionalResults::item:hover{background:#151D26;}"
            "QListWidget#AssignConditionalResults::item:selected{"
            "background:#12324D;color:#F2F4F6;}"
            "QScrollBar:vertical{background:#0B1117;width:8px;margin:2px;}"
            "QScrollBar::handle:vertical{background:#3A4651;min-height:28px;"
            "border-radius:4px;}"
            "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0px;}"
            "QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical{"
            "background:transparent;}");
        popover->setStyleSheet(styleSheet());
        setCurrentRaw(-1);
    }

    ~AssignConditionalSelector() override { delete popover; }

    void addOption(int raw, const QString &label)
    {
        options.append({raw, label});
    }

    void setCurrentRaw(int raw)
    {
        currentRaw = raw;
        for (const Option &option : options) {
            if (option.raw == raw) {
                field->setText(option.label + QString::fromUtf8("  ▾"));
                return;
            }
        }
        field->setText(QString::fromUtf8("—  ▾"));
    }

    std::function<void(int)> valueSelected;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *key = static_cast<QKeyEvent *>(event);
            if (key->key() == Qt::Key_Escape) {
                popover->hide();
                return true;
            }
            if (watched == search && (key->key() == Qt::Key_Down
                                      || key->key() == Qt::Key_Up)) {
                list->setFocus();
                int row = list->currentRow();
                if (row < 0)
                    row = 0;
                else if (key->key() == Qt::Key_Down)
                    row = qMin(row + 1, list->count() - 1);
                else
                    row = qMax(row - 1, 0);
                if (list->count() > 0)
                    list->setCurrentRow(row);
                return true;
            }
        }
        return QWidget::eventFilter(watched, event);
    }

private:
    void openPopover()
    {
        popover->setFixedSize(300, qBound(130, 58 + options.size() * 28, 320));
        search->clear();
        rebuildResults();
        const QPoint below = field->mapToGlobal(QPoint(0, field->height() + 5));
        QScreen *screen = QGuiApplication::screenAt(below);
        const QRect available = screen ? screen->availableGeometry()
                                       : QRect(below, popover->size());
        const int x = qBound(available.left() + 6, below.x(),
                             available.right() - popover->width() - 6);
        int y = below.y();
        if (y + popover->height() > available.bottom() - 6)
            y = field->mapToGlobal(QPoint(0, -popover->height() - 5)).y();
        y = qBound(available.top() + 6, y,
                   available.bottom() - popover->height() - 6);
        popover->move(x, y);
        popover->show();
        popover->raise();
        search->setFocus();
    }

    void rebuildResults()
    {
        const QString query = search->text().trimmed();
        list->clear();
        QListWidgetItem *selected = nullptr;
        for (const Option &option : options) {
            if (!query.isEmpty()
                && !option.label.contains(query, Qt::CaseInsensitive))
                continue;
            QListWidgetItem *item = new QListWidgetItem(
                option.raw == currentRaw
                    ? QString::fromUtf8("✓  ") + option.label
                    : QStringLiteral("    ") + option.label,
                list);
            item->setData(Qt::UserRole, option.raw);
            if (!selected && option.raw == currentRaw)
                selected = item;
        }
        if (selected) {
            list->setCurrentItem(selected);
            list->scrollToItem(selected, QAbstractItemView::PositionAtCenter);
        } else if (list->count() > 0) {
            list->setCurrentRow(0);
        }
    }

    void chooseItem(QListWidgetItem *item)
    {
        if (!item)
            return;
        const int raw = item->data(Qt::UserRole).toInt();
        popover->hide();
        setCurrentRaw(raw);
        if (valueSelected)
            valueSelected(raw);
    }

    QPushButton *field = nullptr;
    QFrame *popover = nullptr;
    QLineEdit *search = nullptr;
    QListWidget *list = nullptr;
    QVector<Option> options;
    int currentRaw = -1;
};

static void addConditionalCatalog(AssignConditionalSelector *selector,
                                  const QString &bank,
                                  const QString &address,
                                  bool expandRange = false)
{
    const Midi map = MidiTable::Instance()->getMidiMap(
        kStructure, bank, kMiddleByte, address);
    for (const Midi &entry : map.level) {
        if (entry.value == "range") {
            if (!expandRange)
                continue;
            const QStringList range = entry.name.split('/');
            if (range.size() < 2)
                continue;
            bool minimumOk = false;
            bool maximumOk = false;
            const int minimum = range.at(0).toInt(&minimumOk, 16);
            const int maximum = range.at(1).toInt(&maximumOk, 16);
            if (!minimumOk || !maximumOk)
                continue;
            for (int raw = minimum; raw <= maximum; ++raw) {
                const QString rawHex = QString("%1")
                    .arg(raw, 2, 16, QChar('0')).toUpper();
                QString display = MidiTable::Instance()->getValue(
                    kStructure, bank, kMiddleByte, address, rawHex).trimmed();
                if (display.isEmpty())
                    display = QString::number(raw);
                selector->addOption(raw, display);
            }
            continue;
        }
        bool ok = false;
        const int raw = entry.value.toInt(&ok, 16);
        if (ok)
            selector->addOption(raw, enumLabel(entry));
    }
}

class AssignRangeSpinBox : public QSpinBox
{
public:
    explicit AssignRangeSpinBox(QWidget *parent = nullptr) : QSpinBox(parent)
    {
        setObjectName("AssignRangeSpinBox");
        setKeyboardTracking(false);
        setButtonSymbols(QAbstractSpinBox::NoButtons);
        setAlignment(Qt::AlignCenter);
        setFixedSize(130, 36);
        setStyleSheet(
            "QSpinBox#AssignRangeSpinBox{background:rgba(15,25,34,235);"
            "color:#F2F4F6;border:1px solid #2B3945;border-radius:6px;"
            "padding:0 8px;font-size:12px;}"
            "QSpinBox#AssignRangeSpinBox:hover{border-color:#3C4D5A;"
            "background:rgba(18,31,42,240);}"
            "QSpinBox#AssignRangeSpinBox:focus{border-color:#00AEEF;}"
            "QSpinBox#AssignRangeSpinBox:disabled{color:#69747E;"
            "background:#0B1117;border-color:#202B34;}");
    }

protected:
    void wheelEvent(QWheelEvent *event) override { event->ignore(); }
};

ModernControlAssignEditor::ModernControlAssignEditor(QObject *parent)
    : QObject(parent)
{
    buildEditor();
}

EffectEditorPanel *ModernControlAssignEditor::widget() const
{
    return editor;
}

void ModernControlAssignEditor::buildEditor()
{
    editor = new EffectEditorPanel(tr("CONTROL ASSIGN"));
    editor->typeLabel()->hide();
    QWidget *artworkPane = editor->artworkArea()->parentWidget();
    if (artworkPane)
        artworkPane->hide();
    editor->setRightPanelTitle("ASSIGNS");
    const QList<QLabel *> editorLabels = editor->findChildren<QLabel *>();
    for (QLabel *label : editorLabels) {
        if (label->objectName() == "WorkspaceColumnTitle"
            && label->text() == "PARAMETERS") {
    label->setText(tr("CONTROL ASSIGN"));
            label->setObjectName("ControlAssignWorkspaceTitle");
            label->setStyleSheet(
                "color:#D4D7DB;font-size:14px;font-weight:700;"
                "letter-spacing:1px;");
            break;
        }
    }

    QVBoxLayout *layout = new QVBoxLayout(editor->parameterArea());
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    ParameterSection *controls = new ParameterSection(tr("DIRECT CONTROLS"), 3);
    const QList<QLabel *> controlLabels = controls->findChildren<QLabel *>();
    for (QLabel *label : controlLabels) {
        if (label->objectName() == "ParameterSectionTitle"
            && label->text() == "DIRECT CONTROLS") {
            label->hide();
            break;
        }
    }
    controls->setResponsiveColumns(3, 2, 1, 580, 390);
    controls->addControl(createDirectControl("EXP Switch Function", "46"));
    controls->addControl(createDirectControl("CTL1 Function", "47"));
    controls->addControl(createDirectControl("CTL2 Function", "48"));
    layout->addWidget(controls);
    layout->addSpacing(14);
    QFrame *divider = new QFrame;
    divider->setObjectName("WorkspaceRule");
    divider->setFixedHeight(1);
    layout->addWidget(divider);
    layout->addSpacing(14);
    layout->addWidget(createAssignDetail(), 1);
    editor->setRightPanelWidget(createAssignList());

    refresh(false, false);
}

QWidget *ModernControlAssignEditor::createAssignList()
{
    QWidget *panel = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);
    assignButtonGroup = new QButtonGroup(this);
    assignButtonGroup->setExclusive(true);
    for (int index = 0; index < 8; ++index) {
        QPushButton *button = new QPushButton;
        button->setObjectName("AssignReadOnlyRow");
        button->setCheckable(true);
        button->setMinimumHeight(40);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        assignButtonGroup->addButton(button, index);
        assignButtons.append(button);
        layout->addWidget(button);
        connect(button, &QPushButton::clicked,
                this, [this, index]() { selectAssign(index); });
    }
    assignButtons.first()->setChecked(true);
    layout->addStretch(1);
    panel->setStyleSheet(
        "QPushButton#AssignReadOnlyRow{padding:6px 8px;text-align:left;"
        "color:#9AA5B1;background:#0B1117;border:1px solid #27313A;"
        "border-radius:5px;font-size:10px;}"
        "QPushButton#AssignReadOnlyRow:checked{color:#F2F4F6;"
        "border-color:#00AEEF;background:#111D26;}"
        "QPushButton#AssignReadOnlyRow[assignEnabled=\"false\"]{"
        "color:#737D86;}"
        "QPushButton#AssignReadOnlyRow:disabled{color:#5F6871;}");
    return panel;
}

QWidget *ModernControlAssignEditor::createAssignDetail()
{
    QWidget *detail = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(detail);
    layout->setContentsMargins(0, 0, 8, 0);
    layout->setSpacing(6);

    QHBoxLayout *heading = new QHBoxLayout;
    heading->setContentsMargins(0, 0, 0, 0);
    detailTitle = new QLabel(tr("ASSIGN 1"));
    detailTitle->setObjectName("ParameterSectionTitle");
    heading->addWidget(detailTitle);
    heading->addStretch(1);
    layout->addLayout(heading);

    QVBoxLayout *stateLayout = new QVBoxLayout;
    stateLayout->setContentsMargins(0, 0, 0, 0);
    stateLayout->setSpacing(4);
    QLabel *stateLabel = new QLabel(tr("STATE"));
    stateLabel->setObjectName("ParameterLabel");
    detailStateToggle = new ModernToggleSwitch;
    detailStateToggle->setAccentColor(QColor("#00AEEF"));
    stateLayout->addWidget(stateLabel, 0, Qt::AlignLeft);
    stateLayout->addWidget(detailStateToggle, 0, Qt::AlignLeft);
    layout->addLayout(stateLayout);
    connect(detailStateToggle, &QAbstractButton::toggled,
            this, &ModernControlAssignEditor::assignStateChanged);

    QGridLayout *grid = new QGridLayout;
    detailGrid = grid;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(18);
    grid->setVerticalSpacing(16);
    const QStringList keys = {
        "source", "target", "min", "max", "mode", "low", "high",
        "trigger", "time", "curve", "rate", "form", "sensitivity"
    };
    const QStringList labels = {
        tr("SOURCE"), tr("TARGET"), tr("TARGET MIN"), tr("TARGET MAX"),
        tr("SOURCE MODE"), tr("ACTIVE RANGE LOW"),
        tr("ACTIVE RANGE HIGH"), tr("TRIGGER"), tr("TIME"),
        tr("CURVE"), tr("RATE"), tr("FORM"), tr("INPUT SENSITIVITY")
    };
    for (int index = 0; index < keys.size(); ++index) {
        const bool firstRow = index <= 3;
        int rowIndex = firstRow ? 0 : 1;
        int column = firstRow ? index * 3 : (index - 4) * 3;
        int columnSpan = 3;
        if (keys.at(index) == "trigger" || keys.at(index) == "rate"
            || keys.at(index) == "sensitivity") {
            rowIndex = 1;
            column = 0;
        } else if (keys.at(index) == "time" || keys.at(index) == "form") {
            rowIndex = 1;
            column = 3;
        } else if (keys.at(index) == "curve") {
            rowIndex = 1;
            column = 6;
        }
        QWidget *row = new QWidget;
        if (!firstRow) {
            row->setFixedHeight(64);
            row->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        }
        QVBoxLayout *rowLayout = new QVBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(8);
        rowLayout->setAlignment(Qt::AlignTop);
        if (keys.at(index) == "source") {
            detailSourceControl = new AssignSourceSelector;
            const Midi sourceMap = MidiTable::Instance()->getMidiMap(
                kStructure, "0B", kMiddleByte, "27");
            for (const Midi &entry : sourceMap.level) {
                if (entry.value == "range")
                    continue;
                bool ok = false;
                const int raw = entry.value.toInt(&ok, 16);
                if (!ok)
                    continue;
                const QString original = enumLabel(entry);
                detailSourceControl->addSource(
                    raw, sourceDisplayLabel(raw, original), original);
            }
            detailSourceControl->sourceSelected = [this](int raw) {
                assignSourceChanged(raw);
            };
            rowLayout->addWidget(detailSourceControl);
            detailRows.insert(keys.at(index), row);
            grid->addWidget(row, rowIndex, column, 1, columnSpan,
                            Qt::AlignLeft | Qt::AlignTop);
            continue;
        }
        if (keys.at(index) == "target") {
            detailTargetControl = new AssignTargetBrowser;
            detailTargetControl->targetApplied =
                [this](int targetId, int minimum, int maximum,
                       int expectedCurrentTarget, int assignIndex,
                       int loadedBank, int loadedPatch) {
                    assignTargetChanged(targetId, minimum, maximum,
                                        expectedCurrentTarget, assignIndex,
                                        loadedBank, loadedPatch);
                };
            rowLayout->addWidget(detailTargetControl);
            detailRows.insert(keys.at(index), row);
            grid->addWidget(row, rowIndex, column, 1, columnSpan,
                            Qt::AlignLeft | Qt::AlignTop);
            continue;
        }
        if (keys.at(index) == "min" || keys.at(index) == "max") {
            AssignTargetValueEditor *control =
                new AssignTargetValueEditor(labels.at(index));
            if (keys.at(index) == "min") {
                detailTargetMinimumControl = control;
                control->valueEdited = [this](int targetId, int raw) {
                    assignTargetMinimumChanged(targetId, raw);
                };
            } else {
                detailTargetMaximumControl = control;
                control->valueEdited = [this](int targetId, int raw) {
                    assignTargetMaximumChanged(targetId, raw);
                };
            }
            rowLayout->addWidget(control);
            detailRows.insert(keys.at(index), row);
            grid->addWidget(row, rowIndex, column, 1, columnSpan,
                            Qt::AlignLeft | Qt::AlignTop);
            continue;
        }
        if (keys.at(index) == "mode") {
            QLabel *name = new QLabel(labels.at(index));
            name->setObjectName("ParameterLabel");
            detailModeControl = new AssignModeSelector;
            QLabel *internalName = detailModeControl->findChild<QLabel *>(
                "ParameterLabel");
            if (internalName)
                internalName->hide();
            detailModeControl->setFixedHeight(36);
            const Midi modeMap = MidiTable::Instance()->getMidiMap(
                kStructure, "0B", kMiddleByte, "28");
            for (const Midi &entry : modeMap.level) {
                if (entry.value == "range")
                    continue;
                bool ok = false;
                const int raw = entry.value.toInt(&ok, 16);
                if (ok)
                    detailModeControl->addOption(raw, enumLabel(entry));
            }
            detailModeControl->modeSelected = [this](int raw) {
                assignModeChanged(raw);
            };
            rowLayout->addWidget(name);
            rowLayout->addWidget(detailModeControl);
            detailRows.insert(keys.at(index), row);
            grid->addWidget(row, rowIndex, column, 1, columnSpan,
                            Qt::AlignLeft | Qt::AlignTop);
            continue;
        }
        if (keys.at(index) == "low" || keys.at(index) == "high") {
            QLabel *name = new QLabel(labels.at(index));
            name->setObjectName("ParameterLabel");
            AssignRangeSpinBox *spin = new AssignRangeSpinBox;
            if (keys.at(index) == "low") {
                detailRangeLowControl = spin;
                spin->setRange(0, 126);
                connect(spin, &QSpinBox::editingFinished,
                        this, &ModernControlAssignEditor::assignRangeLowChanged);
            } else {
                detailRangeHighControl = spin;
                spin->setRange(1, 127);
                connect(spin, &QSpinBox::editingFinished,
                        this, &ModernControlAssignEditor::assignRangeHighChanged);
            }
            rowLayout->addWidget(name);
            rowLayout->addWidget(spin);
            detailRows.insert(keys.at(index), row);
            grid->addWidget(row, rowIndex, column, 1, columnSpan,
                            Qt::AlignLeft | Qt::AlignTop);
            continue;
        }
        if (keys.at(index) == "trigger" || keys.at(index) == "curve"
            || keys.at(index) == "rate" || keys.at(index) == "form") {
            AssignConditionalSelector *selector =
                new AssignConditionalSelector(labels.at(index));
            int offset = 0;
            ModernAssignModel::SourceKind sourceKind =
                ModernAssignModel::SourceKind::InternalPedal;
            if (keys.at(index) == "trigger") {
                offset = 0x0B;
                detailTriggerControl = selector;
                addConditionalCatalog(selector, "0B", "2B");
            } else if (keys.at(index) == "curve") {
                offset = 0x0D;
                detailCurveControl = selector;
                addConditionalCatalog(selector, "0B", "2D");
            } else if (keys.at(index) == "rate") {
                offset = 0x0E;
                sourceKind = ModernAssignModel::SourceKind::WavePedal;
                detailWaveRateControl = selector;
                addConditionalCatalog(selector, "0B", "2E", true);
            } else {
                offset = 0x0F;
                sourceKind = ModernAssignModel::SourceKind::WavePedal;
                detailWaveFormControl = selector;
                addConditionalCatalog(selector, "0B", "2F");
            }
            selector->valueSelected = [this, offset, sourceKind](int raw) {
                assignConditionalChanged(offset, raw, sourceKind);
            };
            rowLayout->addWidget(selector);
            detailRows.insert(keys.at(index), row);
            grid->addWidget(row, rowIndex, column, 1, columnSpan,
                            Qt::AlignLeft | Qt::AlignTop);
            continue;
        }
        if (keys.at(index) == "time" || keys.at(index) == "sensitivity") {
            QLabel *name = new QLabel(labels.at(index));
            name->setObjectName("ParameterLabel");
            AssignRangeSpinBox *spin = new AssignRangeSpinBox;
            spin->setRange(0, 100);
            if (keys.at(index) == "time") {
                detailInternalTimeControl = spin;
                connect(spin, &QSpinBox::editingFinished, this,
                        &ModernControlAssignEditor::assignInternalTimeChanged);
            } else {
                detailInputSensitivityControl = spin;
                connect(spin, &QSpinBox::editingFinished, this,
                        &ModernControlAssignEditor::assignInputSensitivityChanged);
            }
            rowLayout->addWidget(name);
            rowLayout->addWidget(spin);
            detailRows.insert(keys.at(index), row);
            grid->addWidget(row, rowIndex, column, 1, columnSpan,
                            Qt::AlignLeft | Qt::AlignTop);
            continue;
        }
        QLabel *name = new QLabel(labels.at(index));
        name->setObjectName("ParameterLabel");
        QLabel *value = new QLabel(QString::fromUtf8("—"));
        value->setObjectName("AssignReadOnlyValue");
        value->setWordWrap(true);
        rowLayout->addWidget(name);
        rowLayout->addWidget(value);
        detailRows.insert(keys.at(index), row);
        detailValues.insert(keys.at(index), value);
        grid->addWidget(row, rowIndex, column, 1, columnSpan,
                        Qt::AlignLeft | Qt::AlignTop);
    }
    for (int column = 0; column < 12; ++column)
        grid->setColumnStretch(column, 1);
    layout->addLayout(grid);
    layout->addStretch(1);
    detail->setStyleSheet(
        "QLabel#AssignReadOnlyValue{color:#F2F4F6;font-size:12px;}"
        "QLabel#ParameterLabel{color:#9AA5B1;font-size:9px;"
        "font-weight:600;letter-spacing:0.5px;}");
    return detail;
}

void ModernControlAssignEditor::selectAssign(int index)
{
    if (index < 0 || index >= 8)
        return;
    if (detailTargetControl)
        detailTargetControl->cancelPreview();
    selectedAssign = index;
    updateAssignList();
    updateAssignDetail();
    emit summaryChanged();
}

void ModernControlAssignEditor::assignStateChanged(bool enabled)
{
    if (refreshing || !available || selectedAssign < 0
        || selectedAssign >= assignModel.count())
        return;
    const ModernAssignModel::Record &record =
        assignModel.record(selectedAssign);
    if (!record.valid || record.enabled == enabled)
        return;

    SysxIO::Instance()->setFileSource(
        kStructure, record.bank, kMiddleByte, record.baseAddress,
        enabled ? "01" : "00");
    assignModel.refresh(available, available);
    updateAssignList();
    updateAssignDetail();
    emit summaryChanged();
}

void ModernControlAssignEditor::assignSourceChanged(int raw)
{
    if (refreshing || !available || raw < 0 || selectedAssign < 0
        || selectedAssign >= assignModel.count())
        return;

    const ModernAssignModel::Record &record = assignModel.record(selectedAssign);
    if (!record.valid)
        return;
    bool baseOk = false;
    const int base = record.baseAddress.toInt(&baseOk, 16);
    if (!baseOk || raw == record.source)
        return;

    const QString sourceAddress = QString("%1")
        .arg(base + 0x07, 2, 16, QChar('0')).toUpper();
    SysxIO::Instance()->setFileSource(
        kStructure, record.bank, kMiddleByte, sourceAddress,
        QString("%1").arg(raw, 2, 16, QChar('0')).toUpper());
    assignModel.refresh(available, available);
    updateAssignList();
    updateAssignDetail();
}

void ModernControlAssignEditor::assignTargetChanged(
    int targetId, int minimum, int maximum, int expectedCurrentTarget,
    int assignIndex, int loadedBank, int loadedPatch)
{
    SysxIO *sysx = SysxIO::Instance();
    if (refreshing || !available || targetId < 0 || targetId > 618
        || minimum < 0 || maximum < minimum || maximum > 0x3FFF
        || assignIndex != selectedAssign
        || loadedBank != sysx->getLoadedBank()
        || loadedPatch != sysx->getLoadedPatch()
        || !sysx->isConnected() || !sysx->deviceReady()
        || selectedAssign < 0 || selectedAssign >= assignModel.count())
        return;

    const ModernAssignModel::Record &record = assignModel.record(selectedAssign);
    bool baseOk = false;
    const int base = record.baseAddress.toInt(&baseOk, 16);
    if (!record.valid || !baseOk || record.targetId != expectedCurrentTarget)
        return;

    const QList<QString> targetData = {
        QString("%1").arg(targetId / 128, 2, 16, QChar('0')).toUpper(),
        QString("%1").arg(targetId % 128, 2, 16, QChar('0')).toUpper(),
        QString("%1").arg(minimum / 128, 2, 16, QChar('0')).toUpper(),
        QString("%1").arg(minimum % 128, 2, 16, QChar('0')).toUpper(),
        QString("%1").arg(maximum / 128, 2, 16, QChar('0')).toUpper(),
        QString("%1").arg(maximum % 128, 2, 16, QChar('0')).toUpper()
    };
    refreshing = true;
    sysx->setFileSource(
        kStructure, record.bank, kMiddleByte,
        QString("%1").arg(base + 0x01, 2, 16, QChar('0')).toUpper(),
        targetData);
    assignModel.refresh(true, true);
    updateAssignList();
    updateAssignDetail();
    refreshing = false;
}

void ModernControlAssignEditor::assignTargetMinimumChanged(
    int targetId, int raw)
{
    writeAssignTargetValue(0x03, targetId, raw);
}

void ModernControlAssignEditor::assignTargetMaximumChanged(
    int targetId, int raw)
{
    writeAssignTargetValue(0x05, targetId, raw);
}

void ModernControlAssignEditor::writeAssignTargetValue(
    int offset, int targetId, int raw)
{
    if (refreshing || !available || targetId < 0 || targetId > 618
        || (offset != 0x03 && offset != 0x05)
        || selectedAssign < 0 || selectedAssign >= assignModel.count())
        return;
    const ModernAssignModel::Record &record = assignModel.record(selectedAssign);
    bool baseOk = false;
    const int base = record.baseAddress.toInt(&baseOk, 16);
    if (!record.valid || !baseOk || record.targetId != targetId)
        return;

    const QString targetHigh = QString("%1")
        .arg(targetId / 128, 2, 16, QChar('0')).toUpper();
    const QString targetLow = QString("%1")
        .arg(targetId % 128, 2, 16, QChar('0')).toUpper();
    const Midi target = MidiTable::Instance()->getMidiMap(
        kStructure, "0B", kMiddleByte, "21", targetHigh, targetLow);
    const QString targetBank = target.desc.trimmed();
    const QString targetAddress = target.customdesc.trimmed();
    if (targetBank.isEmpty() || targetAddress.isEmpty())
        return;
    const int minimum = MidiTable::Instance()->getRangeMinimum(
        kStructure, targetBank, kMiddleByte, targetAddress);
    const int maximum = MidiTable::Instance()->getRange(
        kStructure, targetBank, kMiddleByte, targetAddress);
    if (raw < minimum || raw > maximum)
        return;

    const int current = offset == 0x03 ? record.targetMin : record.targetMax;
    if (current == raw)
        return;
    const QList<QString> bytes = {
        QString("%1").arg(raw / 128, 2, 16, QChar('0')).toUpper(),
        QString("%1").arg(raw % 128, 2, 16, QChar('0')).toUpper()
    };
    refreshing = true;
    SysxIO::Instance()->setFileSource(
        kStructure, record.bank, kMiddleByte,
        QString("%1").arg(base + offset, 2, 16, QChar('0')).toUpper(),
        bytes);
    assignModel.refresh(true, true);
    updateAssignList();
    updateAssignDetail();
    refreshing = false;
}

int ModernControlAssignEditor::readAssignOffset(int offset) const
{
    if (selectedAssign < 0 || selectedAssign >= assignModel.count())
        return -1;
    const ModernAssignModel::Record &record = assignModel.record(selectedAssign);
    bool baseOk = false;
    const int base = record.baseAddress.toInt(&baseOk, 16);
    if (!record.valid || !baseOk)
        return -1;
    return SysxIO::Instance()->getSourceValue(
        kStructure, record.bank, kMiddleByte,
        QString("%1").arg(base + offset, 2, 16, QChar('0')).toUpper());
}

void ModernControlAssignEditor::writeAssignOffset(int offset, int raw)
{
    if (refreshing || !available || selectedAssign < 0
        || selectedAssign >= assignModel.count())
        return;
    const ModernAssignModel::Record &record = assignModel.record(selectedAssign);
    bool baseOk = false;
    const int base = record.baseAddress.toInt(&baseOk, 16);
    if (!record.valid || !baseOk || readAssignOffset(offset) == raw)
        return;
    SysxIO::Instance()->setFileSource(
        kStructure, record.bank, kMiddleByte,
        QString("%1").arg(base + offset, 2, 16, QChar('0')).toUpper(),
        QString("%1").arg(raw, 2, 16, QChar('0')).toUpper());
    assignModel.refresh(available, available);
    updateAssignList();
    updateAssignDetail();
}

void ModernControlAssignEditor::assignModeChanged(int raw)
{
    if (selectedAssign < 0 || selectedAssign >= assignModel.count()
        || assignModel.record(selectedAssign).sourceKind
            != ModernAssignModel::SourceKind::Normal)
        return;
    writeAssignOffset(0x08, raw);
}

void ModernControlAssignEditor::assignRangeLowChanged()
{
    if (!detailRangeLowControl || selectedAssign < 0
        || selectedAssign >= assignModel.count()
        || assignModel.record(selectedAssign).sourceKind
            != ModernAssignModel::SourceKind::Normal)
        return;
    writeAssignOffset(0x09, detailRangeLowControl->value());
}

void ModernControlAssignEditor::assignRangeHighChanged()
{
    if (!detailRangeHighControl || selectedAssign < 0
        || selectedAssign >= assignModel.count()
        || assignModel.record(selectedAssign).sourceKind
            != ModernAssignModel::SourceKind::Normal)
        return;
    writeAssignOffset(0x0A, detailRangeHighControl->value());
}

void ModernControlAssignEditor::assignConditionalChanged(
    int offset, int raw, ModernAssignModel::SourceKind sourceKind)
{
    if (selectedAssign < 0 || selectedAssign >= assignModel.count()
        || assignModel.record(selectedAssign).sourceKind != sourceKind)
        return;
    writeAssignOffset(offset, raw);
}

void ModernControlAssignEditor::assignInternalTimeChanged()
{
    if (!detailInternalTimeControl || selectedAssign < 0
        || selectedAssign >= assignModel.count()
        || assignModel.record(selectedAssign).sourceKind
            != ModernAssignModel::SourceKind::InternalPedal)
        return;
    writeAssignOffset(0x0C, detailInternalTimeControl->value());
}

void ModernControlAssignEditor::assignInputSensitivityChanged()
{
    if (refreshing || !available || !detailInputSensitivityControl
        || selectedAssign < 0 || selectedAssign >= assignModel.count()
        || assignModel.record(selectedAssign).sourceKind
            != ModernAssignModel::SourceKind::InputLevel)
        return;
    const int raw = detailInputSensitivityControl->value();
    if (SysxIO::Instance()->getSourceValue(
            kStructure, "0C", kMiddleByte, "20") == raw)
        return;
    SysxIO::Instance()->setFileSource(
        kStructure, "0C", kMiddleByte, "20",
        QString("%1").arg(raw, 2, 16, QChar('0')).toUpper());
    assignModel.refresh(available, available);
    updateAssignList();
    updateAssignDetail();
}

void ModernControlAssignEditor::setDetailRow(
    const QString &key, const QString &, const QString &value, bool visible)
{
    if (detailRows.contains(key))
        detailRows.value(key)->setVisible(visible);
    if (detailValues.contains(key))
        detailValues.value(key)->setText(value.isEmpty()
            ? QString::fromUtf8("—") : value);
}

void ModernControlAssignEditor::updateAssignList()
{
    for (int index = 0; index < assignButtons.size(); ++index) {
        QPushButton *button = assignButtons.at(index);
        if (index == selectedAssign && !button->isChecked())
            button->setChecked(true);
        if (!assignModel.isAvailable()) {
            button->setText(tr("ASSIGN %1\n—").arg(index + 1));
            button->setEnabled(false);
            continue;
        }
        const ModernAssignModel::Record &record = assignModel.record(index);
        button->setEnabled(record.valid);
        button->setText(tr("ASSIGN %1   %2\n%3  →  %4")
            .arg(index + 1)
            .arg(record.enabled ? tr("ON") : tr("OFF"))
            .arg(record.sourceDisplay)
            .arg(record.targetName));
        button->setProperty("assignEnabled", record.enabled);
        button->style()->unpolish(button);
        button->style()->polish(button);
    }
}

void ModernControlAssignEditor::updateAssignDetail()
{
    const bool valid = assignModel.isAvailable()
        && selectedAssign >= 0 && selectedAssign < assignModel.count()
        && assignModel.record(selectedAssign).valid;
    detailTitle->setText(tr("ASSIGN %1").arg(selectedAssign + 1));
    detailStateToggle->setEnabled(valid);
    detailStateToggle->setCheckedFromBackend(
        valid && assignModel.record(selectedAssign).enabled);

    if (!valid) {
        detailSourceControl->setEnabled(false);
        detailSourceControl->setCurrentRaw(-1);
        detailTargetControl->setEnabled(false);
        detailTargetControl->setCurrentTarget(
            -1, QString(), QString(), selectedAssign,
            SysxIO::Instance()->getLoadedBank(),
            SysxIO::Instance()->getLoadedPatch());
        detailTargetMinimumControl->setTargetValue(-1, 0, false);
        detailTargetMaximumControl->setTargetValue(-1, 0, false);
        detailModeControl->setEnabled(false);
        detailModeControl->setCurrentRaw(-1);
        detailRangeLowControl->setEnabled(false);
        detailRangeHighControl->setEnabled(false);
        detailRangeLowControl->clear();
        detailRangeHighControl->clear();
        const QList<AssignConditionalSelector *> conditionalSelectors = {
            detailTriggerControl, detailCurveControl,
            detailWaveRateControl, detailWaveFormControl
        };
        for (AssignConditionalSelector *selector : conditionalSelectors) {
            selector->setEnabled(false);
            selector->setCurrentRaw(-1);
        }
        detailInternalTimeControl->setEnabled(false);
        detailInputSensitivityControl->setEnabled(false);
        detailInternalTimeControl->clear();
        detailInputSensitivityControl->clear();
        for (QLabel *value : detailValues)
            value->setText(QString::fromUtf8("—"));
        detailRows.value("mode")->show();
        detailRows.value("low")->show();
        detailRows.value("high")->show();
        detailRows.value("trigger")->hide();
        detailRows.value("time")->hide();
        detailRows.value("curve")->hide();
        detailRows.value("rate")->hide();
        detailRows.value("form")->hide();
        detailRows.value("sensitivity")->hide();
        return;
    }
    const ModernAssignModel::Record &record = assignModel.record(selectedAssign);
    detailSourceControl->setEnabled(true);
    detailSourceControl->setCurrentRaw(record.source);
    detailTargetControl->setEnabled(true);
    detailTargetControl->setCurrentTarget(
        record.targetId, record.targetMinDisplay, record.targetMaxDisplay,
        selectedAssign, SysxIO::Instance()->getLoadedBank(),
        SysxIO::Instance()->getLoadedPatch());
    detailTargetMinimumControl->setTargetValue(
        record.targetId, record.targetMin, true);
    detailTargetMaximumControl->setTargetValue(
        record.targetId, record.targetMax, true);

    const bool normal = record.sourceKind == ModernAssignModel::SourceKind::Normal;
    const bool internal = record.sourceKind
        == ModernAssignModel::SourceKind::InternalPedal;
    const bool wave = record.sourceKind
        == ModernAssignModel::SourceKind::WavePedal;
    const bool input = record.sourceKind
        == ModernAssignModel::SourceKind::InputLevel;
    detailRows.value("mode")->setVisible(normal);
    detailRows.value("low")->setVisible(normal);
    detailRows.value("high")->setVisible(normal);
    detailModeControl->setEnabled(normal);
    detailRangeLowControl->setEnabled(normal);
    detailRangeHighControl->setEnabled(normal);
    if (normal) {
        const QSignalBlocker modeBlocker(detailModeControl);
        const QSignalBlocker lowBlocker(detailRangeLowControl);
        const QSignalBlocker highBlocker(detailRangeHighControl);
        detailModeControl->setCurrentRaw(readAssignOffset(0x08));
        detailRangeLowControl->setRange(
            0, qBound(0, record.activeRangeHigh - 1, 126));
        detailRangeHighControl->setRange(
            qBound(1, record.activeRangeLow + 1, 127), 127);
        detailRangeLowControl->setValue(record.activeRangeLow);
        detailRangeHighControl->setValue(record.activeRangeHigh);
    }
    detailRows.value("trigger")->setVisible(internal);
    detailRows.value("time")->setVisible(internal);
    detailRows.value("curve")->setVisible(internal);
    detailRows.value("rate")->setVisible(wave);
    detailRows.value("form")->setVisible(wave);
    detailRows.value("sensitivity")->setVisible(input);
    detailTriggerControl->setEnabled(internal);
    detailInternalTimeControl->setEnabled(internal);
    detailCurveControl->setEnabled(internal);
    detailWaveRateControl->setEnabled(wave);
    detailWaveFormControl->setEnabled(wave);
    detailInputSensitivityControl->setEnabled(input);
    if (internal) {
        const QSignalBlocker triggerBlocker(detailTriggerControl);
        const QSignalBlocker timeBlocker(detailInternalTimeControl);
        const QSignalBlocker curveBlocker(detailCurveControl);
        detailTriggerControl->setCurrentRaw(readAssignOffset(0x0B));
        detailInternalTimeControl->setValue(readAssignOffset(0x0C));
        detailCurveControl->setCurrentRaw(readAssignOffset(0x0D));
    } else if (wave) {
        const QSignalBlocker rateBlocker(detailWaveRateControl);
        const QSignalBlocker formBlocker(detailWaveFormControl);
        detailWaveRateControl->setCurrentRaw(readAssignOffset(0x0E));
        detailWaveFormControl->setCurrentRaw(readAssignOffset(0x0F));
    } else if (input) {
        const QSignalBlocker sensitivityBlocker(detailInputSensitivityControl);
        detailInputSensitivityControl->setValue(
            SysxIO::Instance()->getSourceValue(
                kStructure, "0C", kMiddleByte, "20"));
    }
}

QWidget *ModernControlAssignEditor::createDirectControl(
    const QString &label, const QString &address)
{
    DirectControlFunctionSelector *control =
        new DirectControlFunctionSelector(label);
    const Midi map = MidiTable::Instance()->getMidiMap(
        kStructure, kBank, kMiddleByte, address);
    for (const Midi &entry : map.level) {
        if (entry.value == "range")
            continue;
        bool ok = false;
        const int raw = entry.value.toInt(&ok, 16);
        if (ok)
            control->addFunction(raw, enumLabel(entry));
    }

    control->functionSelected = [this, address](int raw) {
        if (refreshing || !available)
            return;
        writeValue(address, raw);
    };

    DirectControlBinding binding;
    binding.address = address;
    binding.control = control;
    bindings.append(binding);
    return control;
}

bool ModernControlAssignEditor::bufferContains(const QString &address) const
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

bool ModernControlAssignEditor::hasValidBuffer(
    bool backendConnected, bool backendHasPatchData) const
{
    return backendConnected && backendHasPatchData
        && SysxIO::Instance()->isConnected()
        && bufferContains("46") && bufferContains("47")
        && bufferContains("48");
}

int ModernControlAssignEditor::readValue(const QString &address) const
{
    return SysxIO::Instance()->getSourceValue(
        kStructure, kBank, kMiddleByte, address);
}

void ModernControlAssignEditor::writeValue(const QString &address, int raw)
{
    if (!available || !bufferContains(address))
        return;
    SysxIO::Instance()->setFileSource(
        kStructure, kBank, kMiddleByte, address,
        QString("%1").arg(raw, 2, 16, QChar('0')).toUpper());
    emit summaryChanged();
}

void ModernControlAssignEditor::refresh(bool backendConnected,
                                        bool backendHasPatchData)
{
    refreshing = true;
    available = hasValidBuffer(backendConnected, backendHasPatchData);
    for (DirectControlBinding &binding : bindings) {
        const bool bindingAvailable = available
            && bufferContains(binding.address);
        binding.control->setEnabled(bindingAvailable);
        const QSignalBlocker blocker(binding.control);
        binding.control->setCurrentRaw(bindingAvailable
            ? readValue(binding.address) : -1);
    }
    assignModel.refresh(backendConnected, backendHasPatchData);
    updateAssignList();
    updateAssignDetail();
    refreshing = false;
    emit summaryChanged();
}

bool ModernControlAssignEditor::summaryAvailable() const
{
    return available && assignModel.isAvailable();
}

QString ModernControlAssignEditor::directControlSummary(
    const QString &address) const
{
    for (const DirectControlBinding &binding : bindings)
        if (binding.address.compare(address, Qt::CaseInsensitive) == 0
            && binding.control)
            return binding.control->currentLabel();
    return QString();
}

QVector<bool> ModernControlAssignEditor::assignStateSummary() const
{
    QVector<bool> states;
    if (!assignModel.isAvailable())
        return states;
    states.reserve(assignModel.count());
    for (int index = 0; index < assignModel.count(); ++index) {
        const ModernAssignModel::Record &record = assignModel.record(index);
        states.append(record.valid && record.enabled);
    }
    return states;
}

int ModernControlAssignEditor::selectedAssignIndex() const
{
    return selectedAssign;
}

void ModernControlAssignEditor::selectAssignForNavigation(int index)
{
    selectAssign(index);
}

void ModernControlAssignEditor::focusDirectControl(const QString &address)
{
    for (const DirectControlBinding &binding : bindings) {
        if (binding.address.compare(address, Qt::CaseInsensitive) == 0
            && binding.control) {
            binding.control->focusForNavigation();
            return;
        }
    }
}
