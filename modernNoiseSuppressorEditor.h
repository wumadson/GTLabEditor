#ifndef MODERNNOISESUPPRESSOREDITOR_H
#define MODERNNOISESUPPRESSOREDITOR_H

#include <QObject>
#include <QString>

class EffectEditorPanel;
class ModernToggleSwitch;
class ParameterBar;
class QComboBox;

enum class NoiseSuppressorSlot {
    NS1,
    NS2
};

class ModernNoiseSuppressorEditor : public QObject
{
    Q_OBJECT

public:
    explicit ModernNoiseSuppressorEditor(
        NoiseSuppressorSlot slot, QObject *parent = nullptr);

    EffectEditorPanel *widget() const;
    NoiseSuppressorSlot slot() const;
    void refreshNoiseSuppressor(bool backendConnected,
                                bool backendHasPatchData);

signals:
    void stateChanged(bool available, bool on);

private:
    void buildEditor();
    QString addressForOffset(int offset) const;
    bool bufferContains(int offset) const;
    bool hasValidBuffer(bool backendConnected,
                        bool backendHasPatchData) const;
    int readValue(int offset) const;
    void setNoiseSuppressorValue(int offset, int raw);
    QString displayValue(int offset, int raw) const;
    void updateControls(bool controlsAvailable);

    NoiseSuppressorSlot nsSlot;
    EffectEditorPanel *editor = nullptr;
    ModernToggleSwitch *stateToggle = nullptr;
    ParameterBar *thresholdBar = nullptr;
    ParameterBar *releaseBar = nullptr;
    QComboBox *detectCombo = nullptr;
    bool available = false;
    bool refreshing = false;
};

#endif
