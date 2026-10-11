#ifndef PRODUCTEDITION_H
#define PRODUCTEDITION_H

#include <QString>
class QWidget;

namespace ProductEdition {
enum class Feature { ImportPreset, ExportPreset, Assign, BackupRestore, QuickSettings };
constexpr bool isFree()
{
#ifdef GTLAB_FREE_EDITION
    return true;
#else
    return false;
#endif
}
constexpr bool isFull() { return !isFree(); }
constexpr bool allows(Feature) { return isFull(); }
QString applicationName();
QString lockedLabel(const QString &label);
bool require(Feature feature, QWidget *parent);
bool isAssignParameter(const QString &area, const QString &bank, const QString &offset);
bool requireParameterEdit(const QString &area, const QString &bank,
                          const QString &offset, QWidget *parent);
}
#endif
