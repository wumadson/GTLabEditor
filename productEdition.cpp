#include "productEdition.h"
#include <QCoreApplication>
#include <QMessageBox>

namespace ProductEdition {
QString applicationName()
{
    return isFree() ? QStringLiteral("GT Lab Editor Free")
                    : QStringLiteral("GT Lab Editor");
}

QString lockedLabel(const QString &label)
{
    return isFree() ? QString::fromUtf8("\xF0\x9F\x94\x92 ") + label : label;
}

bool require(Feature feature, QWidget *parent)
{
    if (allows(feature))
        return true;
    QMessageBox dialog(QMessageBox::Information,
        QCoreApplication::translate("ProductEdition", "Premium feature"),
        QCoreApplication::translate("ProductEdition",
            "This feature is available in the full GT Lab Editor."),
        QMessageBox::Ok, parent);
    dialog.setObjectName("PremiumFeatureDialog");
    dialog.setInformativeText(QCoreApplication::translate("ProductEdition",
        "The full version includes preset import/export, Assign configuration, "
        "Backup/Restore and Quick Settings."));
    dialog.exec();
    return false;
}

// Legacy controls remain shared by all effects. Only Assign fields are gated;
// the MIDI engine and ordinary parameter writes stay edition-independent.
bool requireParameterEdit(const QString &area, const QString &bank,
                          const QString &offset, QWidget *parent)
{
    return isFull() || !isAssignParameter(area, bank, offset)
        || require(Feature::Assign, parent);
}

bool isAssignParameter(const QString &area, const QString &bank, const QString &offset)
{
    if (area == QLatin1String("System"))
        return false;
    bool ok = false;
    const int address = offset.toInt(&ok, 16);
    return ok && ((bank == QLatin1String("0B") && address >= 0x20 && address <= 0x7F)
        || (bank == QLatin1String("0C") && (address <= 0x20 || address == 0x23))
        || (bank == QLatin1String("0A") && address >= 0x46 && address <= 0x48));
}
}
