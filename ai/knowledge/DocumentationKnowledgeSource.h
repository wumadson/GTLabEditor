#ifndef AI_KNOWLEDGE_DOCUMENTATIONKNOWLEDGESOURCE_H
#define AI_KNOWLEDGE_DOCUMENTATIONKNOWLEDGESOURCE_H

#include "ai/knowledge/KnowledgeModel.h"

#include <QList>
#include <QMap>

namespace Ai {
namespace Knowledge {

class DocumentationKnowledgeSource
{
public:
    virtual ~DocumentationKnowledgeSource() = default;
    virtual bool isAvailable() const = 0;
    virtual QStringList documentNames() const = 0;
    virtual QList<DocumentationSearchResult> search(
        const QString &query, const QString &language,
        int maximumResults = 5) const = 0;
    virtual bool topic(const QString &id, const QString &language,
                       DocumentationEntry *entry) const = 0;
    virtual bool findByAlias(const QString &alias, const QString &language,
                             DocumentationEntry *entry) const = 0;
    virtual QList<DocumentationEntry> relatedTopics(
        const QString &topicId, const QString &language) const = 0;
    virtual QMap<QString, DocumentationCoverage> coverageReport() const = 0;
    virtual LocalKnowledgeAnswer answer(const QString &question,
                                        const QString &language) const = 0;
};

// In-memory index for content extracted from explicitly approved local
// official documents. Runtime starts empty until those documents are supplied.
class LocalDocumentationIndex final : public DocumentationKnowledgeSource
{
public:
    explicit LocalDocumentationIndex(
        const QList<DocumentationEntry> &approvedEntries = {});

    bool isAvailable() const override;
    QStringList documentNames() const override;
    QList<DocumentationSearchResult> search(
        const QString &query, const QString &language,
        int maximumResults = 5) const override;
    bool topic(const QString &id, const QString &language,
               DocumentationEntry *entry) const override;
    bool findByAlias(const QString &alias, const QString &language,
                     DocumentationEntry *entry) const override;
    QList<DocumentationEntry> relatedTopics(
        const QString &topicId, const QString &language) const override;
    QMap<QString, DocumentationCoverage> coverageReport() const override;
    LocalKnowledgeAnswer answer(const QString &question,
                                const QString &language) const override;

private:
    QList<DocumentationEntry> entries;
};

// Loads the versioned indexes embedded in the Qt resource system. No PDF or
// source-tree path is consulted at runtime.
LocalDocumentationIndex officialGT10DocumentationIndex();

} // namespace Knowledge
} // namespace Ai

#endif
