#include "ai/MockAIProvider.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonObject>
#include <QTextStream>
#include <QTimer>
#include <QTranslator>

using namespace Ai;

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

void wait(int milliseconds = 550)
{
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}

AIContext context(const QString &number = "U29-1",
                  const QString &name = "WC ALL-IN-ONE")
{
    AIContext result;
    QJsonObject patch;
    patch.insert("number", number);
    patch.insert("name", name);
    result.root.insert("patch", patch);

    QJsonArray effects;
    QJsonObject preampA;
    preampA.insert("id", "preampA");
    preampA.insert("type", "MATCH Drive");
    effects.append(preampA);
    QJsonObject preampB;
    preampB.insert("id", "preampB");
    preampB.insert("type", "VO Drive");
    effects.append(preampB);
    QJsonObject delay;
    delay.insert("id", "delay");
    delay.insert("type", "Dual L/R");
    QJsonObject delay1;
    delay1.insert("time", "dotted eighth note");
    delay1.insert("feedback", "45");
    delay.insert("delay1", delay1);
    effects.append(delay);
    QJsonObject reverb;
    reverb.insert("id", "reverb");
    reverb.insert("type", "Modulate");
    reverb.insert("reverbTime", "9.0 sec");
    reverb.insert("effectLevel", "40");
    effects.append(reverb);
    result.root.insert("effects", effects);

    QJsonObject routing;
    routing.insert("mode", "Dual L/R");
    result.root.insert("routing", routing);
    QJsonObject chain;
    QJsonArray prefix;
    QJsonObject comp;
    comp.insert("name", "COMP");
    prefix.append(comp);
    chain.insert("commonPrefix", prefix);
    QJsonArray pathA;
    QJsonObject ampA;
    ampA.insert("name", "PREAMP A");
    pathA.append(ampA);
    chain.insert("pathA", pathA);
    QJsonArray pathB;
    QJsonObject ampB;
    ampB.insert("name", "PREAMP B");
    pathB.append(ampB);
    chain.insert("pathB", pathB);
    chain.insert("commonSuffix", QJsonArray());
    result.root.insert("signalChain", chain);
    return result;
}

QString request(MockAIProvider *provider, const QString &prompt,
                const AIContext &aiContext, bool *async = nullptr,
                const QString &language = QStringLiteral("en"))
{
    QString responseText;
    bool delivered = false;
    const QMetaObject::Connection connection = QObject::connect(
        provider, &AIProvider::responseReady,
        [&](QString, AIResponse response) {
        delivered = true;
        responseText = response.text;
    });
    AIRequest request;
    request.userMessage = prompt;
    request.language = language;
    request.context = aiContext;
    provider->sendRequest(prompt, request);
    if (async)
        *async = !delivered;
    wait();
    QObject::disconnect(connection);
    return responseText;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    MockAIProvider provider;
    const AIProviderCapabilities capabilities = provider.capabilities();
    expect(provider.providerId() == "local-test"
           && provider.providerName() == "Local Test",
           "1 provider identity");
    expect(capabilities.supportsConversation
           && !capabilities.supportsStructuredRecommendations
           && !capabilities.supportsStreaming
           && !capabilities.requiresNetwork
           && !capabilities.requiresAuthentication,
           "2 offline capabilities");

    bool asynchronous = false;
    expect(request(&provider, "what patch is loaded?", context(), &asynchronous)
               == "Current patch: U29-1 — WC ALL-IN-ONE."
           && asynchronous,
           "3 patch response is deterministic and asynchronous");
    expect(request(&provider, "what amps?", context()).contains(
               "PREAMP A: MATCH Drive. PREAMP B: VO Drive."),
           "4 preamp response");
    expect(request(&provider, "what delay am I using?", context()).contains(
               "Dual L/R delay"),
           "5 delay response");
    expect(request(&provider, "what reverb?", context()).contains(
               "Modulate reverb"),
           "6 reverb response");
    expect(request(&provider, "routing", context())
               == "Current routing mode: Dual L/R.",
           "7 routing response");
    expect(request(&provider, "chain", context()).contains("PREAMP A"),
           "8 signal chain response");
    expect(request(&provider, "help", AIContext()).contains(
               "offline development provider"),
           "9 help works without patch context");
    expect(request(&provider, "patch", AIContext()).contains(
               "Patch context is unavailable"),
           "10 invalid context is safe");

    bool cancelled = false;
    bool lateResponse = false;
    QObject::connect(&provider, &AIProvider::requestCancelled,
                     [&](QString id) { cancelled = id == "cancel"; });
    QObject::connect(&provider, &AIProvider::responseReady,
                     [&](QString id, AIResponse) {
        if (id == "cancel") lateResponse = true;
    });
    AIRequest cancelRequest;
    cancelRequest.userMessage = "patch";
    cancelRequest.context = context();
    provider.sendRequest("cancel", cancelRequest);
    provider.cancelRequest("cancel");
    wait();
    expect(cancelled && !lateResponse, "11 cancel prevents late response");

    AIContext privateContext = context();
    privateContext.root.insert("fileName", "/Users/private/secret.syx");
    const QJsonObject before = privateContext.root;
    const QString fallback = request(&provider, "hello", privateContext);
    expect(!fallback.contains("/Users/") && !fallback.contains("fileName"),
           "12 private context fields are not echoed");
    expect(privateContext.root == before, "13 provider does not mutate context");
    expect(request(&provider, "hello", context())
               == "Local Test provider received your message. The conversational AI provider has not been connected yet.",
           "14 fallback is explicit about Local Test limitation");

    QTranslator portuguese;
    expect(portuguese.load(QStringLiteral(":/translations/gtlab_pt_BR.qm"))
           && app.installTranslator(&portuguese),
           "15 pt-BR catalog loads");
    expect(request(&provider, "qual patch está carregado?", context(), nullptr,
                   QStringLiteral("pt-BR"))
               == "Patch atual: U29-1 — WC ALL-IN-ONE.",
           "16 patch response in pt-BR");
    expect(request(&provider, "qual delay estou usando?", context(), nullptr,
                   QStringLiteral("pt-BR")).contains(
               "Você está usando delay Dual L/R."),
           "17 delay response in pt-BR");
    expect(request(&provider, "ajuda", AIContext(), nullptr,
                   QStringLiteral("pt-BR")).contains(
               "provider de desenvolvimento offline"),
           "18 help response in pt-BR");
    expect(request(&provider, "olá", context(), nullptr,
                   QStringLiteral("pt-BR"))
               == "O provider Local Test recebeu sua mensagem. O provider de IA conversacional ainda não foi conectado.",
           "19 fallback response in pt-BR");
    app.removeTranslator(&portuguese);

    // This harness links no SysxIO, MIDI or patch-writing implementation.
    expect(before == privateContext.root,
           "20 localization does not mutate patch data or use MIDI/SysEx");

    QTextStream(stdout) << "Mock AI Provider tests: assertions="
                        << assertions << " failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
