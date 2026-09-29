#ifndef AI_KNOWLEDGE_KNOWLEDGEGUIDES_H
#define AI_KNOWLEDGE_KNOWLEDGEGUIDES_H

#include "ai/knowledge/KnowledgeModel.h"

#include <QList>

namespace Ai {
namespace Knowledge {

struct KnowledgeGuide
{
    QString id;
    QString titlePtBr;
    QString titleEn;
    QString bodyPtBr;
    QString bodyEn;
    QStringList relatedTopics;
    Confidence sourceLevel = Confidence::Guidance;
};

QList<KnowledgeGuide> builtInGuides();
const KnowledgeGuide *findGuide(const QList<KnowledgeGuide> &guides,
                                const QString &id);

} // namespace Knowledge
} // namespace Ai

#endif
