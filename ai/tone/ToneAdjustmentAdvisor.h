#ifndef AI_TONE_TONEADJUSTMENTADVISOR_H
#define AI_TONE_TONEADJUSTMENTADVISOR_H

#include "ai/AIContextBuilder.h"
#include "ai/knowledge/GT10CapabilityService.h"
#include "ai/tone/ToneModels.h"

namespace Ai {
namespace Tone {

class ToneAdjustmentAdvisor
{
public:
    ToneAdvice advise(const AIContext &context,
                      const ToneAdjustmentIntent &intent,
                      const QString &language,
                      const QList<ToneConstraint> &constraints = {}) const;

private:
    Knowledge::GT10CapabilityService capabilityService;
};

} // namespace Tone
} // namespace Ai

#endif
