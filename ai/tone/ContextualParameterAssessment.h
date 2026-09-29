#ifndef AI_TONE_CONTEXTUALPARAMETERASSESSMENT_H
#define AI_TONE_CONTEXTUALPARAMETERASSESSMENT_H

#include "ai/AIProvider.h"
#include "ai/knowledge/QuestionIntentRouter.h"
#include "ai/tone/ToneModels.h"

namespace Ai {
namespace Tone {

class ContextualParameterAssessment
{
public:
    ToneConversationReply respond(
        const AIRequest &request,
        const Knowledge::QuestionRoutingResult &routing);
    ToneConversationReply justify(const AIRequest &request);
    bool hasAssessment() const;
    const ParameterAssessment &lastAssessment() const;
    void reset();

private:
    ParameterAssessment assess(const AIRequest &request,
                               const Knowledge::QuestionEntity &entity,
                               const QString &evaluatedValue = QString()) const;
    QString compose(const ParameterAssessment &assessment,
                    const AIRequest &request,
                    bool hypothetical) const;
    ParameterAssessment previous;
    QString previousPatchIdentity;
};

} // namespace Tone
} // namespace Ai

#endif
