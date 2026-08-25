#ifndef MODERNPEDALBOARDMODEL_H
#define MODERNPEDALBOARDMODEL_H

#include <QList>
#include <QString>
#include <QVector>

class ModernPedalboardModel
{
public:
    enum class ControlId {
        Foot1,
        Foot2,
        Foot3,
        Foot4,
        BankUp,
        BankDown,
        Ctl1,
        Ctl2,
        ExpSwitch,
        Exp1,
        Exp2,
        PedalFxFootVolume
    };

    enum class DataScope {
        System,
        Patch
    };

    enum class LogicalState {
        Off,
        On,
        Momentary,
        Unknown
    };

    enum class NavigationTarget {
        Pedalboard,
        ControlAssign,
        Expression,
        PedalFxFootVolume
    };

    struct ControlState {
        ControlId id = ControlId::Foot1;
        QString label;
        int functionRaw = -1;
        QString functionName;
        DataScope scope = DataScope::Patch;
        QString functionAddress;
        LogicalState state = LogicalState::Unknown;
        QString stateAddress;
        NavigationTarget editor = NavigationTarget::Pedalboard;
        QList<int> relatedAssigns;
        bool dataValid = false;
    };

    void refresh(bool backendConnected, bool backendHasPatchData);
    int count() const;
    const ControlState &control(int index) const;
    const ControlState *control(ControlId id) const;

private:
    struct StateSource {
        QString bank;
        QString middle;
        QString address;
    };

    bool bufferContains(DataScope scope, const QString &bank,
                        const QString &middle, const QString &address) const;
    ControlState readControl(ControlId id, const QString &label,
                             DataScope scope, const QString &bank,
                             const QString &middle, const QString &address,
                             NavigationTarget editor,
                             bool sourceAvailable) const;
    StateSource stateSourceFor(int functionRaw) const;
    LogicalState classifyFunction(int functionRaw, DataScope scope,
                                  QString *stateAddress) const;
    QList<int> relatedAssignsFor(ControlId id,
                                 bool backendConnected,
                                 bool backendHasPatchData) const;
    QString snapshot() const;
    void logIfChanged();

    QVector<ControlState> controls;
    QString lastLoggedSnapshot;
};

#endif
