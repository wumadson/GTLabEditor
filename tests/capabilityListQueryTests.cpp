#include "ai/LocalGT10Provider.h"
#include "ai/knowledge/GT10CapabilityService.h"
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

AIContext context()
{
    AIContext value;
    QJsonArray effects;
    effects.append(QJsonObject{{"id", "delay"}, {"enabled", true},
                                {"type", "Dual L/R"}});
    effects.append(QJsonObject{{"id", "reverb"}, {"enabled", true},
                                {"type", "Modulate"}});
    effects.append(QJsonObject{{"id", "preampA"}, {"enabled", true},
                                {"type", "MATCH Drive"}});
    effects.append(QJsonObject{{"id", "preampB"}, {"enabled", true},
                                {"type", "VO Drive"}});
    value.root.insert("effects", effects);
    value.root.insert("routing", QJsonObject{{"channelSelect", "Channel A"}});
    return value;
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

QString family(const QuestionRoutingResult &result)
{
    for (const QuestionEntity &entity : result.entities) {
        if (entity.kind == QuestionEntityKind::EffectFamily)
            return entity.family;
    }
    return {};
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QuestionIntentRouter router;
    GT10CapabilityService capabilities;

    struct Case { const char *prompt; const char *family; const char *language; };
    const Case corpus[] = {
        {"Quais Delays tem na GT-10?", "delay", "pt-BR"},
        {"Quais tipos de Delay existem?", "delay", "pt-BR"},
        {"Que reverbs a GT-10 possui?", "reverb", "pt-BR"},
        {"Quais Chorus existem?", "chorus", "pt-BR"},
        {"Quais tipos de compressor existem?", "comp", "pt-BR"},
        {"Quais drives existem?", "odDs", "pt-BR"},
        {"Quais amps tem na GT-10?", "preamp", "pt-BR"},
        {"Quais efeitos tem no FX1?", "fx1", "pt-BR"},
        {"Quais efeitos tem no FX2?", "fx2", "pt-BR"},
        {"What delays does the GT-10 have?", "delay", "en"},
        {"Which reverb types are available?", "reverb", "en"},
        {"What chorus modes are available?", "chorus", "en"},
        {"What compressor types are available?", "comp", "en"},
        {"What OD/DS models are available?", "odDs", "en"},
        {"What amps does the GT-10 have?", "preamp", "en"},
        {"What effects are available in FX1?", "fx1", "en"},
        {"What effects are available in FX2?", "fx2", "en"}
    };
    for (int i = 0; i < int(sizeof(corpus) / sizeof(corpus[0])); ++i) {
        const QuestionRoutingResult routed = router.route(
            QString::fromUtf8(corpus[i].prompt));
        expect(routed.intent == QuestionIntent::ListEffectTypes
                   && routed.destination == QuestionDestination::Knowledge
                   && family(routed) == QLatin1String(corpus[i].family),
               QString("corpus %1 list routing").arg(i + 1));
        QTextStream(stdout)
            << "TRACE " << i + 1
            << " detectedIntent=" << QuestionIntentRouter::intentName(routed.intent)
            << " resolvedFamily=" << family(routed)
            << " destination=GT10CapabilityService"
            << " listCount="
            << capabilities.effectTypesForBlock(
                   family(routed) == QLatin1String("preamp")
                       ? QStringLiteral("preampA") : family(routed)).size()
            << " sourceKind=StructuralTruth\n";
    }

    LocalGT10Provider provider;
    const AIResponse delays = ask(&provider, "Quais Delays tem na GT-10?");
    expect(delays.metadata.value("detectedIntent")
               == QLatin1String("LIST_EFFECT_TYPES")
               && delays.metadata.value("resolvedFamily") == QLatin1String("delay")
               && delays.metadata.value("destinationService")
                      == QLatin1String("GT10CapabilityService")
               && delays.metadata.value("sourceKindName")
                      == QLatin1String("StructuralTruth"),
           "provider exposes structural list trace");
    expect(delays.metadata.value("listCount").toInt()
               == capabilities.effectTypesForBlock("delay").size()
               && delays.text.contains("Single")
               && delays.text.contains("Dual Parallel, Dual L/R, Reverse")
               && delays.text.contains("Warp")
               && delays.text.indexOf("tipos/modelos")
                      < delays.text.indexOf("patch atual"),
           "Delay list comes from catalog before optional patch context");
    expect(!delays.text.contains("controla em conjunto")
               && !delays.text.startsWith("O patch atual usa"),
           "Delay list is not contaminated by Warp documentation or patch fact");
    const QStringList expectedDelayTypes = {
        "SINGLE", "PAN", "STEREO", "DUALSERIES", "DUALPARALLEL", "DUALL/R",
        "REVERSE", "ANALOG", "TAPE", "WARP", "MODULATE"
    };
    const QStringList actualDelayTypes = capabilities.effectTypesForBlock("delay");
    bool exactDelayCatalog = actualDelayTypes.size() == expectedDelayTypes.size();
    for (int i = 0; exactDelayCatalog && i < expectedDelayTypes.size(); ++i) {
        exactDelayCatalog = capabilities.normalize(actualDelayTypes.at(i))
            == capabilities.normalize(expectedDelayTypes.at(i));
    }
    expect(exactDelayCatalog, "Delay list exactly reflects MidiTable order");

    const AIResponse reverbs = ask(&provider, "Quais reverbs tem na GT-10?");
    expect(reverbs.metadata.value("listCount").toInt()
               == capabilities.effectTypesForBlock("reverb").size()
               && reverbs.text.contains("Ambience")
               && reverbs.text.contains("Spring")
               && reverbs.text.contains("Modulate"),
           "Reverb list comes from catalog");
    const QStringList expectedReverbTypes = {
        "AMBIENCE", "ROOM", "HALL1", "HALL2", "PLATE", "SPRING", "MOD"
    };
    const QStringList actualReverbTypes = capabilities.effectTypesForBlock("reverb");
    bool exactReverbCatalog = actualReverbTypes.size() == expectedReverbTypes.size();
    for (int i = 0; exactReverbCatalog && i < expectedReverbTypes.size(); ++i) {
        exactReverbCatalog = capabilities.normalize(actualReverbTypes.at(i))
            == capabilities.normalize(expectedReverbTypes.at(i));
    }
    expect(exactReverbCatalog, "Reverb list exactly reflects MidiTable order");

    for (const QString &name : {QStringLiteral("chorus"), QStringLiteral("comp"),
                                QStringLiteral("odDs"), QStringLiteral("preampA"),
                                QStringLiteral("fx1"), QStringLiteral("fx2")}) {
        expect(!capabilities.effectTypesForBlock(name).isEmpty(),
               name + QStringLiteral(" exposes real catalog types"));
    }

    const QuestionRoutingResult current = router.route("Qual delay estou usando?");
    expect(current.intent == QuestionIntent::CurrentPatchFact,
           "singular current Delay remains CURRENT_PATCH_FACT");
    const AIResponse currentAmp = ask(&provider, "Qual amp estou usando?");
    expect(currentAmp.metadata.value("detectedIntent")
               == QLatin1String("CURRENT_PATCH_FACT")
               && currentAmp.text.contains("MATCH Drive")
               && currentAmp.text.contains("PREAMP A"),
           "current amp remains a selected patch fact");
    const QuestionRoutingResult warp = router.route("O que é Warp Delay?");
    expect(warp.intent == QuestionIntent::ExplainEffectType,
           "Warp definition remains EXPLAIN_EFFECT_TYPE");
    const QuestionRoutingResult compare = router.route(
        "Qual a diferença entre Warp e Reverse?");
    expect(compare.intent == QuestionIntent::CompareEffectTypes,
           "Warp/Reverse remains COMPARE_EFFECT_TYPES");

    const AIResponse unknown = ask(
        &provider, "Quais SuperClouds tem na GT-10?");
    expect(unknown.metadata.value("detectedIntent")
               == QLatin1String("LIST_EFFECT_TYPES")
               && !unknown.metadata.value("answered").toBool()
               && unknown.metadata.value("listCount").toInt() == 0,
           "unknown family is not invented");

    QTextStream(stdout) << "Capability List Query tests: assertions="
                        << assertions << " failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
