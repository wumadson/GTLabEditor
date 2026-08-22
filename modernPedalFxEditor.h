#ifndef MODERNPEDALFXEDITOR_H
#define MODERNPEDALFXEDITOR_H

#include <QObject>
#include <QVector>

class EffectEditorPanel;
class EffectModelBrowser;
class QLabel;
class ModernToggleSwitch;
class ParameterBar;
class QComboBox;
class QStackedWidget;
class QWidget;

enum class PedalEditorContext {
    General,
    FootVolume
};

class ModernPedalFxEditor : public QObject
{
    Q_OBJECT

public:
    explicit ModernPedalFxEditor(QObject *parent = nullptr);

    EffectEditorPanel *widget() const;
    void setContext(PedalEditorContext context);
    PedalEditorContext context() const;
    void refreshPedalFx(bool backendConnected, bool backendHasPatchData);

signals:
    void activityChanged(bool available, bool pedalFxActive,
                         bool footVolumeActive);

private:
    struct Binding {
        QString address;
        QWidget *container = nullptr;
        ParameterBar *bar = nullptr;
        QComboBox *combo = nullptr;
    };

    void buildEditor();
    QWidget *createModePage(int mode);
    QWidget *createCombo(const QString &label, const QString &address);
    QWidget *createBar(const QString &label, const QString &address,
                       int centerRaw = -1);
    QWidget *createFootVolumeSection();
    QWidget *createPedalBendSection();
    QWidget *createWahSection();
    QWidget *createCustomWahSection();

    bool bufferContains(const QString &address) const;
    bool hasValidBuffer(bool backendConnected,
                        bool backendHasPatchData) const;
    int readValue(const QString &address) const;
    void writeValue(const QString &address, int raw);
    QString displayValue(const QString &address, int raw) const;
    void setMode(int raw, bool writeBackend);
    void updateControls(bool controlsAvailable);
    void refreshBindings();
    void updateCustomWahVisibility();
    void updateContextPresentation();
    void emitActivity();

    EffectEditorPanel *editor = nullptr;
    EffectModelBrowser *browser = nullptr;
    ModernToggleSwitch *stateToggle = nullptr;
    QStackedWidget *modeStack = nullptr;
    QLabel *modeDisplay = nullptr;
    QLabel *contextMessage = nullptr;
    QVector<Binding> bindings;
    QVector<int> modeRawValues;
    QVector<QWidget *> customWahSections;
    QVector<QWidget *> footVolumeSections;
    PedalEditorContext editorContext = PedalEditorContext::General;
    bool refreshing = false;
    bool available = false;
    bool stateOn = false;
    int currentMode = 0;
};

#endif
