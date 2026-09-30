#ifndef AI_ASSISTANTFEATUREREGISTRY_H
#define AI_ASSISTANTFEATUREREGISTRY_H

#include <QList>
#include <QString>
#include <QStringList>

namespace Ai {

enum class AssistantFeatureId {
    PatchExplanation,
    ToneGuidance,
    ToneBuilding,
    GT10Knowledge,
    EffectComparison,
    ParameterAssessment,
    ControlGuidance,
    SignalRouting,
    PatchSystemScope,
    CapabilityLists,
    OfficialDocumentation,
    FactoryPresetReference,
    AudioAnalysis,
    ArtistProfiles,
    AutomaticPatchChanges,
    AutomaticMidiControl,
    CloudProvider,
    PatchComparison,
    PickupAdaptation
};

struct AssistantFeature
{
    AssistantFeatureId id = AssistantFeatureId::PatchExplanation;
    QString key;
    bool enabled = false;
    QString categoryEn;
    QString categoryPtBr;
    QString titleEn;
    QString titlePtBr;
    QStringList examplesEn;
    QStringList examplesPtBr;
    QString limitationEn;
    QString limitationPtBr;
};

class AssistantFeatureRegistry
{
public:
    AssistantFeatureRegistry();

    QList<AssistantFeature> features() const;
    QList<AssistantFeature> enabledFeatures() const;
    QList<AssistantFeature> disabledFeatures() const;
    QStringList exampleQuestions(const QString &language) const;
    QStringList limitations(const QString &language) const;
    QString capabilityDiscoveryResponse(const QString &language,
                                        bool hasPatchContext) const;

private:
    bool portuguese(const QString &language) const;
    QList<AssistantFeature> registeredFeatures;
};

} // namespace Ai

#endif // AI_ASSISTANTFEATUREREGISTRY_H
