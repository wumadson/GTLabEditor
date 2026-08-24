#ifndef ASSIGNTARGETVALUEEDITOR_H
#define ASSIGNTARGETVALUEEDITOR_H

#include <QWidget>

#include <functional>

class QComboBox;
class QLabel;
class QPushButton;
class QStackedWidget;
class TargetValueSpinBox;

class AssignTargetValueEditor : public QWidget
{
public:
    explicit AssignTargetValueEditor(const QString &label,
                                     QWidget *parent = nullptr);

    void setTargetValue(int targetId, int raw, bool available);

    std::function<void(int, int)> valueEdited;

private:
    void configureTarget(int targetId);
    QString originalDisplayForRaw(int raw) const;
    QString displayForRaw(int raw) const;

    QLabel *title = nullptr;
    QStackedWidget *stack = nullptr;
    QComboBox *selector = nullptr;
    TargetValueSpinBox *spinBox = nullptr;
    QWidget *hybridPage = nullptr;
    QStackedWidget *hybridValueStack = nullptr;
    TargetValueSpinBox *hybridSpinBox = nullptr;
    QComboBox *rhythmSelector = nullptr;
    QPushButton *timeButton = nullptr;
    QPushButton *rhythmButton = nullptr;
    QString targetBank;
    QString targetAddress;
    int configuredTargetId = -1;
    int minimum = 0;
    int maximum = 0;
    bool selectorMode = true;
    bool hybridMode = false;
    bool hybridTimeDirty = false;
    bool updating = false;
};

#endif
