#include "ai/LocalGT10Provider.h"
#include "ai/knowledge/QuestionIntentRouter.h"
#include "ai/knowledge/DocumentationKnowledgeSource.h"

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

AIContext patchContext(bool ambiguousPreDelay = false)
{
    AIContext context;
    context.root.insert("patch", QJsonObject{{"number", "U29-1"},
                                               {"name", "WC ALL-IN-ONE"}});
    context.root.insert("routing", QJsonObject{{"mode", "Dual L/R"},
                                                 {"channelDelay", "3 ms"}});
    QJsonArray effects;
    effects.append(QJsonObject{{"id", "preampA"}, {"enabled", true},
                                {"type", "MATCH Drive"}, {"gain", "55"},
                                {"bass", "40"}, {"middle", "60"},
                                {"treble", "57"}, {"presence", "30"},
                                {"gainSwitch", "Middle"}});
    effects.append(QJsonObject{{"id", "eq"}, {"enabled", true},
                                {"lowCut", "110 Hz"},
                                {"highCut", "Flat"}});
    effects.append(QJsonObject{{"id", "delay"}, {"enabled", true},
                                {"type", "Dual L/R"},
                                {"delay1", QJsonObject{{"feedback", "60"},
                                                        {"level", "60"}}},
                                {"delay2", QJsonObject{{"feedback", "60"},
                                                        {"level", "55"}}}});
    effects.append(QJsonObject{{"id", "reverb"}, {"enabled", true},
                                {"type", "Modulate"},
                                {"reverbTime", "9.0 sec"},
                                {"preDelay", "100 ms"}, {"density", "8"},
                                {"effectLevel", "40"}});
    effects.append(QJsonObject{{"id", "chorus"},
                                {"enabled", ambiguousPreDelay},
                                {"preDelay", "3.5 ms"}});
    context.root.insert("effects", effects);
    return context;
}

const QuestionEntity *parameter(const QuestionRoutingResult &result)
{
    for (const QuestionEntity &entity : result.entities) {
        if (entity.kind == QuestionEntityKind::Parameter)
            return &entity;
    }
    return nullptr;
}

QuestionRoutingResult route(QuestionIntentRouter *router,
                            const QString &prompt, const AIContext &context,
                            bool hasAssessment = false)
{
    QuestionRoutingContext state;
    state.patchContext = &context;
    state.hasAssessment = hasAssessment;
    return router->route(prompt, state);
}

AIResponse ask(LocalGT10Provider *provider, const QString &prompt,
               const AIContext &context,
               const QString &language = QStringLiteral("pt-BR"))
{
    AIResponse response;
    QEventLoop loop;
    const auto connection = QObject::connect(
        provider, &AIProvider::responseReady, &loop,
        [&](const QString &, const AIResponse &value) {
            response = value;
            loop.quit();
        });
    AIRequest request;
    request.userMessage = prompt;
    request.language = language;
    request.context = context;
    provider->sendRequest(QString::number(assertions + 1), request);
    QTimer::singleShot(1000, &loop, &QEventLoop::quit);
    loop.exec();
    QObject::disconnect(connection);
    return response;
}

void expectAssessment(QuestionIntentRouter *router, const AIContext &context,
                      const QString &prompt, const QString &block,
                      const QString &id, const QString &label)
{
    const QuestionRoutingResult result = route(router, prompt, context);
    const QuestionEntity *entity = parameter(result);
    expect(result.intent == QuestionIntent::AssessCurrentSetting
               && result.destination == QuestionDestination::Tone,
           label + " intent (" + QuestionIntentRouter::intentName(result.intent)
               + ")");
    expect(entity && entity->family == block && entity->id == id,
           label + " entity");
}
} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QuestionIntentRouter router;
    const AIContext context = patchContext();

    const struct Case {
        const char *prompt;
        const char *block;
        const char *parameter;
    } pt[] = {
        {"Esse Pre Delay de 100 ms faz sentido nesse preset?", "reverb", "preDelay"},
        {"Esse Pre Delay de 100 ms no reverb faz sentido?", "reverb", "preDelay"},
        {"9 segundos de reverb é muito?", "reverb", "reverbTime"},
        {"Feedback 60 está alto?", "delay", "feedback"},
        {"Esse Gain está baixo?", "preampA", "gain"},
        {"Esse Low Cut está bom?", "eq", "lowCut"},
        {"Presence 30 faz sentido?", "preampA", "presence"},
        {"Esse Channel Delay de 3 ms está bom?", "preampGlobal", "channelDelay"},
        {"Esse Effect Level está exagerado?", "reverb", "effectLevel"}
    };
    for (int i = 0; i < int(sizeof(pt) / sizeof(pt[0])); ++i)
        expectAssessment(&router, context, QString::fromUtf8(pt[i].prompt),
                         pt[i].block, pt[i].parameter,
                         QString("PT %1").arg(i + 1));

    const struct Case en[] = {
        {"Does this Reverb Pre Delay of 100 ms make sense?", "reverb", "preDelay"},
        {"Is 9 seconds of reverb too much?", "reverb", "reverbTime"},
        {"Is Feedback 60 too high?", "delay", "feedback"},
        {"Is this Gain too low?", "preampA", "gain"},
        {"Is this Low Cut setting okay?", "eq", "lowCut"},
        {"Does Presence 30 make sense?", "preampA", "presence"},
        {"Is this Channel Delay of 3 ms too high?", "preampGlobal", "channelDelay"},
        {"Is this Effect Level too much?", "reverb", "effectLevel"}
    };
    for (int i = 0; i < int(sizeof(en) / sizeof(en[0])); ++i)
        expectAssessment(&router, context, QString::fromLatin1(en[i].prompt),
                         en[i].block, en[i].parameter,
                         QString("EN %1").arg(i + 1));

    expectAssessment(&router, context, "Pre Delay do Reverb está bom?",
                     "reverb", "preDelay", "family Reverb.preDelay");
    AIContext chorusOnly = patchContext();
    QJsonArray chorusEffects = chorusOnly.root.value("effects").toArray();
    for (int i = 0; i < chorusEffects.size(); ++i) {
        QJsonObject item = chorusEffects.at(i).toObject();
        if (item.value("id") == QLatin1String("chorus")) {
            item.insert("enabled", true);
            chorusEffects[i] = item;
        }
    }
    chorusOnly.root.insert("effects", chorusEffects);
    expectAssessment(&router, chorusOnly, "Pre Delay do Chorus está bom?",
                     "chorus", "preDelay", "family Chorus.preDelay");
    expectAssessment(&router, context, "Channel Delay está bom?",
                     "preampGlobal", "channelDelay", "Channel Delay");

    const struct EntityCase {
        const char *prompt;
        const char *block;
        const char *id;
    } compounds[] = {
        {"O que faz Delay Time?", "delay", "delayTime"},
        {"O que faz Reverb Time?", "reverb", "reverbTime"},
        {"O que faz Gain Switch no preamp?", "preampA", "gainSwitch"},
        {"O que é Source Mode?", "assign", "sourceMode"},
        {"O que significa Target Min?", "assign", "targetMin"},
        {"O que significa Target Max?", "assign", "targetMax"}
    };
    for (int i = 0; i < int(sizeof(compounds) / sizeof(compounds[0])); ++i) {
        const QuestionRoutingResult result = route(
            &router, QString::fromLatin1(compounds[i].prompt), context);
        const QuestionEntity *entity = parameter(result);
        expect(entity && entity->family == QLatin1String(compounds[i].block)
                   && entity->id == QLatin1String(compounds[i].id),
               QString("compound entity %1").arg(i + 1));
    }

    const QuestionRoutingResult plainPreDelay = route(
        &router, "O que é Pre Delay?", context);
    const QuestionEntity *plainEntity = parameter(plainPreDelay);
    expect(plainEntity && plainEntity->family != QLatin1String("delay"),
           "negative: Pre Delay never resolves as DELAY family");
    const QuestionRoutingResult channel = route(
        &router, "O que é Channel Delay?", context);
    expect(parameter(channel) && parameter(channel)->id == "channelDelay"
               && parameter(channel)->id != "delayTime",
           "negative: Channel Delay wins over generic Delay");

    const QuestionRoutingResult ambiguous = route(
        &router, "Esse Pre Delay está bom?", patchContext(true));
    expect(ambiguous.intent == QuestionIntent::Clarify
               && ambiguous.destination == QuestionDestination::Clarify,
           "ambiguity with active Chorus and Reverb asks to clarify");

    LocalGT10Provider provider;
    AIResponse response = ask(&provider,
        "Esse Pre Delay de 100 ms no reverb faz sentido nesse preset?",
        context);
    const QJsonObject trace = response.metadata
        .value("parameterAssessment").toObject();
    expect(response.metadata.value("detectedIntent")
               == QLatin1String("ASSESS_CURRENT_SETTING")
               && response.metadata.value("responseMode")
                  == QLatin1String("ASSESS"),
           "provider exposes assessment intent and mode");
    expect(trace.value("resolvedBlock") == QLatin1String("reverb")
               && trace.value("resolvedParameter") == QLatin1String("preDelay")
               && trace.value("currentValue") == QLatin1String("100 ms"),
           "assessment trace contains resolved real patch value");
    expect(!response.text.startsWith("O patch atual usa Delay"),
           "current-patch Delay fact cannot hijack assessment");

    response = ask(&provider, "Por quê?", context);
    expect(response.metadata.value("route")
               == QLatin1String("parameter_assessment_justify")
               && response.text.contains("100 ms"),
           "why follow-up justifies last assessment");
    response = ask(&provider, "E se eu baixar para 60 ms?", context);
    expect(response.metadata.value("responseMode")
               == QLatin1String("HYPOTHETICAL_COMPARE")
               && response.text.contains("100 ms")
               && response.text.contains("60 ms"),
           "hypothetical compares current and proposed values read-only");

    response = ask(&provider,
        "Esse Pre Delay de 80 ms no reverb faz sentido?", context);
    expect(response.text.contains("100 ms") && response.text.contains("80 ms"),
           "different user value is not presented as current patch value");

    response = ask(&provider, "Esse SuperDepth 80 faz sentido?", context);
    expect(response.metadata.value("detectedIntent") == QLatin1String("UNKNOWN")
               && !response.text.contains("SuperDepth é"),
           "unknown parameter is never invented");

    const LocalDocumentationIndex documentation =
        officialGT10DocumentationIndex();
    const QList<DocumentationSearchResult> reverbSources =
        documentation.search("Reverb Ambience Modulate", "en", 5);
    bool supportedComparison = false;
    for (const DocumentationSearchResult &source : reverbSources) {
        if (source.entry.sourceKind == SourceKind::OfficialDocumentation
            && source.entry.body.contains("Ambience")
            && source.entry.body.contains("Modulate")) {
            supportedComparison = true;
            break;
        }
    }
    expect(supportedComparison,
           "Ambience/Modulate comparison claims have an official source");

    const struct HotfixCase {
        const char *prompt;
        QuestionIntent intent;
        const char *family;
        const char *parameter;
        const char *effectType;
        const char *value;
    } hotfixCases[] = {
        {"Oque é o pre delay?", QuestionIntent::ExplainParameter,
         "reverb", "preDelay", "", ""},
        {"oque o pre delay em 100ms faz?", QuestionIntent::ExplainParameter,
         "reverb", "preDelay", "", "100 ms"},
        {"pre-delay faz oque?", QuestionIntent::ExplainParameter,
         "reverb", "preDelay", "", ""},
        {"O que é Pre Delay?", QuestionIntent::ExplainParameter,
         "reverb", "preDelay", "", ""},
        {"Pra que serve o Pre Delay?", QuestionIntent::ExplainParameter,
         "reverb", "preDelay", "", ""},
        {"O que faz o Pre Delay do Reverb?", QuestionIntent::ExplainParameter,
         "reverb", "preDelay", "", ""},
        {"O que faz o Pre Delay do Chorus?", QuestionIntent::ExplainParameter,
         "chorus", "preDelay", "", ""},
        {"Esse Pre Delay de 100 ms no reverb faz sentido?",
         QuestionIntent::AssessCurrentSetting,
         "reverb", "preDelay", "", "100 ms"},
        {"Qual delay estou usando?", QuestionIntent::CurrentPatchFact,
         "delay", "", "", ""},
        {"O que é Warp Delay?", QuestionIntent::ExplainEffectType,
         "delay", "", "Warp", ""}
    };
    for (int i = 0; i < int(sizeof(hotfixCases) / sizeof(hotfixCases[0])); ++i) {
        const HotfixCase &test = hotfixCases[i];
        const QuestionRoutingResult routed = route(
            &router, QString::fromUtf8(test.prompt), context);
        const QuestionEntity *resolvedParameter = parameter(routed);
        QString resolvedFamily;
        QString resolvedType;
        QString matchedAlias;
        QString matchKind;
        if (resolvedParameter) {
            resolvedFamily = resolvedParameter->family;
            matchedAlias = resolvedParameter->matchedText;
            matchKind = resolvedParameter->matchKind;
        }
        for (const QuestionEntity &entity : routed.entities) {
            if (entity.kind == QuestionEntityKind::EffectType) {
                resolvedFamily = entity.family;
                resolvedType = entity.canonicalName;
                matchedAlias = entity.matchedText;
                matchKind = entity.matchKind;
                break;
            }
            if (resolvedFamily.isEmpty()
                && entity.kind == QuestionEntityKind::EffectFamily)
                resolvedFamily = entity.family;
        }
        QTextStream(stdout)
            << "TRACE " << (i + 1)
            << " detectedIntent=" << QuestionIntentRouter::intentName(routed.intent)
            << " resolvedFamily=" << resolvedFamily
            << " resolvedParameter="
            << (resolvedParameter ? resolvedParameter->id : QString())
            << " resolvedEffectType=" << resolvedType
            << " destination="
            << QuestionIntentRouter::destinationName(routed.destination)
            << " matchedAlias=" << matchedAlias
            << " matchKind=" << matchKind << '\n';
        expect(routed.intent == test.intent,
               QString("hotfix %1 intent").arg(i + 1));
        if (*test.family)
            expect(resolvedFamily == QLatin1String(test.family),
                   QString("hotfix %1 family").arg(i + 1));
        if (*test.parameter) {
            expect(resolvedParameter
                       && resolvedParameter->id == QLatin1String(test.parameter)
                       && resolvedParameter->family != QLatin1String("delay"),
                   QString("hotfix %1 compound parameter").arg(i + 1));
        }
        if (*test.effectType)
            expect(resolvedType == QLatin1String(test.effectType),
                   QString("hotfix %1 effect type").arg(i + 1));
        if (*test.value)
            expect(resolvedParameter
                       && resolvedParameter->userValue == QLatin1String(test.value),
                   QString("hotfix %1 normalized value").arg(i + 1));

        AIResponse hotfixResponse = ask(&provider,
            QString::fromUtf8(test.prompt), context);
        expect(hotfixResponse.metadata.value("detectedIntent")
                   == QuestionIntentRouter::intentName(test.intent),
               QString("hotfix %1 provider intent").arg(i + 1));
        expect(hotfixResponse.metadata.value("destination")
                   == QuestionIntentRouter::destinationName(routed.destination),
               QString("hotfix %1 provider destination").arg(i + 1));
        if (*test.parameter) {
            expect(hotfixResponse.metadata.value("resolvedFamily")
                       == QLatin1String(test.family)
                       && hotfixResponse.metadata.value("resolvedParameter")
                          == QLatin1String(test.parameter)
                       && hotfixResponse.metadata.value("matchedAlias")
                          == QLatin1String("pre delay")
                       && hotfixResponse.metadata.value("matchKind")
                          == QLatin1String("compound_alias"),
                   QString("hotfix %1 provider resolution trace").arg(i + 1));
        }
        if (*test.effectType) {
            expect(hotfixResponse.metadata.value("resolvedEffectType")
                       == QLatin1String(test.effectType)
                       && hotfixResponse.metadata.value("matchKind")
                          == QLatin1String("catalog_type"),
                   QString("hotfix %1 provider type trace").arg(i + 1));
        }
        if (i < 8) {
            expect(!hotfixResponse.text.contains(
                       QStringLiteral("patch atual usa Delay Dual L/R"),
                       Qt::CaseInsensitive),
                   QString("hotfix %1 no current Delay hijack").arg(i + 1));
            expect(!hotfixResponse.text.contains(
                       QStringLiteral("Warp Delay"), Qt::CaseInsensitive),
                   QString("hotfix %1 no Warp contamination").arg(i + 1));
        }
        if (i == 1)
            expect(hotfixResponse.text.contains("100 ms")
                       && hotfixResponse.text.contains(
                           QStringLiteral("efeito começa"),
                           Qt::CaseInsensitive),
                   "hotfix 2 value-aware explanation");
        if (i == 8)
            expect(hotfixResponse.text.contains("Delay Dual L/R"),
                   "hotfix 9 current Delay remains available");
        if (i == 9)
            expect(hotfixResponse.text.contains("Warp Delay"),
                   "hotfix 10 Warp explanation remains available");
    }

    const QList<DocumentationSearchResult> preDelaySources =
        documentation.search("reverb Pre Delay", "pt-BR", 3);
    expect(!preDelaySources.isEmpty()
               && preDelaySources.first().entry.id
                  == QLatin1String("owners_reverb_parameters")
               && !preDelaySources.first().entry.id.contains(
                      QStringLiteral("warp"), Qt::CaseInsensitive),
           "canonical Pre Delay documentation query cannot rank Warp first");

    QTextStream(stdout) << "Contextual Parameter Assessment tests: assertions="
                        << assertions << " failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
