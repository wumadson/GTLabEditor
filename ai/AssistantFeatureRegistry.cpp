#include "ai/AssistantFeatureRegistry.h"

#include <QMap>
#include <QSet>

namespace Ai {

AssistantFeatureRegistry::AssistantFeatureRegistry()
{
    const auto enabled = [this](AssistantFeatureId id, const char *key,
                                const char *categoryEn, const char *categoryPt,
                                const char *titleEn, const char *titlePt,
                                const QStringList &examplesEn = {},
                                const QStringList &examplesPt = {}) {
        registeredFeatures.append({id, QString::fromLatin1(key), true,
            QString::fromUtf8(categoryEn), QString::fromUtf8(categoryPt),
            QString::fromUtf8(titleEn), QString::fromUtf8(titlePt),
            examplesEn, examplesPt, {}, {}});
    };
    const auto disabled = [this](AssistantFeatureId id, const char *key,
                                 const char *limitationEn,
                                 const char *limitationPt) {
        registeredFeatures.append({id, QString::fromLatin1(key), false,
            {}, {}, {}, {}, {}, {}, QString::fromUtf8(limitationEn),
            QString::fromUtf8(limitationPt)});
    };

    enabled(AssistantFeatureId::PatchExplanation, "patch_explanation",
            "Current patch", "Patch atual",
            "explain how the current preset tends to sound",
            "explicar como o preset atual tende a soar",
            {QStringLiteral("Explain this preset.")},
            {QString::fromUtf8("Me explique esse preset.")});
    enabled(AssistantFeatureId::ToneGuidance, "tone_guidance",
            "Tone", "Timbre",
            "suggest small, contextual tone adjustments",
            "sugerir pequenos ajustes de timbre no contexto do patch",
            {QStringLiteral("I want more crunch without losing definition.")},
            {QString::fromUtf8("Quero deixar esse timbre mais crunch sem perder definição.")});
    enabled(AssistantFeatureId::GT10Knowledge, "gt10_knowledge",
            "GT-10", "GT-10",
            "explain GT-10 effects, parameters, and technical features",
            "explicar efeitos, parâmetros e recursos técnicos da GT-10");
    enabled(AssistantFeatureId::EffectComparison, "effect_comparison",
            "Effects", "Efeitos",
            "compare implemented effect types and parameters",
            "comparar tipos de efeito e parâmetros conhecidos",
            {QStringLiteral("What is the difference between Ambience and Modulate?")},
            {QString::fromUtf8("Qual a diferença entre Ambience e Modulate?")});
    enabled(AssistantFeatureId::ParameterAssessment, "parameter_assessment",
            "Tone", "Timbre",
            "assess whether a parameter value fits the current context",
            "avaliar se um valor de parâmetro faz sentido no contexto atual",
            {QStringLiteral("Does this 100 ms Pre Delay make sense in this preset?")},
            {QString::fromUtf8("Esse Pre Delay de 100 ms faz sentido nesse preset?")});
    enabled(AssistantFeatureId::ControlGuidance, "control_guidance",
            "Controls", "Controles",
            "guide Assign, CTL, EXP, and EXP SW configuration",
            "orientar configurações de Assign, CTL, EXP e EXP SW",
            {QStringLiteral("How do I configure an Assign?"),
             QStringLiteral("How can I use EXP to control a parameter?")},
            {QString::fromUtf8("Como configurar um Assign?"),
             QString::fromUtf8("Como usar EXP para controlar um parâmetro?")});
    enabled(AssistantFeatureId::SignalRouting, "signal_routing",
            "GT-10", "GT-10",
            "explain Signal Chain and routing",
            "explicar Signal Chain e routing");
    enabled(AssistantFeatureId::PatchSystemScope, "patch_system_scope",
            "GT-10", "GT-10",
            "explain Patch versus System scope",
            "explicar o escopo Patch versus System");
    enabled(AssistantFeatureId::CapabilityLists, "capability_lists",
            "Effects", "Efeitos",
            "list effect and model types present in the local catalog",
            "listar efeitos e modelos presentes no catálogo local",
            {QStringLiteral("Which Delays are available on the GT-10?")},
            {QString::fromUtf8("Quais Delays existem na GT-10?")});
    enabled(AssistantFeatureId::OfficialDocumentation,
            "official_documentation", "GT-10", "GT-10",
            "consult the bundled official documentation",
            "consultar a documentação oficial incluída no aplicativo");
    enabled(AssistantFeatureId::FactoryPresetReference,
            "factory_preset_reference", "GT-10", "GT-10",
            "answer questions covered by the factory preset reference",
            "responder perguntas cobertas pela referência de presets de fábrica");

    disabled(AssistantFeatureId::AudioAnalysis, "audio_analysis",
             "analyze audio", "analisar áudio");
    disabled(AssistantFeatureId::ArtistProfiles, "artist_profiles",
             "provide faithful artist tone cloning or Artist Profiles",
             "clonar fielmente timbres de artistas ou fornecer Artist Profiles");
    disabled(AssistantFeatureId::AutomaticPatchChanges,
             "automatic_patch_changes",
             "apply changes or edit the patch automatically",
             "aplicar mudanças ou alterar o patch automaticamente");
    disabled(AssistantFeatureId::AutomaticMidiControl,
             "automatic_midi_control", "control the GT-10 automatically",
             "controlar a GT-10 automaticamente");
    disabled(AssistantFeatureId::CloudProvider, "cloud_provider",
             "use an external or cloud AI provider",
             "usar provider externo ou IA em nuvem");
    disabled(AssistantFeatureId::PatchComparison, "patch_comparison",
             "compare two different patches",
             "comparar dois patches diferentes");
    disabled(AssistantFeatureId::PickupAdaptation, "pickup_adaptation",
             "adapt settings automatically for humbuckers or single coils",
             "adaptar ajustes automaticamente para humbuckers ou single coils");
}

bool AssistantFeatureRegistry::portuguese(const QString &language) const
{
    return language.startsWith(QLatin1String("pt"), Qt::CaseInsensitive);
}

QList<AssistantFeature> AssistantFeatureRegistry::features() const
{
    return registeredFeatures;
}

QList<AssistantFeature> AssistantFeatureRegistry::enabledFeatures() const
{
    QList<AssistantFeature> result;
    for (const AssistantFeature &feature : registeredFeatures) {
        if (feature.enabled)
            result.append(feature);
    }
    return result;
}

QList<AssistantFeature> AssistantFeatureRegistry::disabledFeatures() const
{
    QList<AssistantFeature> result;
    for (const AssistantFeature &feature : registeredFeatures) {
        if (!feature.enabled)
            result.append(feature);
    }
    return result;
}

QStringList AssistantFeatureRegistry::exampleQuestions(
    const QString &language) const
{
    QStringList result;
    for (const AssistantFeature &feature : enabledFeatures()) {
        const QStringList examples = portuguese(language)
            ? feature.examplesPtBr : feature.examplesEn;
        for (const QString &example : examples) {
            if (!example.isEmpty() && !result.contains(example))
                result.append(example);
        }
    }
    return result.mid(0, 8);
}

QStringList AssistantFeatureRegistry::limitations(
    const QString &language) const
{
    QStringList result;
    for (const AssistantFeature &feature : disabledFeatures()) {
        const QString limitation = portuguese(language)
            ? feature.limitationPtBr : feature.limitationEn;
        if (!limitation.isEmpty())
            result.append(limitation);
    }
    return result;
}

QString AssistantFeatureRegistry::capabilityDiscoveryResponse(
    const QString &language, bool hasPatchContext) const
{
    const bool pt = portuguese(language);
    QString response = pt
        ? QString::fromUtf8("Posso ajudar principalmente nestas áreas:\n")
        : QStringLiteral("I can mainly help in these areas:\n");

    QMap<QString, QStringList> categories;
    QStringList categoryOrder;
    for (const AssistantFeature &feature : enabledFeatures()) {
        const QString category = pt ? feature.categoryPtBr : feature.categoryEn;
        const QString title = pt ? feature.titlePtBr : feature.titleEn;
        if (category.isEmpty() || title.isEmpty())
            continue;
        if (!categories.contains(category))
            categoryOrder.append(category);
        categories[category].append(title);
    }
    for (const QString &category : categoryOrder) {
        response += QStringLiteral("\n• %1: %2.")
            .arg(category, categories.value(category).join(QStringLiteral("; ")));
    }

    response += pt ? QString::fromUtf8("\n\nVocê pode perguntar, por exemplo:\n")
                   : QStringLiteral("\n\nFor example, you can ask:\n");
    for (const QString &example : exampleQuestions(language))
        response += QStringLiteral("\n• “%1”").arg(example);

    const QStringList unavailable = limitations(language);
    response += pt ? QString::fromUtf8("\n\nLimitações atuais: ainda não posso ")
                   : QStringLiteral("\n\nCurrent limitations: I cannot yet ");
    response += unavailable.join(pt ? QStringLiteral("; ")
                                    : QStringLiteral("; "));
    response += QLatin1Char('.');

    if (hasPatchContext) {
        response += pt
            ? QString::fromUtf8("\n\nSe quiser começar, posso explicar o preset carregado agora.")
            : QStringLiteral("\n\nIf you want to start, I can explain the preset currently loaded.");
    }
    return response;
}

} // namespace Ai
