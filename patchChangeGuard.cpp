#include "patchChangeGuard.h"

void PatchChangeGuard::clear()
{
    baselineBank = 0;
    baselinePatch = 0;
    baseline.clear();
    baselineValid = false;
    pendingTarget = PatchNavigationTarget();
}

void PatchChangeGuard::setBaseline(int bank, int patch,
                                   const Snapshot &snapshot)
{
    baselineBank = bank;
    baselinePatch = patch;
    baseline = snapshot;
    baselineValid = bank > 0 && patch > 0 && !snapshot.isEmpty();
}

bool PatchChangeGuard::isCurrentPatch(int bank, int patch) const
{
    return baselineValid && baselineBank == bank && baselinePatch == patch;
}

bool PatchChangeGuard::isDirty(const Snapshot &current) const
{
    return baselineValid && current != baseline;
}

PatchNavigationAction PatchChangeGuard::requestNavigation(
    const PatchNavigationTarget &target, const Snapshot &current,
    PatchNavigationChoice choice)
{
    if (!target.isValid() || isCurrentPatch(target.bank, target.patch))
        return PatchNavigationAction::None;
    if (!isDirty(current) || choice == PatchNavigationChoice::Discard)
        return PatchNavigationAction::Load;
    if (choice == PatchNavigationChoice::Save) {
        pendingTarget = target;
        return PatchNavigationAction::SaveThenLoad;
    }
    return PatchNavigationAction::None;
}

PatchNavigationTarget PatchChangeGuard::completeSave(
    bool success, const Snapshot &savedSnapshot)
{
    const PatchNavigationTarget target = success
        ? pendingTarget : PatchNavigationTarget();
    if (success)
        setBaseline(baselineBank, baselinePatch, savedSnapshot);
    pendingTarget = PatchNavigationTarget();
    return target;
}

void PatchChangeGuard::cancelPendingSave()
{
    pendingTarget = PatchNavigationTarget();
}
