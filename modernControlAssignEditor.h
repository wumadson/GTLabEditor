#ifndef MODERNCONTROLASSIGNEDITOR_H
#define MODERNCONTROLASSIGNEDITOR_H

#include <QObject>
#include <QHash>
#include <QVector>

#include "modernAssignModel.h"

class EffectEditorPanel;
class AssignSourceSelector;
class AssignTargetBrowser;
class AssignTargetValueEditor;
class AssignModeSelector;
class AssignConditionalSelector;
class AssignRangeSpinBox;
class DirectControlFunctionSelector;
class QButtonGroup;
class QGridLayout;
class QLabel;
class ModernToggleSwitch;
class QPushButton;
class QWidget;

class ModernControlAssignEditor : public QObject
{
    Q_OBJECT

public:
    explicit ModernControlAssignEditor(QObject *parent = nullptr);

    EffectEditorPanel *widget() const;
    void refresh(bool backendConnected, bool backendHasPatchData);
    bool summaryAvailable() const;
    QString directControlSummary(const QString &address) const;
    QVector<bool> assignStateSummary() const;
    int selectedAssignIndex() const;
    void selectAssignForNavigation(int index);
    void focusDirectControl(const QString &address);

signals:
    void summaryChanged();

private:
    struct DirectControlBinding {
        QString address;
        DirectControlFunctionSelector *control = nullptr;
    };

    void buildEditor();
    QWidget *createDirectControl(const QString &label,
                                 const QString &address);
    bool bufferContains(const QString &address) const;
    bool hasValidBuffer(bool backendConnected,
                        bool backendHasPatchData) const;
    int readValue(const QString &address) const;
    void writeValue(const QString &address, int raw);
    QWidget *createAssignList();
    QWidget *createAssignDetail();
    void selectAssign(int index);
    void assignStateChanged(bool enabled);
    void assignSourceChanged(int raw);
    void assignTargetChanged(int targetId, int minimum, int maximum,
                             int expectedCurrentTarget, int assignIndex,
                             int loadedBank, int loadedPatch);
    void assignTargetMinimumChanged(int targetId, int raw);
    void assignTargetMaximumChanged(int targetId, int raw);
    void writeAssignTargetValue(int offset, int targetId, int raw);
    void assignModeChanged(int raw);
    void assignRangeLowChanged();
    void assignRangeHighChanged();
    void assignConditionalChanged(int offset, int raw,
                                  ModernAssignModel::SourceKind sourceKind);
    void assignInternalTimeChanged();
    void assignInputSensitivityChanged();
    int readAssignOffset(int offset) const;
    void writeAssignOffset(int offset, int raw);
    void updateAssignList();
    void updateAssignDetail();
    void setDetailRow(const QString &key, const QString &label,
                      const QString &value, bool visible = true);

    EffectEditorPanel *editor = nullptr;
    QVector<DirectControlBinding> bindings;
    ModernAssignModel assignModel;
    QVector<QPushButton *> assignButtons;
    QButtonGroup *assignButtonGroup = nullptr;
    QHash<QString, QWidget *> detailRows;
    QHash<QString, QLabel *> detailValues;
    QGridLayout *detailGrid = nullptr;
    QLabel *detailTitle = nullptr;
    ModernToggleSwitch *detailStateToggle = nullptr;
    AssignSourceSelector *detailSourceControl = nullptr;
    AssignTargetBrowser *detailTargetControl = nullptr;
    AssignTargetValueEditor *detailTargetMinimumControl = nullptr;
    AssignTargetValueEditor *detailTargetMaximumControl = nullptr;
    AssignModeSelector *detailModeControl = nullptr;
    AssignRangeSpinBox *detailRangeLowControl = nullptr;
    AssignRangeSpinBox *detailRangeHighControl = nullptr;
    AssignConditionalSelector *detailTriggerControl = nullptr;
    AssignRangeSpinBox *detailInternalTimeControl = nullptr;
    AssignConditionalSelector *detailCurveControl = nullptr;
    AssignConditionalSelector *detailWaveRateControl = nullptr;
    AssignConditionalSelector *detailWaveFormControl = nullptr;
    AssignRangeSpinBox *detailInputSensitivityControl = nullptr;
    int selectedAssign = 0;
    bool refreshing = false;
    bool available = false;
};

#endif
