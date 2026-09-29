#include "ai/LocalGT10Provider.h"

namespace Ai {

LocalGT10Provider::LocalGT10Provider(QObject *parent)
    : AIProvider(parent)
{
    responseTimer.setSingleShot(true);
    responseTimer.setInterval(120);
    connect(&responseTimer, &QTimer::timeout,
            this, &LocalGT10Provider::finishRequest);
}

QString LocalGT10Provider::providerId() const
{
    return QStringLiteral("local-gt10");
}

QString LocalGT10Provider::providerName() const
{
    return tr("Local GT-10");
}

AIProviderCapabilities LocalGT10Provider::capabilities() const
{
    AIProviderCapabilities result;
    result.supportsConversation = true;
    return result;
}

void LocalGT10Provider::sendRequest(const QString &requestId,
                                    const AIRequest &request)
{
    if (!activeRequestId.isEmpty()) {
        emit errorOccurred(requestId,
                           tr("Local GT-10 is already handling a request."));
        return;
    }
    activeRequestId = requestId;
    activeRequest = request;
    responseTimer.start();
}

void LocalGT10Provider::cancelRequest(const QString &requestId)
{
    if (requestId != activeRequestId)
        return;
    responseTimer.stop();
    activeRequestId.clear();
    activeRequest = AIRequest();
    emit requestCancelled(requestId);
}

void LocalGT10Provider::finishRequest()
{
    if (activeRequestId.isEmpty())
        return;
    const QString requestId = activeRequestId;
    const AIRequest request = activeRequest;
    activeRequestId.clear();
    activeRequest = AIRequest();

    const Knowledge::LocalKnowledgeAnswer answer = engine.answer(
        request.userMessage, request.language, &request.context);
    AIResponse response;
    response.text = answer.text;
    response.metadata.insert("provider", providerId());
    response.metadata.insert("answered", answer.answered);
    response.metadata.insert("topicId", answer.topicId);
    response.metadata.insert("confidence", static_cast<int>(answer.confidence));
    response.metadata.insert("sourceKind", static_cast<int>(answer.sourceKind));
    if (answer.metadata.contains("documentationTrace"))
        response.metadata.insert("documentationTrace",
                                 answer.metadata.value("documentationTrace"));
    if (answer.metadata.contains("documentationConflict"))
        response.metadata.insert("documentationConflict",
                                 answer.metadata.value("documentationConflict"));
    emit responseReady(requestId, response);
}

} // namespace Ai
