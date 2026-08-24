#include "modernPatchListModel.h"
#include "globalVariables.h"

ModernPatchListModel::ModernPatchListModel(QObject *parent)
    : QObject(parent)
{
    items.reserve(bankTotalAll * patchPerBank);
    for (int bank = 1; bank <= bankTotalAll; ++bank) {
        const Category category = bank <= bankTotalUser ? User : Preset;
        for (int patch = 1; patch <= patchPerBank; ++patch)
            items.append({category, bank, patch, patchNumber(bank, patch), QString(), false});
    }
}

QString ModernPatchListModel::patchNumber(int bank, int patch) const
{
    if (bank < 1 || bank > bankTotalAll || patch < 1 || patch > patchPerBank)
        return QString::fromUtf8("—");
    const QChar prefix = bank <= bankTotalUser ? QChar('U') : QChar('P');
    const int shownBank = bank <= bankTotalUser ? bank : bank - bankTotalUser;
    return QString("%1%2-%3").arg(prefix).arg(shownBank, 2, 10, QChar('0')).arg(patch);
}

ModernPatchListModel::Patch *ModernPatchListModel::find(int bank, int patch)
{
    const int index = (bank - 1) * patchPerBank + patch - 1;
    return index >= 0 && index < items.size() ? &items[index] : nullptr;
}

void ModernPatchListModel::setPatchName(int bank, int patch, const QString &name)
{
    Patch *item = find(bank, patch);
    if (!item || name.trimmed().isEmpty()) return;
    item->name = name.trimmed();
    item->nameResolved = true;
    emit patchUpdated(bank, patch);
}

void ModernPatchListModel::setCurrentPatch(int bank, int patch)
{
    emit currentPatchChanged(bank, patch);
}
