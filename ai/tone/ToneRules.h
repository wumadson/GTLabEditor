#ifndef AI_TONE_TONERULES_H
#define AI_TONE_TONERULES_H

#include "ai/tone/ToneModels.h"

#include <QString>

namespace Ai {
namespace Tone {

struct ToneParameterRule
{
    QString blockId;
    QString parameterId;
    double minimum = 0.0;
    double maximum = 100.0;
    double smallStepMinimum = 5.0;
    double smallStepMaximum = 10.0;
    ToneSourceKind sourceKind = ToneSourceKind::ToneRule;
};

class ToneRules
{
public:
    static bool parameterRule(const QString &blockId,
                              const QString &parameterId,
                              ToneParameterRule *rule);
    static double normalizedStrength(double value, double low,
                                     double high);
    static QString featureLabel(const QString &id, const QString &language);
    static QString blockLabel(const QString &id, const QString &language);
};

} // namespace Tone
} // namespace Ai

#endif
