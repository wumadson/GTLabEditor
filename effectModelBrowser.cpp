#include "effectModelBrowser.h"

#include "modernTheme.h"

#include <QAbstractItemView>
#include <QColor>
#include <QFont>
#include <QListWidget>
#include <QPainter>
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
    AccentColorRole
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
        if (!index.data(CategoryRole).toBool())
            return QSize(0, 29);
        return QSize(0, index.data(FirstCategoryRole).toBool() ? 29 : 35);
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
            const bool firstCategory = index.data(FirstCategoryRole).toBool();
            const int contentTop = row.top() + (firstCategory ? 2 : 8);
            const int contentBottom = row.bottom() - 3;
            QFont categoryFont = option.font;
            categoryFont.setPointSizeF(qMax(8.0, categoryFont.pointSizeF() - 2.0));
            categoryFont.setWeight(QFont::Bold);
            categoryFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
            painter->setFont(categoryFont);
            painter->setPen(QColor(ModernTheme::color(
                enabled ? ModernTheme::PrimaryText
                        : ModernTheme::DisabledText)));
            painter->drawText(QRect(row.left() + 15, contentTop,
                                    row.width() - 20,
                                    contentBottom - contentTop),
                              Qt::AlignLeft | Qt::AlignVCenter,
                              index.data(Qt::DisplayRole).toString());

            QColor accent = index.data(AccentColorRole).value<QColor>();
            if (!accent.isValid())
                accent = QColor(ModernTheme::color(
                    ModernTheme::EditorAccent));
            accent.setAlpha(enabled ? 175 : 70);
            painter->setPen(Qt::NoPen);
            painter->setBrush(accent);
            painter->drawRoundedRect(
                QRectF(row.left() + 5, contentTop + 5, 2,
                       qMax(7, contentBottom - contentTop - 10)),
                1, 1);

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
        itemFont.setWeight(selected ? QFont::DemiBold : QFont::Normal);
        painter->setFont(itemFont);
        const ModernTheme::ColorRole textRole = !enabled
            ? ModernTheme::DisabledText
            : selected ? ModernTheme::PrimaryText
                       : ModernTheme::SecondaryText;
        painter->setPen(QColor(ModernTheme::color(textRole)));
        painter->drawText(row.adjusted(18, 0, -6, 0),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          index.data(Qt::DisplayRole).toString());
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
      browserAccent(ModernTheme::color(ModernTheme::EditorAccent))
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
    modelList->setCurrentItem(item);
    QListWidgetItem *category = modelCategoryItems.at(index);
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

QColor EffectModelBrowser::accentColor() const
{
    return browserAccent;
}

int EffectModelBrowser::modelCount() const
{
    return modelItems.size();
}

void EffectModelBrowser::itemClicked(QListWidgetItem *item)
{
    if (!item)
        return;

    bool validIndex = false;
    const int index = item->data(ModelIndexRole).toInt(&validIndex);
    if (validIndex)
        emit modelSelected(index);
}
