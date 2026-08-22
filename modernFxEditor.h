#ifndef MODERNFXEDITOR_H
#define MODERNFXEDITOR_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

class QAbstractButton;
class EffectArtworkWidget;
class EffectEditorPanel;
class EffectModelBrowser;
class ModernToggleSwitch;
class ParameterBar;
class QComboBox;
class QStackedWidget;
class QWidget;

namespace FxPresentation {
QString formatRhythmicDivision(const QString &value,
                               bool *recognized = nullptr);
}

enum class FxSlot {
    FX1,
    FX2
};

enum class FxControlKind {
    Bar,
    BipolarBar,
    Combo,
    Toggle,
    SegmentedBar,
    StepRateBar,
    ZeroChoiceBar,
    TwoByteSegmentedBar,
    MappedBar
};

struct FxAddress {
    enum class Scope {
        Relative,
        External
    };

    Scope scope = Scope::Relative;
    int relativeBank = 0;
    QString externalBank;
    QString middleByte = "00";
    QString offset;
    bool twoByte = false;

    static FxAddress relative(int bank, const QString &offset,
                              bool twoByte = false);
    static FxAddress external(const QString &bank,
                              const QString &middleByte,
                              const QString &offset,
                              bool twoByte = false);
};

struct FxCondition {
    bool enabled = false;
    FxAddress controller;
    QVector<int> visibleRawValues;
};

struct FxParameterSpec {
    FxAddress address;
    FxControlKind kind = FxControlKind::Bar;
    QString section;
    QString labelOverride;
    int centerRaw = -1;
    int continuousMaximum = -1;
    FxCondition condition;
};

class ModernFxEditor : public QObject
{
    Q_OBJECT

public:
    explicit ModernFxEditor(FxSlot slot, QObject *parent = nullptr);

    EffectEditorPanel *widget() const;
    FxSlot slot() const;
    QString translatedBank(int relativeBank) const;
    void refreshFx(bool backendConnected, bool backendHasPatchData);

signals:
    void stateChanged(bool available, bool on);

private:
    struct TypeEntry {
        int raw = -1;
        QString name;
    };

    struct ControlBinding {
        FxParameterSpec spec;
        QWidget *container = nullptr;
        ParameterBar *bar = nullptr;
        QComboBox *combo = nullptr;
        ModernToggleSwitch *toggle = nullptr;
        QAbstractButton *offButton = nullptr;
        int algorithmRaw = -1;
        bool visualOnly = false;
    };

    struct AlgorithmSpec {
        int raw = -1;
        QVector<FxParameterSpec> parameters;
        QStringList sideBySideSections;
    };

    void buildEditor();
    void buildTypes();
    void buildPages();
    QVector<AlgorithmSpec> phaseOneAlgorithms() const;
    QVector<AlgorithmSpec> phaseTwoAlgorithms() const;
    QVector<AlgorithmSpec> phaseThreeAAlgorithms() const;
    QVector<AlgorithmSpec> phaseThreeB1Algorithms() const;
    QVector<AlgorithmSpec> phaseThreeB2Algorithms() const;
    QWidget *createAlgorithmPage(const TypeEntry &type,
                                 const AlgorithmSpec *spec);
    QWidget *createParameterControl(const FxParameterSpec &spec,
                                    int algorithmRaw);
    QWidget *createPlaceholderPage(const QString &algorithmName);

    QString bankForAddress(const FxAddress &address) const;
    bool hasValidBuffer(bool backendConnected,
                        bool backendHasPatchData) const;
    bool bufferContains(const FxAddress &address) const;
    int readValue(const FxAddress &address) const;
    void writeValue(const FxAddress &address, int raw);
    QString displayValue(const FxAddress &address, int raw) const;

    void setFxType(int raw, bool writeBackend);
    void updateBrowserForRaw(int raw);
    void updateControls(bool available);
    void refreshControlsForType(int raw);
    void applyConditionalRules();
    QString categoryForRaw(int raw) const;
    QString typeNameForRaw(int raw) const;

    FxSlot fxSlot;
    EffectEditorPanel *editor = nullptr;
    EffectModelBrowser *browser = nullptr;
    ModernToggleSwitch *stateToggle = nullptr;
    QComboBox *hiddenType = nullptr;
    QStackedWidget *algorithmStack = nullptr;
    EffectArtworkWidget *artwork = nullptr;
    QVector<TypeEntry> types;
    QVector<int> browserRawValues;
    QVector<int> pageRawValues;
    QVector<ControlBinding> controls;
    bool refreshing = false;
    bool available = false;
};

#endif
