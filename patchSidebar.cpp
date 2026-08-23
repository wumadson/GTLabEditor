#include "patchSidebar.h"
#include "modernPatchListModel.h"

#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QVBoxLayout>

static QString patchKey(int bank, int patch)
{
    return QString("%1:%2").arg(bank).arg(patch);
}

PatchListItem::PatchListItem(int bank, int patch, const QString &number, QWidget *parent)
    : QFrame(parent), patchBank(bank), patchIndex(patch)
{
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(31);
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 0, 8, 0);
    layout->setSpacing(8);
    numberLabel = new QLabel(number);
    numberLabel->setObjectName("PatchItemNumber");
    numberLabel->setFixedWidth(43);
    nameLabel = new QLabel(QString::fromUtf8("—"));
    nameLabel->setObjectName("PatchItemName");
    nameLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    layout->addWidget(numberLabel);
    layout->addWidget(nameLabel, 1);
    setProperty("current", false);
    setProperty("pending", false);
}

void PatchListItem::setPatchName(const QString &name)
{
    nameLabel->setText(name.isEmpty() ? QString::fromUtf8("—") : name);
}

QString PatchListItem::patchName() const
{
    return nameLabel->text() == QString::fromUtf8("—") ? QString() : nameLabel->text();
}

void PatchListItem::setCurrent(bool value)
{
    current = value;
    if (current) pending = false;
    refreshStyle();
}

void PatchListItem::setPending(bool value)
{
    pending = value && !current;
    refreshStyle();
}

void PatchListItem::refreshStyle()
{
    setProperty("current", current);
    setProperty("pending", pending);
    style()->unpolish(this);
    style()->polish(this);
    for (QLabel *label : {numberLabel, nameLabel}) {
        label->style()->unpolish(label);
        label->style()->polish(label);
    }
    update();
}

void PatchListItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit activated(patchBank, patchIndex, patchName());
    QFrame::mouseReleaseEvent(event);
}

PatchBankSection::PatchBankSection(int bank, const QString &label, QWidget *parent)
    : QWidget(parent), bankNumber(bank)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(1);
    header = new QPushButton(QString::fromUtf8("▸  ") + label);
    header->setCheckable(true);
    header->setObjectName("PatchBankHeader");
    header->setProperty("expanded", false);
    header->setCursor(Qt::PointingHandCursor);
    QFont headerFont = header->font();
    headerFont.setCapitalization(QFont::AllUppercase);
    header->setFont(headerFont);
    connect(header, SIGNAL(clicked()), this, SLOT(toggleExpanded()));
    content = new QWidget;
    contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(5, 1, 0, 4);
    contentLayout->setSpacing(1);
    content->hide();
    layout->addWidget(header);
    layout->addWidget(content);
}

void PatchBankSection::addPatch(PatchListItem *item)
{
    patchItems.append(item);
    contentLayout->addWidget(item);
}

void PatchBankSection::toggleExpanded() { setExpanded(header->isChecked()); }

void PatchBankSection::setExpanded(bool expandedState)
{
    const bool wasExpanded = content->isVisible();
    header->setChecked(expandedState);
    header->setProperty("expanded", expandedState);
    header->setText((expandedState ? QString::fromUtf8("▾  ") : QString::fromUtf8("▸  "))
                    + header->text().mid(3));
    content->setVisible(expandedState);
    header->style()->unpolish(header);
    header->style()->polish(header);
    if (expandedState && !wasExpanded) emit expanded(bankNumber);
}

bool PatchBankSection::matchesSearch(const QString &text)
{
    if (text.isEmpty()) {
        for (PatchListItem *item : patchItems) item->show();
        return true;
    }
    bool any = false;
    for (PatchListItem *item : patchItems) {
        const bool match = !item->patchName().isEmpty()
            && item->patchName().contains(text, Qt::CaseInsensitive);
        item->setVisible(match);
        any |= match;
    }
    if (any) setExpanded(true);
    return any;
}

PatchSidebar::PatchSidebar(ModernPatchListModel *model, QWidget *parent)
    : QFrame(parent), patchModel(model)
{
    setObjectName("PatchSidebar");
    setMinimumWidth(220);
    setMaximumWidth(260);
    resize(245, height());
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(9, 10, 9, 9);
    layout->setSpacing(7);
    QLabel *title = new QLabel("PATCH LIBRARY");
    title->setObjectName("PatchLibraryTitle");
    QLineEdit *search = new QLineEdit;
    search->setObjectName("PatchSearch");
    search->setPlaceholderText("Search patches...");
    connect(search, SIGNAL(textChanged(QString)), this, SLOT(applyFilter(QString)));
    layout->addWidget(title);
    layout->addWidget(search);

    QScrollArea *scroll = new QScrollArea;
    scroll->setObjectName("PatchScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    QWidget *contents = new QWidget;
    QVBoxLayout *bankLayout = new QVBoxLayout(contents);
    bankLayout->setContentsMargins(0, 2, 0, 2);
    bankLayout->setSpacing(3);

    ModernPatchListModel::Category lastCategory = ModernPatchListModel::Temp;
    PatchBankSection *section = nullptr;
    int lastBank = -1;
    for (const ModernPatchListModel::Patch &patch : patchModel->patches()) {
        if (patch.category != ModernPatchListModel::User
            && patch.category != ModernPatchListModel::Preset) continue;
        if (patch.category != lastCategory) {
            QLabel *heading = new QLabel(patch.category == ModernPatchListModel::User ? "USER" : "PRESET");
            heading->setObjectName("PatchGroupTitle");
            bankLayout->addSpacing(5);
            bankLayout->addWidget(heading);
            lastCategory = patch.category;
        }
        if (patch.bank != lastBank) {
            section = new PatchBankSection(patch.bank,
                QString("Bank %1").arg(patch.number.left(3)));
            connect(section, SIGNAL(expanded(int)), this, SLOT(expandBank(int)));
            banks.append(section);
            bankSections.insert(patch.bank, section);
            bankLayout->addWidget(section);
            lastBank = patch.bank;
        }
        PatchListItem *item = new PatchListItem(patch.bank, patch.patch, patch.number);
        connect(item, SIGNAL(activated(int,int,QString)), this, SLOT(activatePatch(int,int,QString)));
        items.insert(patchKey(patch.bank, patch.patch), item);
        section->addPatch(item);
    }
    bankLayout->addStretch();
    scroll->setWidget(contents);
    layout->addWidget(scroll, 1);

    QHBoxLayout *actions = new QHBoxLayout;
    QPushButton *importButton = new QPushButton("IMPORT");
    QPushButton *exportButton = new QPushButton("EXPORT");
    importButton->setEnabled(false);
    exportButton->setEnabled(false);
    actions->addWidget(importButton);
    actions->addWidget(exportButton);
    layout->addLayout(actions);

    connect(patchModel, SIGNAL(patchUpdated(int,int)), this, SLOT(updatePatch(int,int)));
    connect(patchModel, SIGNAL(currentPatchChanged(int,int)), this, SLOT(setCurrentPatch(int,int)));
}

QSize PatchSidebar::sizeHint() const
{
    return QSize(245, QFrame::sizeHint().height());
}

void PatchSidebar::activatePatch(int bank, int patch, QString name)
{
    for (PatchListItem *item : items) item->setPending(false);
    PatchListItem *pendingItem = items.value(patchKey(bank, patch), nullptr);
    if (pendingItem) pendingItem->setPending(true);
    emit patchActivated(bank, patch, name);
}

void PatchSidebar::expandBank(int bank)
{
    const bool userBank = bank <= 50;
    for (auto it = bankSections.constBegin(); it != bankSections.constEnd(); ++it) {
        if (it.key() != bank && (it.key() <= 50) == userBank)
            it.value()->setExpanded(false);
    }
    emit bankExpanded(bank);
}

void PatchSidebar::updatePatch(int bank, int patch)
{
    PatchListItem *widget = items.value(patchKey(bank, patch), nullptr);
    if (!widget) return;
    const int index = (bank - 1) * 4 + patch - 1;
    if (index >= 0 && index < patchModel->patches().size())
        widget->setPatchName(patchModel->patches().at(index).name);
}

void PatchSidebar::setCurrentPatch(int bank, int patch)
{
    for (PatchListItem *item : items) {
        item->setPending(false);
        item->setCurrent(false);
    }
    PatchListItem *current = items.value(patchKey(bank, patch), nullptr);
    if (current) {
        current->setCurrent(true);
        PatchBankSection *section = bankSections.value(bank, nullptr);
        if (section) section->setExpanded(true);
    }
}

void PatchSidebar::applyFilter(const QString &text)
{
    for (PatchBankSection *bank : banks)
        bank->setVisible(bank->matchesSearch(text.trimmed()));
}
