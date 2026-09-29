#include "ai/LocalGT10Provider.h"
#include "ai/tone/ToneAdjustmentAdvisor.h"
#include "ai/tone/ToneAnalyzer.h"
#include "ai/tone/ToneFeatureExtractor.h"
#include "ai/tone/ToneIntentParser.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonObject>
#include <QTextStream>
#include <QTimer>

using namespace Ai;
using namespace Ai::Tone;

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

QJsonObject block(const QString &id, bool enabled, const QString &type)
{
    QJsonObject result;
    result.insert(QStringLiteral("id"), id);
    result.insert(QStringLiteral("enabled"), enabled);
    result.insert(QStringLiteral("type"), type);
    return result;
}

AIContext worshipContext()
{
    AIContext context;
    QJsonArray effects;

    QJsonObject preampA = block("preampA", true, "MATCH Drive");
    preampA.insert("gain", "46");
    preampA.insert("bass", "55");
    preampA.insert("middle", "62");
    preampA.insert("treble", "55");
    preampA.insert("presence", "52");
    effects.append(preampA);

    QJsonObject preampB = block("preampB", true, "VO Drive");
    preampB.insert("gain", "44");
    preampB.insert("bass", "50");
    preampB.insert("middle", "58");
    preampB.insert("treble", "54");
    preampB.insert("presence", "50");
    effects.append(preampB);

    QJsonObject od = block("odDs", false, "Blues OD");
    od.insert("drive", "85");
    effects.append(od);

    QJsonObject comp = block("comp", false, "Compressor");
    comp.insert("sustain", "90");
    effects.append(comp);

    QJsonObject eq = block("eq", true, "Parametric EQ");
    eq.insert("lowCut", "100 Hz");
    eq.insert("lowGain", "0 dB");
    eq.insert("highGain", "2 dB");
    effects.append(eq);

    QJsonObject delay = block("delay", true, "Dual L/R");
    QJsonObject delay1;
    delay1.insert("time", "1/8D");
    delay1.insert("feedback", "48");
    delay1.insert("level", "35");
    QJsonObject delay2;
    delay2.insert("time", "1/4");
    delay2.insert("feedback", "42");
    delay2.insert("level", "30");
    delay.insert("delay1", delay1);
    delay.insert("delay2", delay2);
    effects.append(delay);

    QJsonObject reverb = block("reverb", true, "Modulate");
    reverb.insert("reverbTime", "9.0 sec");
    reverb.insert("preDelay", "80 ms");
    reverb.insert("density", "8");
    reverb.insert("effectLevel", "40");
    effects.append(reverb);

    effects.append(block("chorus", false, "Stereo 1"));
    context.root.insert("effects", effects);

    QJsonObject routing;
    routing.insert("mode", "Dual L/R");
    routing.insert("channelSelect", "A");
    routing.insert("channelDelay", "4");
    context.root.insert("routing", routing);
    return context;
}

AIContext cleanDryContext()
{
    AIContext context = worshipContext();
    QJsonArray effects = context.root.value("effects").toArray();
    for (int index = 0; index < effects.size(); ++index) {
        QJsonObject item = effects.at(index).toObject();
        if (item.value("id").toString() == QLatin1String("preampA")
            || item.value("id").toString() == QLatin1String("preampB")) {
            item.insert("gain", "20");
        }
        if (item.value("id").toString() == QLatin1String("delay")
            || item.value("id").toString() == QLatin1String("reverb")
            || item.value("id").toString() == QLatin1String("chorus")) {
            item.insert("enabled", false);
        }
        effects.replace(index, item);
    }
    context.root.insert("effects", effects);
    context.root["routing"] = QJsonObject{{"mode", "Single"},
                                           {"channelDelay", "0"}};
    return context;
}

AIContext highGainContext()
{
    AIContext context = cleanDryContext();
    QJsonArray effects = context.root.value("effects").toArray();
    for (int index = 0; index < effects.size(); ++index) {
        QJsonObject item = effects.at(index).toObject();
        if (item.value("id").toString() == QLatin1String("preampA")) {
            item.insert("gain", "82");
            effects.replace(index, item);
        }
    }
    context.root.insert("effects", effects);
    return context;
}

bool hasFeature(const QList<ToneFeature> &features, const QString &id)
{
    for (const ToneFeature &feature : features) {
        if (feature.id == id)
            return true;
    }
    return false;
}

ToneAdjustmentIntent intent(ToneDirection direction)
{
    ToneAdjustmentIntent result;
    result.direction = direction;
    return result;
}

AIResponse providerRequest(LocalGT10Provider *provider, const QString &prompt,
                           const AIContext &context,
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
    request.context = context;
    provider->sendRequest(prompt, request);
    QTimer::singleShot(1000, &loop, &QEventLoop::quit);
    loop.exec();
    QObject::disconnect(connection);
    return result;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const AIContext current = worshipContext();
    const QJsonObject immutable = current.root;

    ToneFeatureExtractor extractor;
    const QList<ToneFeature> features = extractor.extract(current);
    expect(hasFeature(features, "crunch"), "01 mid-gain preamp derives crunch");
    expect(hasFeature(features, "wide"), "02 Dual L/R derives wide");
    expect(hasFeature(features, "ambient"), "03 delay/reverb derive ambient");
    expect(hasFeature(features, "deep"), "04 long tail/feedback derive deep");
    expect(hasFeature(features, "rhythmic"), "05 note delay derives rhythmic");
    expect(hasFeature(features, "defined"), "06 low cut/mids derive defined");
    expect(!hasFeature(features, "aggressive"),
           "07 disabled high-drive OD does not influence analysis");

    const QList<ToneFeature> cleanFeatures = extractor.extract(cleanDryContext());
    expect(hasFeature(cleanFeatures, "clean") && hasFeature(cleanFeatures, "dry"),
           "08 clean dry context is classified without inactive time effects");
    const QList<ToneFeature> highGainFeatures = extractor.extract(
        highGainContext());
    expect(hasFeature(highGainFeatures, "driven")
               && hasFeature(highGainFeatures, "aggressive"),
           "08b high-gain preamp derives driven/aggressive potential");

    ToneAnalyzer analyzer;
    const ToneAnalysis ptAnalysis = analyzer.analyze(current, "pt-BR");
    const ToneAnalysis enAnalysis = analyzer.analyze(current, "en");
    expect(ptAnalysis.summary.startsWith("Com base nos parâmetros atuais"),
           "09 pt-BR explanation is natural and evidence-based");
    expect(enAnalysis.summary.startsWith("Based on its current parameters"),
           "10 English explanation is natural and evidence-based");
    expect(!ptAnalysis.evidence.isEmpty() && !ptAnalysis.caveats.isEmpty(),
           "11 analysis carries evidence and scope caveat");

    ToneIntentParser parser;
    expect(parser.parse("Me explique esse preset").kind
               == ToneRequestKind::ExplainTone,
           "12 explain intent pt-BR");
    expect(parser.parse("What does this patch sound like?").kind
               == ToneRequestKind::ExplainTone,
           "13 explain intent English");
    expect(parser.parse("Por que meu reverb soa profundo?").kind
               == ToneRequestKind::ExplainTone,
           "14 explanatory why intent");
    expect(parser.parse("Quero mais crunch").adjustment.direction
               == ToneDirection::MoreCrunch,
           "15 more crunch intent");
    expect(parser.parse("deixe mais ambiência").adjustment.direction
               == ToneDirection::MoreAmbient,
           "16 accent-insensitive ambience intent");
    expect(parser.parse("I need more definition").adjustment.direction
               == ToneDirection::MoreDefined,
           "17 definition intent English");
    expect(parser.parse("menos embolado").adjustment.direction
               == ToneDirection::LessMuddy,
           "18 less muddy intent");
    expect(parser.parse("mais brilhante").adjustment.direction
               == ToneDirection::MoreBright,
           "19 bright intent");
    expect(parser.parse("more dynamic").adjustment.direction
               == ToneDirection::MoreDynamic,
           "20 dynamic intent");
    expect(parser.parse("Qual reverb estou usando?").kind == ToneRequestKind::None,
           "21 factual request remains in knowledge route");
    expect(parser.parse("Quero Neural Capture e Shimmer Cloud").kind
               == ToneRequestKind::UnsupportedCapability,
           "22 unsupported products are rejected by tone parser");
    expect(parser.parse("Quero soar como Mateus Asato").kind
               == ToneRequestKind::ArtistProfileUnavailable,
           "23 unavailable artist profile is explicit");

    ToneAdjustmentAdvisor advisor;
    const ToneAdvice crunch = advisor.advise(
        current, intent(ToneDirection::MoreCrunch), "pt-BR");
    expect(!crunch.suggestions.isEmpty()
               && crunch.suggestions.first().blockId == QLatin1String("preampA")
               && crunch.suggestions.first().parameterId == QLatin1String("gain"),
           "24 crunch starts with active preamp Gain");
    expect(crunch.suggestions.size() <= 2
               && crunch.suggestions.last().optionalActivation,
           "25 inactive OD is explicit optional second step only");

    const ToneAdvice ambient = advisor.advise(
        current, intent(ToneDirection::MoreAmbient), "pt-BR");
    expect(!ambient.suggestions.isEmpty()
               && ambient.suggestions.first().blockId == QLatin1String("reverb")
               && ambient.suggestions.first().parameterId
                      == QLatin1String("effectLevel"),
           "26 long reverb favors level over extending an already long tail");
    const ToneAdvice defined = advisor.advise(
        current, intent(ToneDirection::LessMuddy), "pt-BR");
    expect(!defined.suggestions.isEmpty()
               && defined.suggestions.first().suggestedDirection
                      == QLatin1String("decrease"),
           "27 less muddy begins with a reduction");
    expect(!advisor.advise(current, intent(ToneDirection::MoreBright), "en")
                .suggestions.isEmpty(),
           "28 bright guidance exists in English");
    expect(!advisor.advise(current, intent(ToneDirection::MoreWide), "pt-BR")
                .suggestions.isEmpty(),
           "29 width guidance respects current routing");
    expect(!advisor.advise(current, intent(ToneDirection::MoreFocused), "pt-BR")
                .suggestions.isEmpty(),
           "30 focused guidance uses current routing/ambience");
    expect(current.root == immutable,
           "31 extraction, analysis and advice never mutate AIContext");

    AIContext limited = current;
    QJsonArray limitedEffects = limited.root.value("effects").toArray();
    for (int index = 0; index < limitedEffects.size(); ++index) {
        QJsonObject item = limitedEffects.at(index).toObject();
        if (item.value("id").toString() == QLatin1String("reverb")) {
            item.insert("reverbTime", "10.0 sec");
            item.insert("effectLevel", "100");
            limitedEffects.replace(index, item);
        }
    }
    limited.root.insert("effects", limitedEffects);
    const ToneAdvice atLimit = advisor.advise(
        limited, intent(ToneDirection::MoreAmbient), "pt-BR");
    expect(atLimit.suggestions.isEmpty()
               || atLimit.suggestions.first().parameterId
                      != QLatin1String("reverbTime"),
           "32 advisor does not exceed a known parameter limit");

    LocalGT10Provider provider;
    const AIResponse explain = providerRequest(
        &provider, "Me explique esse preset", current);
    expect(explain.metadata.value("route") == QLatin1String("tone_explain")
               && explain.metadata.value("answered").toBool(),
           "33 provider routes tone explanation locally");
    expect(!explain.metadata.value("toneFeatures").toArray().isEmpty(),
           "34 provider exposes structured tone evidence");
    const QJsonArray explainSources = explain.metadata.value("toneFeatures")
        .toArray().first().toObject().value("sources").toArray();
    expect(explainSources.contains(QStringLiteral("PatchContext"))
               && explainSources.size() >= 2,
           "34b explanation traces patch data and its rule/document source");
    const AIResponse adjust = providerRequest(
        &provider, "Quero mais crunch", current);
    expect(adjust.metadata.value("route") == QLatin1String("tone_adjustment")
               && !adjust.metadata.value("toneSuggestions").toArray().isEmpty(),
           "35 provider routes adjustment guidance locally");
    const AIResponse factual = providerRequest(
        &provider, "Qual delay estou usando?", current);
    expect(factual.metadata.value("route") == QLatin1String("knowledge")
               && factual.text.contains("Dual L/R"),
           "36 factual question remains delegated to knowledge engine");
    const AIResponse artist = providerRequest(
        &provider, "Quero soar como Mateus Asato", current);
    expect(artist.metadata.value("route") == QLatin1String("tone_limitation")
               && !artist.metadata.value("answered").toBool(),
           "37 unavailable artist profile is not hallucinated");
    expect(current.root == immutable,
           "38 provider path remains read-only");

    const QStringList validationPrompts = {
        QStringLiteral("Me explique esse preset"),
        QStringLiteral("Quero deixar esse preset um pouco mais crunch"),
        QStringLiteral("Quero mais profundidade para ambiência"),
        QStringLiteral("Quero mais definição"),
        QStringLiteral("Quero menos embolado"),
        QStringLiteral("Quero mais brilhante"),
        QStringLiteral("Quero mais escuro"),
        QStringLiteral("Quero mais aberto")
    };
    for (const QString &prompt : validationPrompts) {
        const AIResponse response = providerRequest(&provider, prompt, current);
        expect(response.metadata.value("answered").toBool()
                   && (response.metadata.value("route")
                           == QLatin1String("tone_explain")
                       || response.metadata.value("route")
                           == QLatin1String("tone_adjustment")),
               QStringLiteral("manual-validation prompt: ") + prompt);
    }
    for (const QString &prompt : {QStringLiteral("deixe mais shimmer"),
                                  QStringLiteral("adicione cloud reverb"),
                                  QStringLiteral("faça Neural Capture")}) {
        const AIResponse response = providerRequest(&provider, prompt, current);
        expect(!response.metadata.value("answered").toBool()
                   && response.metadata.value("route")
                       == QLatin1String("tone_limitation"),
               QStringLiteral("anti-hallucination prompt: ") + prompt);
    }

    QTextStream(stdout) << "Tone Reasoning tests: assertions=" << assertions
                        << " failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
