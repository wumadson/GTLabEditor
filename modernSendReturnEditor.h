#ifndef MODERNSENDRETURNEDITOR_H
#define MODERNSENDRETURNEDITOR_H

#include <QObject>

class EffectEditorPanel;
class ModernToggleSwitch;
class ParameterBar;
class QComboBox;

class ModernSendReturnEditor : public QObject
{
    Q_OBJECT

public:
    explicit ModernSendReturnEditor(QObject *parent = nullptr);

    EffectEditorPanel *widget() const;
    void refreshSendReturn(bool backendConnected,
                           bool backendHasPatchData);

signals:
    void stateChanged(bool available, bool on);

private:
    void buildEditor();
    bool bufferContains(const QString &address) const;
    bool hasValidBuffer(bool backendConnected,
                        bool backendHasPatchData) const;
    int readValue(const QString &address) const;
    void setSendReturnValue(const QString &address, int raw);
    QString displayValue(const QString &address, int raw) const;
    void updateControls(bool controlsAvailable);

    EffectEditorPanel *editor = nullptr;
    ModernToggleSwitch *stateToggle = nullptr;
    QComboBox *modeCombo = nullptr;
    ParameterBar *sendLevelBar = nullptr;
    ParameterBar *returnLevelBar = nullptr;
    bool available = false;
    bool refreshing = false;
};

#endif
