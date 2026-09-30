#ifndef AI_TONE_TONECONVERSATIONREASONER_H
#define AI_TONE_TONECONVERSATIONREASONER_H

#include "ai/AIProvider.h"
#include "ai/tone/ToneAdjustmentAdvisor.h"
#include "ai/tone/ToneAnalyzer.h"
#include "ai/tone/ToneIntentParser.h"
#include "ai/tone/ToneResponseComposer.h"

namespace Ai {
namespace Tone {

class ToneConversationReasoner
{
public:
    ToneConversationReply respond(const AIRequest &request);
    void reset();
    const ToneConversationState &state() const;

private:
    static QString patchIdentity(const AIContext &context);
    static QList<ToneConstraint> constraints(const QString &normalized);
    static bool isJustify(const QString &normalized);
    static bool isDetail(const QString &normalized);
    static bool isCompare(const QString &normalized);
    static bool isFeedback(const QString &normalized);
    static bool isAmbiguous(const QString &normalized);
    static void mergeConstraints(QList<ToneConstraint> *target,
                                 const QList<ToneConstraint> &incoming);
    ToneRecommendationRecord recommendationRecord(
        const ToneAdvice &advice, const ToneAnalysis &analysis,
        const QString &language) const;
    ToneConversationReply recommendation(const AIRequest &request,
        const ToneAdjustmentIntent &intent, ToneResponseMode mode);

    ToneConversationState conversationState;
    QString lastBuildQuestion;
    ToneAnalyzer analyzer;
    ToneIntentParser parser;
    ToneAdjustmentAdvisor advisor;
    ToneResponseComposer composer;
};

} // namespace Tone
} // namespace Ai

#endif
