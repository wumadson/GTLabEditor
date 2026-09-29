#ifndef AI_LOCALGT10PROVIDER_H
#define AI_LOCALGT10PROVIDER_H

#include "ai/AIProvider.h"
#include "ai/knowledge/LocalKnowledgeEngine.h"
#include "ai/knowledge/QuestionIntentRouter.h"
#include "ai/tone/ToneConversationReasoner.h"
#include "ai/tone/ContextualParameterAssessment.h"

#include <QTimer>

namespace Ai {

class LocalGT10Provider : public AIProvider
{
    Q_OBJECT

public:
    explicit LocalGT10Provider(QObject *parent = nullptr);

    QString providerId() const override;
    QString providerName() const override;
    AIProviderCapabilities capabilities() const override;
    void sendRequest(const QString &requestId,
                     const AIRequest &request) override;
    void cancelRequest(const QString &requestId) override;
    void resetConversation() override;

private slots:
    void finishRequest();

private:
    Knowledge::LocalKnowledgeEngine engine;
    Knowledge::QuestionIntentRouter questionRouter;
    Tone::ToneConversationReasoner toneConversationReasoner;
    Tone::ContextualParameterAssessment parameterAssessment;
    bool lastResponseWasAssessment = false;
    QList<Knowledge::QuestionEntity> lastComparisonEntities;
    QTimer responseTimer;
    QString activeRequestId;
    AIRequest activeRequest;
};

} // namespace Ai

#endif
