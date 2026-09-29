#ifndef AI_TONE_TONERESPONSECOMPOSER_H
#define AI_TONE_TONERESPONSECOMPOSER_H

#include "ai/AIContextBuilder.h"
#include "ai/tone/ToneModels.h"

namespace Ai {
namespace Tone {

class ToneResponseComposer
{
public:
    QString explain(const ToneAnalysis &analysis, const AIContext &context,
                    const QString &language, bool detailed = false) const;
    QString recommend(const ToneRecommendationRecord &record,
                      const AIContext &context, const QString &language,
                      bool refined = false) const;
    QString justify(const ToneRecommendationRecord &record,
                    const QString &language, bool detailed = true) const;
    QString compare(const AIContext &context, const QString &normalizedPrompt,
                    const ToneRecommendationRecord &record,
                    const QString &language) const;
    QString feedback(const ToneRecommendationRecord &record,
                     const QString &normalizedPrompt,
                     const QString &language) const;
    QString clarify(const QString &language, bool patchChanged = false) const;
};

} // namespace Tone
} // namespace Ai

#endif
