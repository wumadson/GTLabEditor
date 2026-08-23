#include "effectModelBrowser.h"

#include "modernTheme.h"

#include <QAbstractItemView>
#include <QColor>
#include <QEvent>
#include <QFont>
#include <QKeyEvent>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QVBoxLayout>

namespace {
enum ItemRole {
    ModelIndexRole = Qt::UserRole + 1,
    CategoryRole,
    FirstCategoryRole,
    AccentColorRole,
    CollapsibleRole,
    ExpandedRole
};

class EffectModelDelegate : public QStyledItemDelegate
{
public:
    explicit EffectModelDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override
    {
        Q_UNUSED(option)
        return QSize(0, index.data(CategoryRole).toBool() ? 30 : 28);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const bool category = index.data(CategoryRole).toBool();
        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;
        const bool enabled = option.state & QStyle::State_Enabled;
        QRect row = option.rect.adjusted(2, 1, -2, -1);

        if (category) {
            const bool collapsible = index.data(CollapsibleRole).toBool();
            const bool expanded = index.data(ExpandedRole).toBool();
            const int contentTop = row.top() + 2;
            const int contentBottom = row.bottom() - 3;
            QFont categoryFont = option.font;
            categoryFont.setPixelSize(9);
            categoryFont.setWeight(QFont::Bold);
            categoryFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.7);
            painter->setFont(categoryFont);
            painter->setPen(QColor(ModernTheme::color(
                enabled ? ModernTheme::PrimaryText
                        : ModernTheme::DisabledText)));
            painter->drawText(QRect(row.left() + (collapsible ? 22 : 15),
                                    contentTop,
                                    row.width() - (collapsible ? 27 : 20),
                                    contentBottom - contentTop),
                              Qt::AlignLeft | Qt::AlignVCenter,
                              index.data(Qt::DisplayRole).toString());

            QColor accent = index.data(AccentColorRole).value<QColor>();
            if (!accent.isValid())
                accent = QColor(ModernTheme::color(
                    ModernTheme::EditorAccent));
            accent.setAlpha(enabled ? 175 : 70);
            if (collapsible) {
                painter->setPen(QPen(accent, 1.4, Qt::SolidLine,
                                     Qt::RoundCap, Qt::RoundJoin));
                painter->setBrush(Qt::NoBrush);
                QPainterPath chevron;
                if (expanded) {
                    chevron.moveTo(row.left() + 7, contentTop + 8);
                    chevron.lineTo(row.left() + 11, contentTop + 12);
                    chevron.lineTo(row.left() + 15, contentTop + 8);
                } else {
                    chevron.moveTo(row.left() + 9, contentTop + 6);
                    chevron.lineTo(row.left() + 13, contentTop + 10);
                    chevron.lineTo(row.left() + 9, contentTop + 14);
                }
                painter->drawPath(chevron);
            } else {
                painter->setPen(Qt::NoPen);
                painter->setBrush(accent);
                painter->drawRoundedRect(
                    QRectF(row.left() + 5, contentTop + 5, 2,
                           qMax(7, contentBottom - contentTop - 10)),
                    1, 1);
            }

            QColor divider(ModernTheme::color(ModernTheme::BorderSubtle));
            divider.setAlpha(190);
            painter->setPen(QPen(divider, 1));
            painter->drawLine(row.left() + 15, contentBottom,
                              row.right() - 5, contentBottom);
            painter->restore();
            return;
        }

        if (selected) {
            QColor selection = index.data(AccentColorRole).value<QColor>();
            if (!selection.isValid())
                selection = QColor(ModernTheme::color(
                    ModernTheme::EditorAccent));
            selection.setAlpha(72);
            painter->setPen(Qt::NoPen);
            painter->setBrush(selection);
            painter->drawRoundedRect(row, 4, 4);
            QColor selectionEdge = index.data(
                AccentColorRole).value<QColor>();
            if (!selectionEdge.isValid())
                selectionEdge = QColor(ModernTheme::color(
                    ModernTheme::EditorAccentHover));
            else
                selectionEdge = selectionEdge.lighter(116);
            painter->setBrush(selectionEdge);
            painter->drawRoundedRect(
                QRectF(row.left() + 4, row.top() + 4,
                       3, row.height() - 8),
                1.5, 1.5);
        } else if (hovered && enabled) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(ModernTheme::color(
                ModernTheme::ElevatedPanel)));
            painter->drawRoundedRect(row, 4, 4);
        }

        QFont itemFont = option.font;
        itemFont.setPixelSize(10);
        itemFont.setWeight(selected ? QFont::DemiBold : QFont::Normal);
        painter->setFont(itemFont);
        const ModernTheme::ColorRole textRole = !enabled
            ? ModernTheme::DisabledText
            : selected ? ModernTheme::PrimaryText
                       : ModernTheme::SecondaryText;
        painter->setPen(QColor(ModernTheme::color(textRole)));
        painter->drawText(row.adjusted(18, 0, -6, 0),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          index.data(Qt::DisplayRole).toString().toUpper());
        painter->restore();
    }
};

void splitModelLabel(const QString &source, QString *category, QString *name)
{
    const QString trimmed = source.trimmed();
    static const QRegularExpression categoryPattern(
        QStringLiteral("^\\(([^)]+)\\)\\s*(.+)$"));
    const QRegularExpressionMatch match = categoryPattern.match(trimmed);

    if (match.hasMatch()) {
        *category = match.captured(1).trimmed().toUpper();
        *name = match.captured(2).trimmed();
    } else {
        category->clear();
        *name = trimmed;
    }

}
}

EffectModelBrowser::EffectModelBrowser(QWidget *parent)
    : QWidget(parent), modelList(new QListWidget(this)),
      browserAccent(ModernTheme::color(ModernTheme::EditorAccent)),
      categoriesCollapsible(false)
{
    setObjectName("EffectModelBrowser");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    modelList->setObjectName("EffectModelList");
    modelList->setFrameShape(QFrame::NoFrame);
    modelList->setSelectionMode(QAbstractItemView::SingleSelection);
    modelList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    modelList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    modelList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    modelList->setMouseTracking(true);
    modelList->installEventFilter(this);
    modelList->setSpacing(0);
    modelList->setItemDelegate(new EffectModelDelegate(modelList));
    modelList->verticalScrollBar()->setObjectName("EffectModelScroll");
    layout->addWidget(modelList);

    connect(modelList, &QListWidget::itemClicked,
            this, &EffectModelBrowser::itemClicked);
}

void EffectModelBrowser::setModels(const QStringList &labels)
{
    modelList->clear();
    modelItems.clear();
    modelCategoryItems.clear();

    QString previousCategory;
    bool hasPreviousCategory = false;
    QListWidgetItem *currentCategoryItem = nullptr;
    bool firstCategory = true;
    for (int index = 0; index < labels.size(); ++index) {
        QString category;
        QString name;
        splitModelLabel(labels.at(index), &category, &name);

        if (!category.isEmpty()
            && (!hasPreviousCategory || category != previousCategory)) {
            QListWidgetItem *heading = new QListWidgetItem(category, modelList);
            heading->setData(CategoryRole, true);
            heading->setData(FirstCategoryRole, firstCategory);
            heading->setData(AccentColorRole, browserAccent);
            heading->setData(CollapsibleRole, categoriesCollapsible);
            heading->setData(ExpandedRole, !categoriesCollapsible);
            heading->setFlags(Qt::ItemIsEnabled);
            currentCategoryItem = heading;
            previousCategory = category;
            hasPreviousCategory = true;
            firstCategory = false;
        } else if (category.isEmpty()) {
            previousCategory.clear();
            hasPreviousCategory = false;
            currentCategoryItem = nullptr;
        }

        QListWidgetItem *item = new QListWidgetItem(name, modelList);
        item->setData(ModelIndexRole, index);
        item->setData(CategoryRole, false);
        item->setData(AccentColorRole, browserAccent);
        modelItems.append(item);
        modelCategoryItems.append(currentCategoryItem);
        if (currentCategoryItem && categoriesCollapsible)
            item->setHidden(true);
    }
}

void EffectModelBrowser::setCurrentIndex(int index)
{
    if (index < 0 || index >= modelItems.size()) {
        modelList->setCurrentItem(nullptr);
        modelList->clearSelection();
        return;
    }

    QListWidgetItem *item = modelItems.at(index);
    QListWidgetItem *category = modelCategoryItems.at(index);
    if (category && categoriesCollapsible)
        setCategoryExpanded(category, true);
    modelList->setCurrentItem(item);
    if (category)
        modelList->scrollToItem(category, QAbstractItemView::PositionAtTop);
    modelList->scrollToItem(item, QAbstractItemView::EnsureVisible);
}

void EffectModelBrowser::setAccentColor(const QColor &color)
{
    if (!color.isValid() || browserAccent == color)
        return;
    browserAccent = color;
    for (int row = 0; row < modelList->count(); ++row)
        modelList->item(row)->setData(AccentColorRole, browserAccent);
    modelList->viewport()->update();
}

void EffectModelBrowser::setCategoriesCollapsible(bool enabled)
{
    if (categoriesCollapsible == enabled)
        return;
    categoriesCollapsible = enabled;
    for (QListWidgetItem *category : modelCategoryItems) {
        if (!category)
            continue;
        category->setData(CollapsibleRole, enabled);
        setCategoryExpanded(category, !enabled);
    }
    modelList->viewport()->update();
}

QColor EffectModelBrowser::accentColor() const
{
    return browserAccent;
}

int EffectModelBrowser::modelCount() const
{
    return modelItems.size();
}

bool EffectModelBrowser::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != modelList || event->type() != QEvent::KeyPress)
        return QWidget::eventFilter(watched, event);

    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
    const int key = keyEvent->key();
    if (key == Qt::Key_Return || key == Qt::Key_Enter
        || key == Qt::Key_Space) {
        QListWidgetItem *current = modelList->currentItem();
        if (current)
            itemClicked(current);
        return current != nullptr;
    }

    if (key != Qt::Key_Up && key != Qt::Key_Down)
        return QWidget::eventFilter(watched, event);

    const int direction = key == Qt::Key_Down ? 1 : -1;
    int row = modelList->currentRow();
    if (row < 0)
        row = direction > 0 ? -1 : modelList->count();
    for (row += direction;
         row >= 0 && row < modelList->count(); row += direction) {
        QListWidgetItem *candidate = modelList->item(row);
        if (!candidate->isHidden()
            && candidate->flags().testFlag(Qt::ItemIsSelectable)) {
            modelList->setCurrentItem(candidate);
            modelList->scrollToItem(
                candidate, QAbstractItemView::EnsureVisible);
            return true;
        }
    }
    return true;
}

void EffectModelBrowser::itemClicked(QListWidgetItem *item)
{
    if (!item)
        return;

    if (item->data(CategoryRole).toBool()) {
        if (categoriesCollapsible) {
            setCategoryExpanded(
                item, !item->data(ExpandedRole).toBool());
        }
        return;
    }

    bool validIndex = false;
    const int index = item->data(ModelIndexRole).toInt(&validIndex);
    if (validIndex)
        emit modelSelected(index);
}

void EffectModelBrowser::setCategoryExpanded(QListWidgetItem *category,
                                               bool expanded)
{
    if (!category)
        return;
    category->setData(ExpandedRole, expanded);
    for (int index = 0; index < modelItems.size(); ++index) {
        if (modelCategoryItems.value(index) == category)
            modelItems.at(index)->setHidden(!expanded);
    }
    modelList->viewport()->update();
}
