#ifndef AI_TONE_TONESTYLERECIPES_H
#define AI_TONE_TONESTYLERECIPES_H

#include "ai/AIProvider.h"
#include "ai/tone/ToneModels.h"
#include "ai/tone/ToneTaxonomy.h"
#include "ai/knowledge/GT10CapabilityService.h"

namespace Ai { namespace Tone {

struct ToneRecipeRationale {
    QString ptBr;
    QString en;
    QStringList sources; // Musical Guidance is distinct from technical truth.
};
struct ToneRecipeStep {
    QString blockId;
    QString parameterId;
    QStringList compatibleTypes;
    QString ptBr;
    QString en;
    bool optional = false;
    ToneRecipeRationale rationale;
};
struct ToneRecipe {
    ToneFamily family = ToneFamily::Clean;
    ToneProfile profile = ToneProfile::Neutral;
    QStringList goals;
    QStringList preferredBlocks;
    QList<ToneRecipeStep> blockGuidance;
    QStringList optionalBlocks;
    QStringList avoid;
    ToneRecipeRationale rationale;
};
struct ToneCatalogEntry {
    QString blockId;
    QString type; // Exact existing CapabilityService label, never a new effect.
    QStringList characterTags;
    QString typicalRole;
    QString gainTendency;
    QString brightnessTendency;
    QStringList suitableProfiles;
    QString limitation;
    QStringList sources;
};

enum class FieldValidationType {
    PhysicalFunctionalValidation, PhysicalToneValidation,
    CuratedMusicalGuidance, OfficialDocumentation
};
struct FieldKnowledgeEntry {
    QString id;
    QString claimPtBr;
    QString claimEn;
    QString category;
    QString scope;
    FieldValidationType validationType = FieldValidationType::CuratedMusicalGuidance;
    QString confidence;
    QString notes;
};

class ToneStyleRecipes {
public:
    QList<ToneRecipe> recipes() const;
    QList<ToneCatalogEntry> catalog() const;
    QList<FieldKnowledgeEntry> fieldKnowledge() const;
    ToneConversationReply respond(const AIRequest &request) const;

private:
    Knowledge::GT10CapabilityService capabilities;
};

} }
#endif
