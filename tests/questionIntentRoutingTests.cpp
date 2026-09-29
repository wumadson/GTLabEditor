#include "ai/LocalGT10Provider.h"
#include "ai/knowledge/QuestionIntentRouter.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonObject>
#include <QTextStream>
#include <QTimer>

using namespace Ai;
using namespace Ai::Knowledge;

namespace {

int assertions = 0;
int failures = 0;

void expect(bool condition, const QString &name)
{
    ++assertions;
    if (!condition) {
        ++failures;
        QTextStream(stderr) << "FAIL: " << name << '\n';
    }
}

bool hasEntity(const QuestionRoutingResult &result, QuestionEntityKind kind,
               const QString &name)
{
    for (const QuestionEntity &entity : result.entities) {
        if (entity.kind == kind
            && entity.canonicalName.compare(name, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

struct CorpusCase {
    const char *prompt;
    QuestionIntent intent;
    QuestionDestination destination;
};

AIContext context()
{
    AIContext result;
    result.root.insert("patch", QJsonObject{{"number", "U29-1"},
                                             {"name", "WC ALL-IN-ONE"}});
    QJsonArray effects;
    effects.append(QJsonObject{{"id", "preampA"}, {"enabled", true},
                                {"type", "MATCH Drive"}, {"gain", "46"},
                                {"treble", "55"}, {"presence", "52"}});
    effects.append(QJsonObject{{"id", "odDs"}, {"enabled", false},
                                {"type", "Blues OD"}, {"drive", "25"}});
    effects.append(QJsonObject{{"id", "delay"}, {"enabled", true},
                                {"type", "Dual L/R"}, {"feedback", "48"}});
    effects.append(QJsonObject{{"id", "reverb"}, {"enabled", true},
                                {"type", "Modulate"},
                                {"density", "8"},
                                {"reverbTime", "9.0 sec"}});
    result.root.insert("effects", effects);
    return result;
}

AIResponse ask(LocalGT10Provider *provider, const QString &prompt,
               const QString &language = QStringLiteral("pt-BR"))
{
    AIResponse result;
    QEventLoop loop;
    const QMetaObject::Connection connection = QObject::connect(
        provider, &AIProvider::responseReady, &loop,
        [&](const QString &, const AIResponse &response) {
            result = response;
            loop.quit();
        });
    AIRequest request;
    request.userMessage = prompt;
    request.language = language;
    request.context = context();
    provider->sendRequest(QString::number(assertions + 1), request);
    QTimer::singleShot(1000, &loop, &QEventLoop::quit);
    loop.exec();
    QObject::disconnect(connection);
    return result;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QuestionIntentRouter router;
    QuestionRoutingContext noState;
    QuestionRoutingContext recommendationState;
    recommendationState.hasRecommendation = true;

    const CorpusCase ptCases[] = {
        {"Qual a diferença do Reverb Ambience e o Modulate?",
         QuestionIntent::CompareEffectTypes, QuestionDestination::Knowledge},
        {"Qual a diferença do reverb modulate e o Ambience?",
         QuestionIntent::CompareEffectTypes, QuestionDestination::Knowledge},
        {"Qual reverb estou usando?", QuestionIntent::CurrentPatchFact,
         QuestionDestination::PatchContext},
        {"O que é Reverb Modulate?", QuestionIntent::ExplainEffectType,
         QuestionDestination::Knowledge},
        {"O que faz Density?", QuestionIntent::ExplainParameter,
         QuestionDestination::Knowledge},
        {"Por que você está sugerindo isso?", QuestionIntent::Justify,
         QuestionDestination::Tone},
        {"Por que mexer no Gain?", QuestionIntent::Justify,
         QuestionDestination::Tone},
        {"Como configurar um Assign?", QuestionIntent::HowTo,
         QuestionDestination::Knowledge},
        {"Qual delay estou usando?", QuestionIntent::CurrentPatchFact,
         QuestionDestination::PatchContext},
        {"Por que meu delay está embolando?", QuestionIntent::ExplainTone,
         QuestionDestination::Tone},
        {"Quero mais crunch", QuestionIntent::AdjustTone,
         QuestionDestination::Tone},
        {"Melhor amp ou drive?", QuestionIntent::CompareRecommendation,
         QuestionDestination::Tone}
    };
    for (int i = 0; i < int(sizeof(ptCases) / sizeof(ptCases[0])); ++i) {
        const QuestionRoutingResult result = router.route(
            QString::fromUtf8(ptCases[i].prompt), recommendationState);
        expect(result.intent == ptCases[i].intent
                   && result.destination == ptCases[i].destination,
               QString("PT corpus %1: %2 -> %3").arg(i + 1)
                   .arg(QuestionIntentRouter::intentName(result.intent),
                        QuestionIntentRouter::destinationName(result.destination)));
    }

    const CorpusCase enCases[] = {
        {"What is the difference between Reverb Ambience and Modulate?",
         QuestionIntent::CompareEffectTypes, QuestionDestination::Knowledge},
        {"Compare Reverb Modulate and Ambience",
         QuestionIntent::CompareEffectTypes, QuestionDestination::Knowledge},
        {"What reverb am I using?", QuestionIntent::CurrentPatchFact,
         QuestionDestination::PatchContext},
        {"What is Reverb Modulate?", QuestionIntent::ExplainEffectType,
         QuestionDestination::Knowledge},
        {"What does Density do?", QuestionIntent::ExplainParameter,
         QuestionDestination::Knowledge},
        {"Why did you suggest that?", QuestionIntent::Justify,
         QuestionDestination::Tone},
        {"How do I configure an Assign?", QuestionIntent::HowTo,
         QuestionDestination::Knowledge},
        {"Which delay am I using?", QuestionIntent::CurrentPatchFact,
         QuestionDestination::PatchContext},
        {"I want more crunch", QuestionIntent::AdjustTone,
         QuestionDestination::Tone},
        {"Is it better to change amp or drive?",
         QuestionIntent::CompareRecommendation, QuestionDestination::Tone}
    };
    for (int i = 0; i < int(sizeof(enCases) / sizeof(enCases[0])); ++i) {
        const QuestionRoutingResult result = router.route(
            QString::fromLatin1(enCases[i].prompt), recommendationState);
        expect(result.intent == enCases[i].intent
                   && result.destination == enCases[i].destination,
               QString("EN corpus %1: %2 -> %3").arg(i + 1)
                   .arg(QuestionIntentRouter::intentName(result.intent),
                        QuestionIntentRouter::destinationName(result.destination)));
    }

    const QuestionRoutingResult comparison = router.route(
        "Qual a diferença entre Reverb Ambience e Modulate?", noState);
    expect(comparison.intent == QuestionIntent::CompareEffectTypes
               && comparison.intent != QuestionIntent::CurrentPatchFact,
           "negative: explicit comparison never becomes current-patch fact");
    expect(hasEntity(comparison, QuestionEntityKind::EffectFamily, "reverb")
               && hasEntity(comparison, QuestionEntityKind::EffectType, "Ambience")
               && hasEntity(comparison, QuestionEntityKind::EffectType, "Modulate"),
           "comparison resolves family and both catalog types");

    const QuestionRoutingResult technicalAfterRecommendation = router.route(
        "O que é Reverb Modulate?", recommendationState);
    expect(technicalAfterRecommendation.intent
               == QuestionIntent::ExplainEffectType
               && technicalAfterRecommendation.destination
                      == QuestionDestination::Knowledge,
           "technical question is not hijacked by recommendation state");
    const QuestionRoutingResult why = router.route(
        "Por que você sugeriu isso?", recommendationState);
    expect(why.intent == QuestionIntent::Justify
               && why.destination == QuestionDestination::Tone,
           "justify with recommendation is not Unknown or Clarify");
    const QuestionRoutingResult ambiguous = router.route(
        "Qual a diferença?", noState);
    expect(ambiguous.intent == QuestionIntent::Clarify
               && ambiguous.destination == QuestionDestination::Clarify,
           "context-free ambiguous comparison asks for clarification");
    QuestionRoutingContext previous;
    previous.previousComparisonEntities = comparison.entities;
    const QuestionRoutingResult continued = router.route(
        "Qual a diferença?", previous);
    expect(continued.intent == QuestionIntent::CompareEffectTypes
               && hasEntity(continued, QuestionEntityKind::EffectType, "Ambience")
               && hasEntity(continued, QuestionEntityKind::EffectType, "Modulate"),
           "ambiguous comparison reuses structured prior entities");
    QuestionRoutingContext previousParameters;
    previousParameters.previousComparisonEntities = router.route(
        "Qual a diferença entre Treble e Presence?", noState).entities;
    const QuestionRoutingResult continuedParameters = router.route(
        "Qual a diferença?", previousParameters);
    expect(continuedParameters.intent == QuestionIntent::CompareParameters
               && hasEntity(continuedParameters,
                            QuestionEntityKind::Parameter, "Treble")
               && hasEntity(continuedParameters,
                            QuestionEntityKind::Parameter, "Presence"),
           "ambiguous comparison preserves prior parameter intent");

    LocalGT10Provider provider;
    AIResponse compareResponse = ask(
        &provider, "Qual a diferença do Reverb Ambience e o Modulate?");
    expect(compareResponse.metadata.value("detectedIntent")
               == QLatin1String("COMPARE_EFFECT_TYPES")
               && compareResponse.metadata.value("destination")
                      == QLatin1String("KNOWLEDGE")
               && compareResponse.text.contains("Ambience")
               && compareResponse.text.contains("Modulate")
               && compareResponse.text.contains("patch atual"),
           "provider compares first and adds current type only as context");
    expect(compareResponse.metadata.value("resolvedEntities").toArray().size()
               >= 3
               && !compareResponse.metadata.value("intentSource").toString().isEmpty(),
           "provider exposes intent trace and resolved entities");
    AIResponse current = ask(&provider, "Qual reverb estou usando?");
    expect(current.metadata.value("detectedIntent")
               == QLatin1String("CURRENT_PATCH_FACT")
               && current.text.contains("Modulate"),
           "literal current-patch fact remains patch-aware");
    AIResponse definition = ask(&provider, "O que é Reverb Modulate?");
    expect(definition.metadata.value("detectedIntent")
               == QLatin1String("EXPLAIN_EFFECT_TYPE")
               && definition.text.contains("oscilação"),
           "effect type explanation uses supported documentation summary");
    AIResponse density = ask(&provider, "O que faz Density?");
    expect(density.metadata.value("detectedIntent")
               == QLatin1String("EXPLAIN_PARAMETER")
               && density.text.contains("densidade"),
           "parameter explanation routes to documentation");

    provider.resetConversation();
    AIResponse noRecommendation = ask(
        &provider, "Por que você está sugerindo isso?");
    expect(noRecommendation.metadata.value("detectedIntent")
               == QLatin1String("JUSTIFY")
               && noRecommendation.text.contains("Não tenho uma recomendação"),
           "justify without recommendation does not invent tone context");
    ask(&provider, "Quero mais crunch");
    AIResponse justified = ask(&provider, "Por que você está sugerindo isso?");
    expect(justified.metadata.value("detectedIntent") == QLatin1String("JUSTIFY")
               && justified.metadata.value("responseMode")
                      == QLatin1String("JUSTIFY")
               && justified.metadata.value("answered").toBool(),
           "justify resolves the active structured recommendation");

    const QuestionRoutingResult unknownEntity = router.route(
        "Qual a diferença entre Reverb Modulate e SuperCloudUltra?", noState);
    expect(unknownEntity.intent == QuestionIntent::CompareEffectTypes
               && hasEntity(unknownEntity, QuestionEntityKind::EffectType,
                            "Modulate"),
           "partial comparison preserves known entity for safe unknown response");
    AIResponse unknownResponse = ask(
        &provider,
        "Qual a diferença entre Reverb Modulate e SuperCloudUltra?");
    expect(!unknownResponse.metadata.value("answered").toBool()
               && unknownResponse.text.contains("apenas um"),
           "unknown comparison peer is not hallucinated");

    QTextStream(stdout) << "Question Intent Routing tests: assertions="
                        << assertions << " failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
