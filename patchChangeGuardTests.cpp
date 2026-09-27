#include "patchChangeGuard.h"

#include <QCoreApplication>
#include <QDebug>

namespace {
int failures = 0;

void check(bool condition, const QString &message)
{
    if (condition)
        qInfo().noquote() << "PASS" << message;
    else {
        qCritical().noquote() << "FAIL" << message;
        ++failures;
    }
}

PatchChangeGuard::Snapshot snapshot(const QString &value)
{
    return {{QStringLiteral("00"), value},
            {QStringLiteral("01"), QStringLiteral("effect-data")},
            {QStringLiteral("0B"), QStringLiteral("signal-chain")}};
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    Q_UNUSED(app);

    PatchChangeGuard guard;
    const auto clean = snapshot(QStringLiteral("clean"));
    const auto edited = snapshot(QStringLiteral("edited"));
    const auto reordered = PatchChangeGuard::Snapshot{
        {QStringLiteral("00"), QStringLiteral("clean")},
        {QStringLiteral("01"), QStringLiteral("effect-data")},
        {QStringLiteral("0B"), QStringLiteral("reordered-chain")}};
    const PatchNavigationTarget other{8, 3, QStringLiteral("Other")};

    guard.setBaseline(8, 1, clean);
    check(guard.requestNavigation(other, clean, PatchNavigationChoice::Cancel)
              == PatchNavigationAction::Load,
          "A clean patch loads immediately");
    check(guard.isDirty(edited), "B parameter edit is dirty");

    check(guard.requestNavigation(other, edited, PatchNavigationChoice::Save)
              == PatchNavigationAction::SaveThenLoad,
          "C Save defers navigation");
    check(guard.completeSave(true, edited).isValid(),
          "C verified Save releases requested patch exactly once");
    check(!guard.completeSave(true, edited).isValid(),
          "C completed Save cannot release navigation twice");

    guard.setBaseline(8, 1, clean);
    check(guard.requestNavigation(other, edited,
                                  PatchNavigationChoice::Discard)
              == PatchNavigationAction::Load,
          "D Discard loads without saving");
    check(guard.requestNavigation(other, edited, PatchNavigationChoice::Cancel)
              == PatchNavigationAction::None,
          "E Cancel preserves the current patch");
    check(guard.requestNavigation(other, edited, PatchNavigationChoice::Cancel)
              == PatchNavigationAction::None,
          "F dialog close or Escape maps to Cancel");
    check(guard.isDirty(reordered), "G Signal Chain reorder is dirty");
    check(!guard.isDirty(clean),
          "H visual node selection without data mutation stays clean");

    guard.setBaseline(9, 2, edited);
    check(!guard.isDirty(edited), "I normal patch load establishes clean baseline");
    guard.setBaseline(8, 1, clean);
    guard.requestNavigation(other, edited, PatchNavigationChoice::Save);
    check(!guard.completeSave(false, edited).isValid() && guard.isDirty(edited),
          "J failed Save neither loads nor clears dirty state");
    guard.requestNavigation(other, edited, PatchNavigationChoice::Save);
    guard.completeSave(true, edited);
    check(!guard.isDirty(edited), "J verified Save clears dirty state");
    check(guard.isDirty(clean), "K editing again after Save becomes dirty");
    check(guard.requestNavigation({8, 1, QString()}, clean,
                                  PatchNavigationChoice::Discard)
              == PatchNavigationAction::None,
          "L selecting the current patch is a no-op");

    qInfo() << "failures=" << failures;
    return failures == 0 ? 0 : 1;
}
