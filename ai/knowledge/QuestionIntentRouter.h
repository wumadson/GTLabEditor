#ifndef AI_KNOWLEDGE_QUESTIONINTENTROUTER_H
#define AI_KNOWLEDGE_QUESTIONINTENTROUTER_H

#include "ai/knowledge/GT10CapabilityService.h"

#include <QJsonArray>
#include <QList>

namespace Ai {
struct AIContext;
namespace Knowledge {

enum class QuestionIntent {
    CurrentPatchFact,
    AssessCurrentSetting,
    CompareEffectTypes,
    CompareParameters,
    ExplainEffectType,
    ExplainParameter,
    ListEffectTypes,
    HowTo,
    CapabilityDiscovery,
    Capability,
    ExplainTone,
    AdjustTone,
    Justify,
    Refine,
    CompareRecommendation,
    DetailFollowUp,
    Feedback,
    Help,
    Clarify,
    Unknown
};

enum class QuestionDestination { Knowledge, PatchContext, Tone, Clarify, Unknown };

enum class QuestionEntityKind {
    EffectFamily,
    EffectType,
    Parameter,
    Controller,
    PatchReference,
    Unresolved
};

struct QuestionEntity
{
    QuestionEntityKind kind = QuestionEntityKind::Unresolved;
    QString family;
    QString canonicalName;
    QString matchedText;
    QString id;
    QString userValue;
    QString matchKind;
};

struct QuestionRoutingContext
{
    bool hasRecommendation = false;
    bool hasToneAnalysis = false;
    bool hasAssessment = false;
    const AIContext *patchContext = nullptr;
    QList<QuestionEntity> previousComparisonEntities;
};

struct QuestionRoutingResult
{
    QuestionIntent intent = QuestionIntent::Unknown;
    QuestionDestination destination = QuestionDestination::Unknown;
    QString confidence;
    QString source;
    QList<QuestionEntity> entities;

    bool isComparison() const;
    QJsonArray entitiesJson() const;
};

class QuestionIntentRouter
{
public:
    QuestionRoutingResult route(
        const QString &question,
        const QuestionRoutingContext &context = QuestionRoutingContext()) const;

    static QString intentName(QuestionIntent intent);
    static QString destinationName(QuestionDestination destination);
    static QString entityKindName(QuestionEntityKind kind);

private:
    QList<QuestionEntity> resolveEntities(
        const QString &normalized,
        const QuestionRoutingContext &context) const;
    GT10CapabilityService capabilities;
};

} // namespace Knowledge
} // namespace Ai

#endif
