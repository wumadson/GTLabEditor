#ifndef PATCHCHANGEGUARD_H
#define PATCHCHANGEGUARD_H

#include <QMap>
#include <QString>

enum class PatchNavigationChoice { Cancel, Save, Discard };
enum class PatchNavigationAction { None, Load, SaveThenLoad };

struct PatchNavigationTarget
{
    int bank = 0;
    int patch = 0;
    QString name;

    bool isValid() const { return bank > 0 && patch > 0; }
};

class PatchChangeGuard
{
public:
    using Snapshot = QMap<QString, QString>;

    void clear();
    void setBaseline(int bank, int patch, const Snapshot &snapshot);
    bool hasBaseline() const { return baselineValid; }
    bool isCurrentPatch(int bank, int patch) const;
    bool isDirty(const Snapshot &current) const;

    PatchNavigationAction requestNavigation(
        const PatchNavigationTarget &target, const Snapshot &current,
        PatchNavigationChoice choice);
    PatchNavigationTarget completeSave(bool success,
                                       const Snapshot &savedSnapshot);
    void cancelPendingSave();

private:
    int baselineBank = 0;
    int baselinePatch = 0;
    Snapshot baseline;
    bool baselineValid = false;
    PatchNavigationTarget pendingTarget;
};

#endif // PATCHCHANGEGUARD_H
