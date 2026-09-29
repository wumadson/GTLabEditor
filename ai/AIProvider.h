#ifndef AI_AIPROVIDER_H
#define AI_AIPROVIDER_H

#include "ai/AIContextBuilder.h"
#include "ai/AssistantMessage.h"

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

namespace Ai {

struct AIProviderCapabilities
{
    bool supportsConversation = false;
    bool supportsStructuredRecommendations = false;
    bool supportsStreaming = false;
    bool requiresNetwork = false;
    bool requiresAuthentication = false;
};

struct AIRequest
{
    QString userMessage;
    QString language;
    AIContext context;
    QList<AssistantMessage> conversationHistory;
};

struct AIResponse
{
    QString text;
    QJsonObject metadata;
};

class AIProvider : public QObject
{
    Q_OBJECT

public:
    explicit AIProvider(QObject *parent = nullptr) : QObject(parent) {}
    ~AIProvider() override = default;

    virtual QString providerId() const = 0;
    virtual QString providerName() const = 0;
    virtual AIProviderCapabilities capabilities() const = 0;
    virtual void sendRequest(const QString &requestId,
                             const AIRequest &request) = 0;
    virtual void cancelRequest(const QString &requestId) = 0;
    virtual void resetConversation() {}

signals:
    void responseReady(QString requestId, Ai::AIResponse response);
    void errorOccurred(QString requestId, QString error);
    void requestCancelled(QString requestId);
};

} // namespace Ai

Q_DECLARE_METATYPE(Ai::AIResponse)

#endif // AI_AIPROVIDER_H
