#ifndef AI_TONE_TONEMODELS_H
#define AI_TONE_TONEMODELS_H

#include "ai/knowledge/KnowledgeModel.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace Ai {
namespace Tone {

enum class ToneSourceKind {
    PatchContext,
    ToneRule,
    OfficialDocumentation,
    Guidance
};

struct ToneEvidence
{
    QString blockId;
    QString parameterId;
    QString value;
    ToneSourceKind sourceKind = ToneSourceKind::PatchContext;
};

struct ToneFeature
{
    QString id;
    double strength = 0.0;
    QList<ToneEvidence> evidence;
    QList<ToneSourceKind> sources;
    Knowledge::Confidence confidence = Knowledge::Confidence::Unknown;
    QStringList relatedBlocks;
};

struct ToneAnalysis
{
    QString summary;
    QList<ToneFeature> features;
    QList<ToneEvidence> evidence;
    QStringList caveats;
};

enum class ToneDirection {
    None,
    MoreCrunch,
    MoreClean,
    MoreAmbient,
    MoreDeep,
    MoreBright,
    MoreDark,
    MoreDefined,
    LessMuddy,
    MoreWide,
    MoreFocused,
    MoreAggressive,
    MoreSmooth,
    MoreDynamic,
    MoreCompressed
};

enum class ChangeMagnitude { Small, Moderate, Large };

enum class ToneConstraint {
    PreserveClean,
    PreserveDynamics,
    PreserveDefinition,
    PreserveBrightness,
    PreserveLowEnd,
    PreserveAmbience,
    AvoidOdDs,
    AvoidReverb,
    AvoidDelay,
    AvoidAmp,
    SmallChangeOnly
};

enum class ToneResponseMode {
    Unknown,
    Explain,
    Recommend,
    Justify,
    Refine,
    Compare,
    Assess,
    HypotheticalCompare,
    Clarify
};

struct ParameterAssessment
{
    QString blockId;
    QString parameterId;
    QString currentValue;
    QString userReferencedValue;
    QString range;
    QString contextualRole;
    QStringList supportingEvidence;
    QStringList tradeoffs;
    QString assessment;
    Knowledge::Confidence confidence = Knowledge::Confidence::Unknown;
    bool valid = false;
};

struct ToneAdjustmentIntent
{
    ToneDirection direction = ToneDirection::None;
    ChangeMagnitude magnitude = ChangeMagnitude::Small;
    QString matchedText;

    bool isValid() const { return direction != ToneDirection::None; }
};

struct ToneAdjustmentSuggestion
{
    QString blockId;
    QString parameterId;
    QString currentValue;
    QString suggestedDirection;
    QString suggestedTargetRange;
    QString reason;
    Knowledge::Confidence confidence = Knowledge::Confidence::Unknown;
    QList<ToneSourceKind> sources;
    bool optionalActivation = false;
};

struct ToneAdvice
{
    ToneAdjustmentIntent intent;
    QList<ToneAdjustmentSuggestion> suggestions;
    QString response;
    QStringList caveats;
};

struct ToneRecommendationRecord
{
    ToneAdjustmentIntent intent;
    QList<ToneAdjustmentSuggestion> suggestions;
    QList<ToneEvidence> evidence;
    QString rationale;
    QString tradeOff;
    QStringList affectedBlocks;
    QList<ToneConstraint> constraints;
    bool valid = false;
};

struct ToneConversationState
{
    QString currentPatchIdentity;
    ToneAnalysis lastToneAnalysis;
    ToneAdjustmentIntent lastToneIntent;
    ToneRecommendationRecord lastRecommendation;
    ParameterAssessment lastAssessment;
    QList<ToneConstraint> activeConstraints;
    QString lastResponse;
    ToneResponseMode lastMode = ToneResponseMode::Unknown;
};

struct ToneConversationReply
{
    bool handled = false;
    bool answered = false;
    ToneResponseMode mode = ToneResponseMode::Unknown;
    QString text;
    QString topicId;
    QString route;
    ToneAnalysis analysis;
    ToneAdvice advice;
    ToneRecommendationRecord recommendation;
};

enum class ToneRequestKind {
    None,
    ExplainTone,
    AdjustTone,
    ArtistProfileUnavailable,
    UnsupportedCapability
};

struct ToneRequest
{
    ToneRequestKind kind = ToneRequestKind::None;
    ToneAdjustmentIntent adjustment;
};

} // namespace Tone
} // namespace Ai

#endif
