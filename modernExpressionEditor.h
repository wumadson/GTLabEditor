#ifndef MODERNEXPRESSIONEDITOR_H
#define MODERNEXPRESSIONEDITOR_H

#include <QObject>
#include <QStringList>

#include "modernAssignModel.h"

class EffectEditorPanel;
class QLabel;
class QWidget;

class ModernExpressionEditor : public QObject
{
    Q_OBJECT

public:
    explicit ModernExpressionEditor(QObject *parent = nullptr);

    EffectEditorPanel *widget() const;
    void refresh(bool backendConnected, bool backendHasPatchData);
    bool summaryAvailable() const;
    QString exp1Summary() const;
    QString expSwitchSummary() const;
    QString exp2Summary() const;
    QList<int> expressionAssigns() const;

signals:
    void openControlAssignRequested(int assignIndex);
    void openPedalFxRequested();
    void summaryChanged();

private:
    struct ValueRow {
        QLabel *value = nullptr;
        QString area;
        QString bank;
        QString middle;
        QString address;
    };

    void buildEditor();
    QWidget *createSection(const QString &title,
                           const QList<QPair<QString, QString> > &fields,
                           const QString &area, const QString &bank);
    bool bufferContains(const QString &area, const QString &bank,
                        const QString &middle,
                        const QString &address) const;
    QString displayValue(const QString &area, const QString &bank,
                         const QString &middle,
                         const QString &address) const;
    void setUnavailable();
    void refreshAssigns(bool available);
    void refreshPedalSummary(bool available);

    EffectEditorPanel *editor = nullptr;
    QList<ValueRow> rows;
    ModernAssignModel assignModel;
    QWidget *assignList = nullptr;
    QLabel *assignEmpty = nullptr;
    QLabel *pedalState = nullptr;
    QLabel *pedalMode = nullptr;
    QLabel *pedalDetails = nullptr;
    bool available = false;
    QString currentExp1;
    QString currentExpSwitch;
    QString currentExp2;
    QList<int> currentAssigns;
};

#endif
