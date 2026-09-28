#include "ai/MockAIProvider.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

namespace Ai {
namespace {

QJsonObject effect(const QJsonObject &root, const QString &id)
{
    for (const QJsonValue &value : root.value("effects").toArray()) {
        const QJsonObject candidate = value.toObject();
        if (candidate.value("id").toString() == id)
            return candidate;
    }
    return {};
}

QString effectType(const QJsonObject &root, const QString &id)
{
    return effect(root, id).value("type").toString();
}

QString moduleNames(const QJsonArray &modules)
{
    QStringList names;
    for (const QJsonValue &value : modules) {
        const QString name = value.toObject().value("name").toString();
        if (!name.isEmpty())
            names.append(name);
    }
    return names.join(QStringLiteral(" → "));
}

bool containsAny(const QString &text, const QStringList &terms)
{
    for (const QString &term : terms) {
        if (text.contains(term))
            return true;
    }
    return false;
}

} // namespace

MockAIProvider::MockAIProvider(QObject *parent)
    : AIProvider(parent)
{
    responseTimer.setSingleShot(true);
    responseTimer.setInterval(450);
    connect(&responseTimer, &QTimer::timeout,
            this, &MockAIProvider::finishRequest);
}

QString MockAIProvider::providerId() const
{
    return QStringLiteral("local-test");
}

QString MockAIProvider::providerName() const
{
    return tr("Local Test");
}

AIProviderCapabilities MockAIProvider::capabilities() const
{
    AIProviderCapabilities result;
    result.supportsConversation = true;
    return result;
}

void MockAIProvider::sendRequest(const QString &requestId,
                                 const AIRequest &request)
{
    if (!activeRequestId.isEmpty()) {
        emit errorOccurred(requestId, tr("Local Test is already handling a request."));
        return;
    }
    activeRequestId = requestId;
    activeRequest = request;
    responseTimer.start();
}

void MockAIProvider::cancelRequest(const QString &requestId)
{
    if (requestId != activeRequestId)
        return;
    responseTimer.stop();
    activeRequestId.clear();
    activeRequest = AIRequest();
    emit requestCancelled(requestId);
}

void MockAIProvider::finishRequest()
{
    if (activeRequestId.isEmpty())
        return;
    const QString requestId = activeRequestId;
    const AIRequest request = activeRequest;
    activeRequestId.clear();
    activeRequest = AIRequest();

    if (request.userMessage.contains(QStringLiteral("simulate error"),
                                     Qt::CaseInsensitive)) {
        emit errorOccurred(requestId,
                           tr("Local Test simulated a provider error."));
        return;
    }

    AIResponse response;
    response.text = responseFor(request);
    response.metadata.insert(QStringLiteral("provider"), providerId());
    emit responseReady(requestId, response);
}

QString MockAIProvider::responseFor(const AIRequest &request) const
{
    const QString prompt = request.userMessage.toLower();
    const QJsonObject root = request.context.root;
    const QJsonObject patch = root.value("patch").toObject();

    if (containsAny(prompt, {QStringLiteral("help"),
                             QStringLiteral("ajuda")})) {
        return tr("Local Test is an offline development provider. It can summarize the current patch, amps, delay, reverb, routing, and signal chain.");
    }

    if (root.isEmpty() || patch.isEmpty())
        return tr("Patch context is unavailable. Local Test cannot inspect the current tone yet.");

    if (prompt.contains(QStringLiteral("patch"))) {
        return tr("Current patch: %1 — %2.")
            .arg(patch.value("number").toString(),
                 patch.value("name").toString());
    }

    if (containsAny(prompt, {QStringLiteral("amp"),
                             QStringLiteral("amplificador"),
                             QStringLiteral("pré"),
                             QStringLiteral("pre")})) {
        const QString preampA = effectType(root, QStringLiteral("preampA"));
        const QString preampB = effectType(root, QStringLiteral("preampB"));
        if (!preampA.isEmpty() || !preampB.isEmpty())
            return tr("PREAMP A: %1. PREAMP B: %2.")
                .arg(preampA.isEmpty() ? tr("Unavailable") : preampA,
                     preampB.isEmpty() ? tr("Unavailable") : preampB);
    }

    if (containsAny(prompt, {QStringLiteral("delay"),
                             QStringLiteral("atraso")})) {
        const QJsonObject delay = effect(root, QStringLiteral("delay"));
        const QString type = delay.value("type").toString();
        if (!type.isEmpty()) {
            QString answer = tr("You are using %1 delay.").arg(type);
            const QJsonObject delay1 = delay.value("delay1").toObject();
            if (!delay1.isEmpty())
                answer += tr(" Delay 1 time: %1; feedback: %2.")
                    .arg(delay1.value("time").toString(),
                         delay1.value("feedback").toString());
            return answer;
        }
    }

    if (containsAny(prompt, {QStringLiteral("reverb"),
                             QStringLiteral("reverberação"),
                             QStringLiteral("reverberacao")})) {
        const QJsonObject reverb = effect(root, QStringLiteral("reverb"));
        const QString type = reverb.value("type").toString();
        if (!type.isEmpty()) {
            QString answer = tr("You are using %1 reverb.").arg(type);
            if (reverb.contains("reverbTime"))
                answer += tr(" Time: %1; effect level: %2.")
                    .arg(reverb.value("reverbTime").toString(),
                         reverb.value("effectLevel").toString());
            return answer;
        }
    }

    if (containsAny(prompt, {QStringLiteral("routing"),
                             QStringLiteral("roteamento")})) {
        const QString mode = root.value("routing").toObject()
                                 .value("mode").toString();
        return mode.isEmpty()
            ? tr("Routing information is unavailable.")
            : tr("Current routing mode: %1.").arg(mode);
    }

    if (containsAny(prompt, {QStringLiteral("chain"),
                             QStringLiteral("cadeia"),
                             QStringLiteral("sinal")})) {
        const QJsonObject chain = root.value("signalChain").toObject();
        QStringList parts;
        const QString prefix = moduleNames(chain.value("commonPrefix").toArray());
        const QString pathA = moduleNames(chain.value("pathA").toArray());
        const QString pathB = moduleNames(chain.value("pathB").toArray());
        const QString suffix = moduleNames(chain.value("commonSuffix").toArray());
        if (!prefix.isEmpty()) parts << prefix;
        if (!pathA.isEmpty()) parts << tr("A: %1").arg(pathA);
        if (!pathB.isEmpty()) parts << tr("B: %1").arg(pathB);
        if (!suffix.isEmpty()) parts << suffix;
        return parts.isEmpty()
            ? tr("Signal chain information is unavailable.")
            : tr("Signal chain: %1.").arg(parts.join(QStringLiteral(" | ")));
    }

    return tr("Local Test provider received your message. The conversational AI provider has not been connected yet.");
}

} // namespace Ai
