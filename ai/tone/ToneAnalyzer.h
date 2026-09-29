#ifndef AI_TONE_TONEANALYZER_H
#define AI_TONE_TONEANALYZER_H

#include "ai/AIContextBuilder.h"
#include "ai/tone/ToneModels.h"

namespace Ai {
namespace Tone {

class ToneAnalyzer
{
public:
    ToneAnalysis analyze(const AIContext &context,
                         const QString &language) const;
};

} // namespace Tone
} // namespace Ai

#endif
