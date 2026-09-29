#ifndef AI_KNOWLEDGE_LOCALKNOWLEDGEENGINE_H
#define AI_KNOWLEDGE_LOCALKNOWLEDGEENGINE_H

#include "ai/AIContextBuilder.h"
#include "ai/knowledge/DocumentationKnowledgeSource.h"
#include "ai/knowledge/GT10CapabilityService.h"
#include "ai/knowledge/KnowledgeGuides.h"

namespace Ai {
namespace Knowledge {

class LocalKnowledgeEngine
{
public:
    enum class Intent {
        CurrentPatch,
        Capability,
        EffectInfo,
        HowToAssign,
        HowToController,
        HowToExpression,
        SignalChain,
        Routing,
        PatchVsSystem,
        Help,
        Unknown
    };

    LocalKnowledgeEngine();
    explicit LocalKnowledgeEngine(
        const DocumentationKnowledgeSource *documentationSource);

    Intent classify(const QString &question) const;
    LocalKnowledgeAnswer answer(const QString &question,
                                const QString &language,
                                const AIContext *context = nullptr) const;
    const GT10CapabilityService &capabilities() const;

private:
    bool portuguese(const QString &language) const;
    LocalKnowledgeAnswer unknown(const QString &language,
                                 const QString &topic = QString()) const;
    LocalKnowledgeAnswer guideAnswer(const QString &id,
                                     const QString &language) const;
    LocalKnowledgeAnswer currentPatchAnswer(const QString &normalized,
                                            const QString &language,
                                            const AIContext *context) const;
    LocalKnowledgeAnswer capabilityAnswer(const QString &normalized,
                                          const QString &language) const;
    LocalKnowledgeAnswer controllerAnswer(const QString &normalized,
                                          const QString &language) const;
    LocalKnowledgeAnswer enrichWithDocumentation(
        const LocalKnowledgeAnswer &base, const QString &question,
        const QString &language) const;
    LocalKnowledgeAnswer documentationUnknown(
        const QString &language, const QString &topic = QString()) const;

    GT10CapabilityService capabilityService;
    QList<KnowledgeGuide> guides;
    LocalDocumentationIndex officialDocumentation;
    const DocumentationKnowledgeSource *documentationSource = nullptr;
};

} // namespace Knowledge
} // namespace Ai

#endif
