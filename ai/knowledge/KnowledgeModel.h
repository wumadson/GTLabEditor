#ifndef AI_KNOWLEDGE_KNOWLEDGEMODEL_H
#define AI_KNOWLEDGE_KNOWLEDGEMODEL_H

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace Ai {
namespace Knowledge {

enum class Confidence { Authoritative, Derived, Guidance, Unknown };
enum class SourceKind {
    Backend,
    MidiTable,
    EditorModel,
    Guide,
    OfficialDocumentation,
    OfficialPatchReference,
    DerivedFromOfficialDocumentation,
    Inference,
    None
};
enum class DataScope { Patch, System, Mixed, Unknown };
enum class DocumentationCoverage { Covered, Partial, Unavailable };

struct KnowledgeTopic
{
    QString id;
    QString category;
    QString title;
    QStringList aliases;
    DataScope scope = DataScope::Unknown;
    QStringList relatedBlocks;
    SourceKind sourceKind = SourceKind::None;
};

struct ParameterCapability
{
    QString id;
    QString name;
    QString blockId;
    QString effectType;
    QString minimum;
    QString maximum;
    QStringList enumValues;
    bool available = false;
    SourceKind sourceKind = SourceKind::None;
};

struct ControllerCapability
{
    QString id;
    QString name;
    DataScope scope = DataScope::Unknown;
    bool assignSource = false;
    QString configuredIn;
};

struct DocumentationEntry
{
    QString id;
    QString documentId;
    QString title;
    QStringList aliases;
    QString section;
    int pageStart = 0;
    int pageEnd = 0;
    QString body;
    QString answerEn;
    QString answerPtBr;
    QString language;
    QStringList relatedTopics;
    QStringList relatedBlocks;
    QString sourceDocument;
    SourceKind sourceKind = SourceKind::OfficialDocumentation;
    QJsonObject claims;
};

struct DocumentationSearchResult
{
    DocumentationEntry entry;
    int score = 0;
};

struct LocalKnowledgeAnswer
{
    bool answered = false;
    Confidence confidence = Confidence::Unknown;
    QString topicId;
    QString text;
    SourceKind sourceKind = SourceKind::None;
    QStringList relatedBlocks;
    QJsonObject metadata;
};

} // namespace Knowledge
} // namespace Ai

#endif
