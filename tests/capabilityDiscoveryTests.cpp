#include "ai/AssistantFeatureRegistry.h"
#include "ai/LocalGT10Provider.h"
#include "ai/knowledge/QuestionIntentRouter.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonObject>
#include <QSet>
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

AIResponse ask(LocalGT10Provider *provider, const QString &prompt,
               const QString &language, bool withPatch = false)
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
    if (withPatch) {
        request.context.root.insert(
            QStringLiteral("patch"),
            QJsonObject{{QStringLiteral("number"), QStringLiteral("U01-1")},
                        {QStringLiteral("name"), QStringLiteral("TEST")}});
    }
    provider->sendRequest(QString::number(assertions + 1), request);
    QTimer::singleShot(1000, &loop, &QEventLoop::quit);
    loop.exec();
    QObject::disconnect(connection);
    return result;
}

bool containsAll(const QString &text, const QStringList &needles)
{
    for (const QString &needle : needles) {
        if (!text.contains(needle, Qt::CaseInsensitive))
            return false;
    }
    return true;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QuestionIntentRouter router;

    const QStringList ptPrompts = {
        QString::fromUtf8("O que você pode fazer?"),
        QString::fromUtf8("O que você sabe fazer?"),
        QString::fromUtf8("Como você pode me ajudar?"),
        QString::fromUtf8("Que tipo de pergunta posso fazer?"),
        QString::fromUtf8("Quais perguntas posso fazer?"),
        QString::fromUtf8("O que eu posso perguntar?")
    };
    for (const QString &prompt : ptPrompts) {
        expect(router.route(prompt).intent
                   == QuestionIntent::CapabilityDiscovery,
               QStringLiteral("PT discovery: ") + prompt);
    }
    const QStringList enPrompts = {
        QStringLiteral("What can you do?"),
        QStringLiteral("How can you help me?"),
        QStringLiteral("What can I ask?"),
        QStringLiteral("What kinds of questions can I ask?"),
        QStringLiteral("What do you know how to do?"),
        QStringLiteral("What can you help with?")
    };
    for (const QString &prompt : enPrompts) {
        expect(router.route(prompt).intent
                   == QuestionIntent::CapabilityDiscovery,
               QStringLiteral("EN discovery: ") + prompt);
    }
    expect(router.route(QString::fromUtf8("O que a GT-10 pode fazer?")).intent
               == QuestionIntent::Capability,
           "GT-10 capability routes to device knowledge");

    AssistantFeatureRegistry registry;
    const QList<AssistantFeature> enabled = registry.enabledFeatures();
    const QList<AssistantFeature> disabled = registry.disabledFeatures();
    expect(!enabled.isEmpty(), "registry has enabled features");
    expect(!disabled.isEmpty(), "registry has disabled features");
    QSet<QString> enabledKeys;
    for (const AssistantFeature &feature : enabled) {
        enabledKeys.insert(feature.key);
        expect(!feature.titleEn.isEmpty() && !feature.titlePtBr.isEmpty(),
               QStringLiteral("enabled titles: ") + feature.key);
        expect(feature.limitationEn.isEmpty()
                   && feature.limitationPtBr.isEmpty(),
               QStringLiteral("enabled is not limitation: ") + feature.key);
    }
    for (const AssistantFeature &feature : disabled) {
        expect(!feature.limitationEn.isEmpty()
                   && !feature.limitationPtBr.isEmpty(),
               QStringLiteral("disabled limitation: ") + feature.key);
        expect(!enabledKeys.contains(feature.key),
               QStringLiteral("disabled key not enabled: ") + feature.key);
    }
    const QStringList examplesPt = registry.exampleQuestions("pt-BR");
    const QStringList examplesEn = registry.exampleQuestions("en");
    expect(examplesPt.size() >= 5 && examplesPt.size() <= 8,
           "pt-BR exposes five to eight examples");
    expect(examplesEn.size() >= 5 && examplesEn.size() <= 8,
           "English exposes five to eight examples");
    expect(examplesPt == registry.exampleQuestions("pt-BR")
               && examplesEn == registry.exampleQuestions("en"),
           "registry output is deterministic");

    LocalGT10Provider provider;
    const AIResponse pt = ask(&provider,
        QString::fromUtf8("O que você pode fazer?"), "pt-BR", true);
    expect(pt.metadata.value("detectedIntent")
               == QLatin1String("CAPABILITY_DISCOVERY"),
           "provider exposes discovery intent");
    expect(pt.metadata.value("answered").toBool()
               && pt.metadata.value("route")
                    == QLatin1String("capability_discovery"),
           "provider answers via discovery route");
    expect(containsAll(pt.text,
               {QString::fromUtf8("preset"), QString::fromUtf8("timbre"),
                QStringLiteral("GT-10"), QStringLiteral("Assign"),
                QStringLiteral("EXP"), QString::fromUtf8("não posso"),
                QString::fromUtf8("áudio"),
                QString::fromUtf8("automaticamente")}),
           "pt-BR response covers capabilities and limitations");
    expect(pt.text.contains(QString::fromUtf8("preset carregado agora")),
           "valid patch context adds a starting suggestion");

    const AIResponse en = ask(&provider, QStringLiteral("What can you do?"),
                              QStringLiteral("en"));
    expect(containsAll(en.text,
               {QStringLiteral("preset"), QStringLiteral("tone"),
                QStringLiteral("GT-10"), QStringLiteral("Assign"),
                QStringLiteral("EXP"), QStringLiteral("cannot yet"),
                QStringLiteral("audio"), QStringLiteral("automatically")}),
           "English response covers capabilities and limitations");
    expect(!en.text.contains(QStringLiteral("currently loaded")),
           "missing patch context does not promise current-patch explanation");

    const AIResponse device = ask(&provider,
        QString::fromUtf8("O que a GT-10 pode fazer?"), "pt-BR");
    expect(device.metadata.value("detectedIntent")
               == QLatin1String("CAPABILITY"),
           "provider preserves device-capability distinction");

    QTextStream(stdout) << "Capability Discovery tests: assertions="
                        << assertions << " failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
