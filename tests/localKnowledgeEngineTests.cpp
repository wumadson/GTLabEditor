#include "ai/knowledge/LocalKnowledgeEngine.h"
#include "ai/LocalGT10Provider.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonObject>
#include <QTextStream>
#include <QTimer>

using namespace Ai;
using namespace Ai::Knowledge;

namespace {
int failures = 0;
int assertions = 0;
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
    QJsonObject patch;
    patch.insert("number", "U29-1");
    patch.insert("name", "WC WORSP CLEAN");
    value.root.insert("patch", patch);
    QJsonObject delay;
    delay.insert("id", "delay");
    delay.insert("type", "Dual L/R");
    QJsonArray effects;
    effects.append(delay);
    value.root.insert("effects", effects);
    QJsonObject routing;
    routing.insert("mode", "Dual L/R");
    value.root.insert("routing", routing);
    QJsonObject chain;
    QJsonArray pathA;
    QJsonObject amp;
    amp.insert("name", "PREAMP A");
    pathA.append(amp);
    chain.insert("pathA", pathA);
    chain.insert("pathB", QJsonArray());
    chain.insert("commonPrefix", QJsonArray());
    chain.insert("commonSuffix", QJsonArray());
    value.root.insert("signalChain", chain);
    return value;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    LocalKnowledgeEngine engine;
    const AIContext current = context();

    expect(engine.answer("A GT-10 possui Dual Delay?", "pt-BR").answered,
           "27 capability pt-BR");
    expect(engine.answer("Does the GT-10 have Dual Delay?", "en").answered,
           "28 capability en");
    expect(engine.answer("Como configurar um Assign?", "pt-BR").text.contains("Target Min"),
           "29 assign pt-BR");
    expect(engine.answer("Como usar o pedal de expressão?", "pt-BR").answered,
           "30 expression pt-BR");
    expect(engine.answer("Como funciona o routing?", "pt-BR", &current).text.contains("Dual L/R"),
           "31 routing question");
    expect(engine.answer("Qual é a cadeia de sinal?", "pt-BR", &current).text.contains("PREAMP A"),
           "32 signal chain question");
    expect(engine.answer("Qual delay estou usando?", "pt-BR", &current).text.contains("Dual L/R"),
           "33 patch-aware answer");
    expect(!engine.answer("conte-me uma história", "pt-BR").answered,
           "34 unknown intent");
    expect(engine.answer("conte-me uma história", "pt-BR").text.contains("informação suficiente"),
           "35 fallback pt-BR");
    expect(engine.answer("tell me a story", "en").text.contains("enough information"),
           "36 fallback en");

    expect(!engine.answer("A GT-10 possui XYZ123?", "pt-BR").answered,
           "37 nonexistent effect is unknown");
    expect(!engine.answer("A GT-10 tem Neural Capture?", "pt-BR").answered,
           "38 Neural Capture is not invented");
    expect(!engine.answer("Posso usar 17 reverbs simultâneos?", "pt-BR").answered,
           "39 unsupported count is not invented");
    expect(!engine.answer("FX1 possui XYZ123?", "pt-BR").answered,
           "40 nonexistent FX algorithm is unknown");

    AIContext privateContext = current;
    privateContext.root.insert("fileName", "/Users/private/secret.syx");
    privateContext.root.insert("midiPort", "Private MIDI Port");
    const QJsonObject before = privateContext.root;
    const QString response = engine.answer("Qual patch está carregado?", "pt-BR",
                                           &privateContext).text;
    expect(!response.contains("/Users/") && !response.contains("Private MIDI"),
           "41 privacy fields are not exposed");
    expect(privateContext.root == before, "42 context remains immutable");
    expect(engine.answer("Ela tem Harmonist?", "pt-BR").answered,
           "43 unqualified effect question");
    expect(engine.answer("Quais parâmetros existem no Modulate Reverb?", "pt-BR")
               .text.contains("Reverb Time"),
           "44 effect parameter question");
    expect(engine.answer("O que é Target?", "pt-BR").text.contains("parâmetro real"),
           "45 Target explanation");
    expect(engine.answer("O que é Source Mode?", "pt-BR").text.contains("Moment"),
           "46 Source Mode explanation");
    expect(engine.answer("Assign é Patch ou System?", "pt-BR").text.contains("patch"),
           "47 Assign scope explanation");

    LocalGT10Provider provider;
    const AIProviderCapabilities providerCapabilities = provider.capabilities();
    expect(provider.providerId() == "local-gt10"
           && providerCapabilities.supportsConversation
           && !providerCapabilities.supportsStructuredRecommendations
           && !providerCapabilities.supportsStreaming
           && !providerCapabilities.requiresNetwork
           && !providerCapabilities.requiresAuthentication,
           "48 local provider identity and offline capabilities");
    bool asynchronous = false;
    bool delivered = false;
    QString providerResponse;
    QObject::connect(&provider, &AIProvider::responseReady,
                     [&](QString requestId, AIResponse response) {
        if (requestId == "local-provider") {
            delivered = true;
            providerResponse = response.text;
        }
    });
    AIRequest request;
    request.userMessage = "Quantos Assigns a GT-10 tem?";
    request.language = "pt-BR";
    provider.sendRequest("local-provider", request);
    asynchronous = !delivered;
    QEventLoop waitLoop;
    QTimer::singleShot(500, &waitLoop, &QEventLoop::quit);
    QObject::connect(&provider, &AIProvider::responseReady,
                     &waitLoop, &QEventLoop::quit);
    waitLoop.exec();
    expect(asynchronous && delivered && providerResponse.contains("8 Assigns"),
           "49 local provider delegates asynchronously to knowledge engine");
    expect(engine.answer("O que é EXP SW?", "pt-BR").text.contains("controle separado"),
           "50 EXP SW guidance");
    expect(engine.answer("CTL1 é Patch ou System?", "pt-BR")
               .text.contains("patch", Qt::CaseInsensitive),
           "51 CTL1 scope");
    expect(engine.answer("A GT-10 tem Dual Delay?", "pt-BR").answered,
           "52 Dual Delay capability");
    expect(engine.answer("Quais reverbs existem?", "pt-BR").text.contains("Spring"),
           "53 Reverb catalog");
    expect(engine.answer("Como controlar Tremolo Rate com EXP?", "pt-BR")
               .text.contains("Target"),
           "54 verified Tremolo Rate target guidance");
    expect(!engine.answer("O manual fala que a GT-10 tem Neural Capture?", "pt-BR").answered
           && engine.answer("O manual fala que a GT-10 tem Neural Capture?", "pt-BR")
                  .text.contains("documentação local"),
           "55 absent official documentation cannot prove Neural Capture");
    expect(!engine.answer("Como configuro Wi-Fi na GT-10?", "pt-BR").answered,
           "56 undocumented Wi-Fi is not invented");
    expect(engine.answer("Pra que serve Assign?", "pt-BR")
               .text.contains("oito Assigns"),
           "57 official Assign purpose");
    expect(engine.answer("Como configurar um Assign?", "pt-BR")
               .text.contains("Target Min"),
           "58 official Assign procedure");
    expect(engine.answer("O que significa Target Min?", "pt-BR")
               .text.contains("extremos"),
           "59 official Target Min meaning");
    expect(engine.answer("Como usar EXP SW?", "pt-BR")
               .text.contains("controle separado"),
           "60 official EXP SW usage");
    expect(engine.answer("Como calibrar EXP?", "pt-BR")
               .text.contains("PDL CALIBRATION"),
           "61 official EXP calibration");
    expect(engine.answer("O que faz Density?", "pt-BR")
               .text.contains("densidade"),
           "62 official Reverb Density meaning");
    expect(engine.answer("O que é Warp Delay?", "pt-BR")
               .text.contains("Warp Delay"),
           "63 official Warp Delay meaning");
    expect(engine.answer("O que é Dual L/R?", "pt-BR").answered,
           "64 official Dual L/R routing");
    expect(!engine.answer("A GT-10 tem Neural Capture?", "pt-BR").answered,
           "65 Neural Capture remains unknown");
    expect(!engine.answer("Como conectar Wi-Fi?", "pt-BR").answered,
           "66 Wi-Fi remains unknown");

    QTextStream(stdout) << "Local Knowledge Engine tests: assertions="
                        << assertions << " failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
