#ifndef MODERNPEDALBOARDEDITOR_H
#define MODERNPEDALBOARDEDITOR_H

#include "modernPedalboardModel.h"

#include <QObject>
#include <QVector>

class QWidget;
class PedalboardModuleWidget;
class PedalboardFunctionSelector;

class ModernPedalboardEditor : public QObject
{
    Q_OBJECT

public:
    explicit ModernPedalboardEditor(QObject *parent = nullptr);
    ~ModernPedalboardEditor() override;

    QWidget *widget() const;
    void refresh(bool backendConnected, bool backendHasPatchData);
    bool summaryAvailable() const;
    int activePatchFoot() const;
    QVector<ModernPedalboardModel::LogicalState> summaryStates() const;

signals:
    void summaryChanged();
    void openControlAssignRequested(QString address);

private:
    void buildEditor();
    void openFunctionSelector(int index);
    void functionSelected(int index, int raw);
    void navigateDirectControl(int index);
    void updateModules();

    QWidget *editor = nullptr;
    QVector<PedalboardModuleWidget *> moduleWidgets;
    PedalboardFunctionSelector *functionSelector = nullptr;
    ModernPedalboardModel model;
    bool available = false;
    int currentPatchFoot = 0;
    bool backendConnected = false;
    bool backendHasPatchData = false;
};

#endif
