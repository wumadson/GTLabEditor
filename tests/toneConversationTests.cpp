#include "ai/LocalGT10Provider.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonObject>
#include <QTextStream>
#include <QTimer>

using namespace Ai;

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
    result.insert("id", id);
    result.insert("enabled", enabled);
    result.insert("type", type);
    return result;
}

AIContext context(const QString &number = QStringLiteral("U29-1"),
                  const QString &name = QStringLiteral("WC ALL-IN-ONE"))
{
    AIContext result;
    result.root.insert("patch", QJsonObject{{"number", number},
                                             {"name", name}});
    QJsonArray effects;
    QJsonObject ampA = block("preampA", true, "MATCH Drive");
    ampA.insert("gain", "46");
    ampA.insert("bass", "55");
    ampA.insert("middle", "62");
    ampA.insert("treble", "55");
    ampA.insert("presence", "52");
    effects.append(ampA);
    QJsonObject ampB = block("preampB", true, "VO Drive");
    ampB.insert("gain", "44");
    ampB.insert("bass", "50");
    ampB.insert("middle", "58");
    ampB.insert("treble", "54");
    ampB.insert("presence", "50");
    effects.append(ampB);
    QJsonObject od = block("odDs", false, "Blues OD");
    od.insert("drive", "25");
    effects.append(od);
    QJsonObject eq = block("eq", true, "Parametric EQ");
    eq.insert("lowCut", "100 Hz");
    eq.insert("lowGain", "0 dB");
    eq.insert("highGain", "2 dB");
    effects.append(eq);
    QJsonObject delay = block("delay", true, "Dual L/R");
    delay.insert("feedback", "48");
    delay.insert("effectLevel", "35");
    delay.insert("delay1", QJsonObject{{"time", "1/8D"},
                                        {"feedback", "48"},
                                        {"level", "35"}});
    delay.insert("delay2", QJsonObject{{"time", "1/4"},
                                        {"feedback", "42"},
                                        {"level", "30"}});
    effects.append(delay);
    QJsonObject reverb = block("reverb", true, "Modulate");
    reverb.insert("reverbTime", "9.0 sec");
    reverb.insert("preDelay", "80 ms");
    reverb.insert("density", "8");
    reverb.insert("effectLevel", "40");
    effects.append(reverb);
    effects.append(block("chorus", false, "Stereo 1"));
    effects.append(block("comp", false, "Compressor"));
    result.root.insert("effects", effects);
    result.root.insert("routing", QJsonObject{{"mode", "Dual L/R"},
                                               {"channelSelect", "A"},
                                               {"channelDelay", "4"}});
    return result;
}

AIResponse request(LocalGT10Provider *provider, const QString &prompt,
                   const AIContext &patch,
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
    AIRequest input;
    input.userMessage = prompt;
    input.language = language;
    input.context = patch;
    provider->sendRequest(QString::number(assertions + 1), input);
    QTimer::singleShot(1000, &loop, &QEventLoop::quit);
    loop.exec();
    QObject::disconnect(connection);
    return result;
}

bool mode(const AIResponse &response, const QString &expected)
{
    return response.metadata.value("responseMode").toString() == expected;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const AIContext patchA = context();
    const QJsonObject immutable = patchA.root;
    LocalGT10Provider provider;

    AIResponse explain = request(&provider, "Me explique esse preset", patchA);
    expect(mode(explain, "EXPLAIN") && explain.text.count("\n\n") >= 2,
           "01 explain uses multiple short paragraphs");
    expect(explain.text.contains("Dual L/R")
               && explain.text.contains("MATCH Drive")
               && explain.text.contains("Delay")
               && explain.text.contains("Reverb"),
           "02 explain reasons across routing, amps and ambience");
    AIResponse explainDetail = request(&provider, "Explique melhor", patchA);
    expect(mode(explainDetail, "EXPLAIN")
               && explainDetail.text.size() > explain.text.size(),
           "02b detail follow-up expands the previous tone analysis");

    AIResponse crunch = request(&provider, "Quero mais crunch", patchA);
    expect(mode(crunch, "RECOMMEND")
               && crunch.text.contains("trade-off", Qt::CaseInsensitive),
           "03 recommendation includes a trade-off");
    expect(crunch.text.contains("Gain") && crunch.text.contains("46"),
           "04 recommendation combines parameter and current value");

    AIResponse why = request(&provider,
                             "Por que você está sugerindo isso?", patchA);
    expect(mode(why, "JUSTIFY") && why.metadata.value("answered").toBool(),
           "05 why follow-up resolves to JUSTIFY, never Unknown");
    expect(why.text.contains("Gain") && why.text.contains("46")
               && why.text.contains("faixa"),
           "06 justification uses the last structured recommendation");

    AIResponse clean = request(&provider,
                               "Mas quero preservar o clean", patchA);
    expect(mode(clean, "REFINE")
               && !clean.metadata.value("toneConstraints").toArray().isEmpty(),
           "07 refine records preserve-clean constraint");
    expect(clean.text.contains("clean", Qt::CaseInsensitive),
           "08 refined trade-off reflects clean constraint");

    AIResponse avoidDrive = request(&provider,
                                    "Prefiro não usar OD/DS", patchA);
    bool hasOd = false;
    for (const QJsonValue &value
         : avoidDrive.metadata.value("toneSuggestions").toArray()) {
        if (value.toObject().value("blockId") == "odDs")
            hasOd = true;
    }
    expect(mode(avoidDrive, "REFINE") && !hasOd,
           "09 avoid-OD constraint removes OD/DS alternative");

    AIResponse ampOrDrive = request(
        &provider, "É melhor mexer no amp ou no Blues OD?", patchA);
    expect(mode(ampOrDrive, "COMPARE")
               && ampOrDrive.text.contains("amp", Qt::CaseInsensitive)
               && ampOrDrive.text.contains("Blues OD"),
           "10 compare discusses both amp and drive in current state");
    AIResponse timeOrLevel = request(
        &provider, "Reverb Time ou Effect Level?", patchA);
    expect(mode(timeOrLevel, "COMPARE")
               && timeOrLevel.text.contains("Reverb Time")
               && timeOrLevel.text.contains("Effect Level"),
           "11 compare distinguishes reverb duration and balance");
    AIResponse trebleOrPresence = request(
        &provider, "Treble ou Presence?", patchA);
    expect(mode(trebleOrPresence, "COMPARE")
               && trebleOrPresence.text.contains("Treble")
               && trebleOrPresence.text.contains("Presence"),
           "12 compare distinguishes Treble and Presence");

    AIResponse deeper = request(
        &provider, "Quero mais profundidade para ambiência", patchA);
    expect(mode(deeper, "RECOMMEND")
               && deeper.text.contains("Reverb", Qt::CaseInsensitive),
           "13 ambient/deep recommendation uses active ambience");
    AIResponse preserveDefinition = request(
        &provider, "Mas não quero perder definição", patchA);
    expect(mode(preserveDefinition, "REFINE")
               && preserveDefinition.text.contains("definição"),
           "14 ambience refinement preserves definition");

    AIResponse more = request(&provider, "Mais um pouco", patchA);
    expect(mode(more, "REFINE")
               && more.text.contains("mais um pouco", Qt::CaseInsensitive),
           "15 positive feedback advances the last adjustment");
    AIResponse tooMuch = request(&provider, "Ficou demais", patchA);
    expect(mode(tooMuch, "REFINE")
               && tooMuch.text.contains("recuaria", Qt::CaseInsensitive),
           "16 excessive feedback backs off the last adjustment");
    AIResponse detail = request(&provider, "Explique melhor", patchA);
    expect(mode(detail, "JUSTIFY") && detail.text.size() > why.text.size() / 2,
           "17 detail follow-up expands the previous rationale");

    AIResponse factual = request(&provider, "Qual reverb estou usando?", patchA);
    expect(factual.metadata.value("route") == "knowledge"
               && !factual.metadata.contains("responseMode"),
           "18 factual question remains on knowledge route");

    for (const QString &prompt : {QStringLiteral("deixe mais shimmer"),
                                  QStringLiteral("adicione cloud reverb"),
                                  QStringLiteral("faça Neural Capture")}) {
        AIResponse unsupported = request(&provider, prompt, patchA);
        expect(unsupported.metadata.value("route") == "tone_limitation"
                   && !unsupported.metadata.value("answered").toBool(),
               QStringLiteral("anti-hallucination: ") + prompt);
    }

    provider.resetConversation();
    AIResponse resetWhy = request(&provider, "Por quê?", patchA);
    expect(mode(resetWhy, "CLARIFY") && !resetWhy.text.contains("Gain 46"),
           "22 new conversation clears recommendation state");

    request(&provider, "Quero mais crunch", patchA);
    const AIContext patchB = context("U30-1", "SECOND PATCH");
    AIResponse changedWhy = request(&provider, "Por que?", patchB);
    expect(mode(changedWhy, "CLARIFY")
               && changedWhy.text.contains("patch mudou", Qt::CaseInsensitive)
               && !changedWhy.text.contains("Gain 46"),
           "23 patch identity change invalidates prior recommendation");

    provider.resetConversation();
    AIResponse enCrunch = request(&provider, "I want more crunch", patchA, "en");
    AIResponse enWhy = request(&provider, "Why did you suggest that?", patchA,
                               "en");
    expect(mode(enCrunch, "RECOMMEND") && mode(enWhy, "JUSTIFY")
               && enWhy.text.contains("current value"),
           "24 English conversation follows the same structured logic");

    expect(patchA.root == immutable,
           "25 complete conversation remains read-only");
    expect(!explain.text.contains("{{") && !crunch.text.contains("{{"),
           "26 responses contain no internal placeholders");
    expect(explain.text.left(40) != crunch.text.left(40)
               && crunch.text.left(40) != why.text.left(40),
           "27 deterministic modes avoid repetitive openings");

    QTextStream(stdout) << "Tone Conversation tests: assertions="
                        << assertions << " failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
