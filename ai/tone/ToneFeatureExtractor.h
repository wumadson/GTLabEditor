#ifndef AI_TONE_TONEFEATUREEXTRACTOR_H
#define AI_TONE_TONEFEATUREEXTRACTOR_H

#include "ai/AIContextBuilder.h"
#include "ai/tone/ToneModels.h"

namespace Ai {
namespace Tone {

class ToneFeatureExtractor
{
public:
    QList<ToneFeature> extract(const AIContext &context) const;
};

} // namespace Tone
} // namespace Ai

#endif
