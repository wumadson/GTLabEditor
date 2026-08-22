#ifndef MODERNFLOORBOARD_H
#define MODERNFLOORBOARD_H

#include <QWidget>
#include "modernSignalChainModel.h"
#include "modernPatchListModel.h"

class QLabel;
class QPushButton;
class QFrame;
class QComboBox;
class QDial;
class QScrollArea;
class QStackedWidget;
class SignalChainPanel;
class SignalChainModule;
class SignalJunction;
class SignalConnector;
class QGridLayout;
class QHBoxLayout;
class QResizeEvent;
class QEvent;
class QObject;
class EffectModule;
class EffectEditorPanel;
class EffectArtworkWidget;
class EffectModelBrowser;
class ParameterBar;
class AudioGearSwitch;
class ModernToggleSwitch;
class ModernEqGraph;
enum class FxSlot;
enum class NoiseSuppressorSlot;
class ModernFxEditor;
class ModernPedalFxEditor;
class ModernNoiseSuppressorEditor;
class ModernSendReturnEditor;
class PatchSidebar;

class modernFloorBoard : public QWidget
{
    Q_OBJECT

public:
    explicit modernFloorBoard(QWidget *parent = nullptr);

public slots:
    void backendConnected();
    void backendDisconnected();
    void refreshReverbState();
    void patchNameResolved(int bank, int patch, QString name);

signals:
    void requestPatchNames(int bank);
    void selectPatchRequested(int bank, int patch, QString name);
    void connectionStateChanged(bool connected);

private slots:
    void toggleReverb();
    void reverbComboChanged(int value);
    void reverbModelSelected(int index);
    void reverbBarChanged(int value);
    void showCompEditor();
    void showReverbEditor();
    void showOddsEditor();
    void showDelayEditor();
    void showChorusEditor();
    void showEqEditor();
    void showPreampAEditor();
    void showPreampBEditor();
    void showFx1Editor();
    void showFx2Editor();
    void showPedalFxEditor();
    void showFootVolumeEditor();
    void showNs1Editor();
    void showNs2Editor();
    void showSendReturnEditor();
    void showChannelRoutingEditor();
    void compTypeChanged(int value);
    void compModelSelected(int index);
    void compBarChanged(int value);
    void toggleComp();
    void oddsComboChanged(int value);
    void oddsModelSelected(int index);
    void oddsBarChanged(int value);
    void oddsToggleChanged();
    void delayComboChanged(int value);
    void delayModelSelected(int index);
    void delayBarChanged(int value);
    void delayToggleChanged();
    void chorusComboChanged(int index);
    void chorusModeSelected(int index);
    void chorusBarChanged(int value);
    void toggleChorus();
    void eqComboChanged(int value);
    void eqBarChanged(int value);
    void toggleEq();
    void preampComboChanged(int value);
    void preampModelSelected(int index);
    void preampBarChanged(int value);
    void preampToggleChanged();
    void channelModeChanged(int index);
    void channelRoutingBarChanged(int value);
    void outputSelectChanged(int index);

private:
    EffectModule *createEffectBlock(const QString &name, bool available);
    bool hasValidReverbBuffer() const;
    void setReverbUnavailable();
    QWidget *createReverbCombo(const QString &label, const QString &address);
    QWidget *createReverbBar(const QString &label,
                             const QString &address,
                             bool twoByte = false);
    void setReverbValue(const QString &address, int value, bool twoByte);
    void setReverbType(int index);
    void updateReverbParameterControls(bool available);
    bool hasValidCompBuffer() const;
    void setCompUnavailable();
    QWidget *createCompCombo(const QString &label, const QString &address);
    QWidget *createCompBar(const QString &label, const QString &address);
    void setCompValue(const QString &address, int value);
    void setCompType(int index);
    void updateCompParameterControls(bool available);
    void refreshCompState();
    bool hasValidOddsBuffer() const;
    void setOddsUnavailable();
    QWidget *createOddsCombo(const QString &label, const QString &address);
    QWidget *createOddsBar(const QString &label, const QString &address);
    void setOddsValue(const QString &address, int value);
    void setOddsType(int index);
    void updateOddsParameterControls(bool available);
    void refreshOddsState();
    bool hasValidDelayBuffer() const;
    void setDelayUnavailable();
    QWidget *createDelayCombo(const QString &label, const QString &address);
    QWidget *createDelayBar(const QString &label, const QString &address,
                            bool twoByte = false);
    void setDelayValue(const QString &address, int value, bool twoByte = false);
    void setDelayType(int index);
    void updateDelayParameterControls(bool available);
    void updateDelayPageForType(int type);
    void refreshDelayState();
    bool hasValidChorusBuffer() const;
    bool hasValidChorusParameter(const QString &address) const;
    void setChorusUnavailable();
    QWidget *createChorusCombo(const QString &label,
                               const QString &address);
    QWidget *createChorusBar(const QString &label,
                             const QString &address);
    void setChorusValue(const QString &address, int value);
    void setChorusMode(int index);
    void updateChorusParameterControls(bool available);
    void refreshChorus();
    bool hasValidEqBuffer() const;
    void setEqUnavailable();
    QWidget *createEqCombo(const QString &label, const QString &address);
    QWidget *createEqBar(const QString &label, const QString &address);
    void setEqValue(const QString &address, int value);
    void updateEqParameterControls(bool available);
    void updateEqGraph();
    void refreshEq();
    enum class PreampChannel { A, B };
    struct PreampEditorState {
        EffectEditorPanel *editor = nullptr;
        EffectArtworkWidget *artwork = nullptr;
        EffectModelBrowser *browser = nullptr;
        QComboBox *type = nullptr;
        QComboBox *customType = nullptr;
        QComboBox *speakerType = nullptr;
        ModernToggleSwitch *globalState = nullptr;
        ModernToggleSwitch *bright = nullptr;
        ModernToggleSwitch *solo = nullptr;
        QWidget *brightControl = nullptr;
        QWidget *customPreampSection = nullptr;
        QWidget *customSpeakerSection = nullptr;
        QList<QComboBox *> combos;
        QList<ParameterBar *> bars;
        QList<ModernToggleSwitch *> toggles;
    };
    PreampEditorState &preampState(PreampChannel channel);
    const PreampEditorState &preampState(PreampChannel channel) const;
    QString preampAddress(PreampChannel channel, int offset) const;
    EffectEditorPanel *createPreampEditor(PreampChannel channel);
    QWidget *createPreampCombo(PreampChannel channel,
                               const QString &label, int offset);
    QWidget *createPreampBar(PreampChannel channel,
                             const QString &label, int offset);
    QWidget *createPreampToggle(PreampChannel channel,
                                const QString &label, int offset,
                                ModernToggleSwitch **target = nullptr);
    bool hasValidPreampBuffer() const;
    void setPreampUnavailable();
    void setPreampValue(PreampChannel channel, int offset, int value);
    void setPreampType(PreampChannel channel, int index);
    void setPreampGlobalState(bool on);
    void updatePreampConditionalSections(PreampChannel channel);
    void updatePreampParameterControls(PreampChannel channel,
                                       bool available);
    void refreshPreamp(PreampChannel channel);
    void refreshPreampGlobalState();
    void showFxEditor(FxSlot slot);
    void refreshFx(FxSlot slot);
    void fx1StateChanged(bool available, bool on);
    void fx2StateChanged(bool available, bool on);
    void showPedalEditor(bool footVolumeContext);
    void refreshPedalFx();
    void pedalFxActivityChanged(bool available, bool pedalFxActive,
                                bool footVolumeActive);
    void clearPedalSelection();
    void showNoiseSuppressorEditor(NoiseSuppressorSlot slot);
    void refreshNoiseSuppressors();
    void clearNoiseSuppressorSelection();
    void refreshSendReturn();
    void clearSendReturnSelection();
    QWidget *createChannelRoutingEditor();
    void updateChannelRoutingPage(int mode);
    void updateChannelRoutingControls(bool available);
    void refreshChannelRouting();
    void setChannelRoutingValue(const QString &address, int value);
    void setChannelMode(int index);
    void setChannelSelect(int index);
    void setChannelDelay(int value);
    void setDynamicSense(int value);
    void refreshSignalChainModel();
    void refreshOutputSelectHeader();
    void requestOutputSystemData();
    void pollOutputSystemData();
    bool hasSourceValue(const QString &area,
                        const QString &hex1,
                        const QString &hex2,
                        const QString &hex3) const;
    void rebuildSignalChainView();
    void applyResponsiveSignalChainLayout();
    SignalChainModule *createSignalChainModule(const modernSignalChainModel::Entry &entry);

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:

    QLabel *patchNumber;
    QLabel *patchName;
    QComboBox *outputSelectCombo = nullptr;
    bool outputSystemDataRequested = false;
    bool outputSystemDataReady = false;
    PatchSidebar *patchSidebar = nullptr;

    SignalChainModule *reverbCard = nullptr;
    SignalChainModule *compCard = nullptr;
    SignalChainModule *oddsCard = nullptr;
    SignalChainModule *delayCard = nullptr;
    SignalChainModule *chorusCard = nullptr;
    SignalChainModule *eqCard = nullptr;
    SignalChainModule *preampACard = nullptr;
    SignalChainModule *preampBCard = nullptr;
    SignalChainModule *fx1Card = nullptr;
    SignalChainModule *fx2Card = nullptr;
    SignalChainModule *pedalFxCard = nullptr;
    SignalChainModule *footVolumeCard = nullptr;
    SignalChainModule *ns1Card = nullptr;
    SignalChainModule *ns2Card = nullptr;
    SignalChainModule *sendReturnCard = nullptr;
    SignalJunction *splitJunction = nullptr;
    QStackedWidget *effectEditorStack = nullptr;
    EffectEditorPanel *reverbEditor = nullptr;
    EffectEditorPanel *compEditor = nullptr;
    EffectEditorPanel *oddsEditor = nullptr;
    EffectEditorPanel *delayEditor = nullptr;
    EffectEditorPanel *chorusEditor = nullptr;
    QWidget *eqEditor = nullptr;
    PreampEditorState preampA;
    PreampEditorState preampB;
    ModernFxEditor *fx1Editor = nullptr;
    ModernFxEditor *fx2Editor = nullptr;
    ModernPedalFxEditor *pedalFxEditor = nullptr;
    ModernNoiseSuppressorEditor *ns1Editor = nullptr;
    ModernNoiseSuppressorEditor *ns2Editor = nullptr;
    ModernSendReturnEditor *sendReturnEditor = nullptr;
    QWidget *channelRoutingEditor = nullptr;
    QWidget *channelRoutingDiagram = nullptr;
    QComboBox *channelMode = nullptr;
    QPushButton *channelAButton = nullptr;
    QPushButton *channelBButton = nullptr;
    QStackedWidget *channelRoutingStack = nullptr;
    ParameterBar *channelDelay = nullptr;
    ParameterBar *dynamicSense = nullptr;
    EffectArtworkWidget *reverbArtwork = nullptr;
    EffectModelBrowser *reverbModelBrowser = nullptr;
    EffectArtworkWidget *oddsArtwork = nullptr;
    EffectModelBrowser *oddsModelBrowser = nullptr;
    EffectArtworkWidget *delayArtwork = nullptr;
    EffectModelBrowser *delayModelBrowser = nullptr;
    EffectArtworkWidget *chorusArtwork = nullptr;
    EffectModelBrowser *chorusModeBrowser = nullptr;
    QString selectedEditor = "REVERB";
    QComboBox *reverbType = nullptr;
    QLabel *reverbTypeDisplay = nullptr;
    QComboBox *reverbLowCut = nullptr;
    QComboBox *reverbHighCut = nullptr;
    ModernToggleSwitch *reverbOnOff = nullptr;
    ParameterBar *reverbSpringSensitivity = nullptr;
    QList<ParameterBar *> reverbBars;
    QComboBox *compType = nullptr;
    QLabel *compTypeDisplay = nullptr;
    ModernToggleSwitch *compOnOff = nullptr;
    QStackedWidget *compModeStack = nullptr;
    EffectModelBrowser *compModelBrowser = nullptr;
    QList<ParameterBar *> compBars;
    QComboBox *oddsType = nullptr;
    QComboBox *oddsCustomType = nullptr;
    ModernToggleSwitch *oddsOnOff = nullptr;
    ModernToggleSwitch *oddsSoloSwitch = nullptr;
    QWidget *oddsCustomSection = nullptr;
    QList<ParameterBar *> oddsBars;
    QComboBox *delayType = nullptr;
    ModernToggleSwitch *delayOnOff = nullptr;
    ModernToggleSwitch *delayWarpSwitch = nullptr;
    QStackedWidget *delayPageStack = nullptr;
    QStackedWidget *delayExtraStack = nullptr;
    QList<QComboBox *> delayCombos;
    QList<ParameterBar *> delayBars;
    QComboBox *chorusMode = nullptr;
    ModernToggleSwitch *chorusOnOff = nullptr;
    QList<QComboBox *> chorusCombos;
    QList<ParameterBar *> chorusBars;
    ModernToggleSwitch *eqOnOff = nullptr;
    ModernEqGraph *eqGraph = nullptr;
    QList<QComboBox *> eqCombos;
    QList<ParameterBar *> eqBars;
    bool backendIsConnected = false;
    bool backendHasPatchData = false;
    modernSignalChainModel signalChainModel;
    ModernPatchListModel patchListModel;
    SignalChainPanel *signalChainPanel = nullptr;
    QScrollArea *signalChainScroll = nullptr;
    QHBoxLayout *signalFlowLayout = nullptr;
    QGridLayout *signalPathsLayout = nullptr;
    QWidget *signalParallelPaths = nullptr;
    QList<SignalChainModule *> signalChainModules;
    QList<SignalJunction *> signalChainJunctions;
    QList<SignalConnector *> signalChainConnectors;
    QLabel *signalPathALabel = nullptr;
    QLabel *signalPathBLabel = nullptr;
};

#endif
