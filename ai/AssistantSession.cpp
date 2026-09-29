#include "ai/AssistantSession.h"

namespace Ai {

AssistantSession::AssistantSession(QObject *parent)
    : QObject(parent)
{
}

void AssistantSession::setProvider(AIProvider *provider)
{
    if (provider == currentProvider)
        return;
    if (!activeRequestId.isEmpty())
        cancel();
    if (currentProvider)
        disconnect(currentProvider, nullptr, this, nullptr);
    currentProvider = provider;
    if (currentProvider) {
        connect(currentProvider, &AIProvider::responseReady,
                this, &AssistantSession::handleResponse);
        connect(currentProvider, &AIProvider::errorOccurred,
                this, &AssistantSession::handleError);
        connect(currentProvider, &AIProvider::requestCancelled,
                this, &AssistantSession::handleCancelled);
    }
    emit providerChanged(currentProvider ? currentProvider->providerName()
                                         : QString());
}

AIProvider *AssistantSession::provider() const
{
    return currentProvider;
}

QList<AssistantMessage> AssistantSession::messages() const
{
    return history;
}

AssistantSession::State AssistantSession::state() const
{
    return currentState;
}

QString AssistantSession::contextLabel() const
{
    return currentContextLabel;
}

void AssistantSession::setResponseLanguage(const QString &language)
{
    const QString normalized = language.trimmed();
    currentResponseLanguage = normalized.isEmpty()
        ? QStringLiteral("en") : normalized;
}

QString AssistantSession::responseLanguage() const
{
    return currentResponseLanguage;
}

bool AssistantSession::sendMessage(const QString &text,
                                   const AIContext &context,
                                   const QString &contextLabel)
{
    const QString prompt = text.trimmed();
    if (prompt.isEmpty() || !currentProvider
        || currentState == Sending || currentState == Cancelling)
        return false;

    AIRequest request;
    request.userMessage = prompt;
    request.language = currentResponseLanguage;
    request.context = context;
    const int retainedMessages = 8;
    request.conversationHistory = history.mid(
        qMax(0, history.size() - retainedMessages), retainedMessages);

    currentContextLabel = contextLabel.trimmed().isEmpty()
        ? tr("Patch context unavailable") : contextLabel.trimmed();
    emit contextChanged(currentContextLabel);
    emit errorChanged(QString());
    appendMessage(AssistantMessage::User, prompt);

    activeRequestId = QString::number(++nextRequestId);
    setState(Sending);
    currentProvider->sendRequest(activeRequestId, request);
    return true;
}

void AssistantSession::cancel()
{
    if (!currentProvider || activeRequestId.isEmpty()
        || currentState != Sending)
        return;
    setState(Cancelling);
    currentProvider->cancelRequest(activeRequestId);
}

void AssistantSession::resetConversation()
{
    if (currentState == Sending)
        cancel();
    if (currentProvider)
        currentProvider->resetConversation();
    history.clear();
    emit conversationReset();
}

void AssistantSession::handleResponse(QString requestId, AIResponse response)
{
    if (requestId != activeRequestId || currentState != Sending)
        return;
    clearActiveRequest();
    appendMessage(AssistantMessage::Assistant, response.text);
    setState(Idle);
}

void AssistantSession::handleError(QString requestId, QString error)
{
    if (requestId != activeRequestId
        || (currentState != Sending && currentState != Cancelling))
        return;
    clearActiveRequest();
    emit errorChanged(error);
    appendMessage(AssistantMessage::System, error);
    setState(Error);
}

void AssistantSession::handleCancelled(QString requestId)
{
    if (requestId != activeRequestId || currentState != Cancelling)
        return;
    clearActiveRequest();
    setState(Idle);
}

void AssistantSession::appendMessage(AssistantMessage::Role role,
                                     const QString &text)
{
    AssistantMessage message;
    message.id = QString::number(++nextMessageId);
    message.role = role;
    message.text = text;
    message.timestamp = QDateTime::currentDateTimeUtc();
    history.append(message);
    emit messageAdded(message);
}

void AssistantSession::setState(State state)
{
    if (currentState == state)
        return;
    currentState = state;
    emit stateChanged(currentState);
}

void AssistantSession::clearActiveRequest()
{
    activeRequestId.clear();
}

} // namespace Ai
