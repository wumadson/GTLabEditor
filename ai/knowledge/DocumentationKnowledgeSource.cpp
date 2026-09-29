#include "ai/knowledge/DocumentationKnowledgeSource.h"

#include "ai/knowledge/GT10CapabilityService.h"

#include <algorithm>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

namespace Ai {
namespace Knowledge {
namespace {

const QStringList kCoverageTopics = {
    "assign", "expression", "controllers", "signal_chain", "routing",
    "delay", "reverb", "midi", "system"
};

bool languageMatches(const QString &entryLanguage, const QString &language)
{
    return entryLanguage.compare(language, Qt::CaseInsensitive) == 0
        || (language.startsWith("pt", Qt::CaseInsensitive)
            && entryLanguage.startsWith("pt", Qt::CaseInsensitive))
        || (language.startsWith("en", Qt::CaseInsensitive)
            && entryLanguage.startsWith("en", Qt::CaseInsensitive));
}

QString searchableText(const DocumentationEntry &entry)
{
    return GT10CapabilityService::normalize(
        entry.title + " " + entry.aliases.join(" ") + " " + entry.section
        + " " + entry.body + " " + entry.relatedTopics.join(" ")
        + " " + entry.relatedBlocks.join(" "));
}

int scoreEntry(const DocumentationEntry &entry, const QString &query,
               const QString &language)
{
    const QString normalized = GT10CapabilityService::normalize(query);
    if (normalized.isEmpty())
        return 0;
    const QString title = GT10CapabilityService::normalize(entry.title);
    const bool titleMatch = title == normalized || title.contains(normalized)
        || normalized.contains(title);
    bool aliasMatch = false;
    int score = title == normalized ? 120 : (titleMatch ? 70 : 0);
    for (const QString &alias : entry.aliases) {
        const QString candidate = GT10CapabilityService::normalize(alias);
        if (candidate == normalized) {
            aliasMatch = true;
            score = qMax(score, 110);
        } else if (candidate.contains(normalized) || normalized.contains(candidate)) {
            aliasMatch = true;
            score = qMax(score, 65);
        }
    }
    const QString haystack = searchableText(entry);
    const QStringList tokens = normalized.split(' ', Qt::SkipEmptyParts);
    for (int i = 0; i < tokens.size(); ++i) {
        const QString &token = tokens.at(i);
        const bool gt10ModelToken = token == QStringLiteral("10")
            && i > 0 && tokens.at(i - 1) == QStringLiteral("gt");
        if (token.contains(QRegularExpression(QStringLiteral("\\d")))
            && !gt10ModelToken && !haystack.contains(token))
            return 0;
    }
    int matched = 0;
    for (const QString &token : tokens) {
        if (haystack.contains(token)) {
            ++matched;
            score += 8;
        }
    }
    if (matched != tokens.size() && !titleMatch && !aliasMatch)
        return 0;
    if (languageMatches(entry.language, language))
        score += 20;
    return score;
}

QJsonObject trace(const DocumentationEntry &entry)
{
    QJsonObject value;
    value.insert("documentId", entry.documentId);
    value.insert("sourceDocument", entry.sourceDocument);
    value.insert("section", entry.section);
    value.insert("pageStart", entry.pageStart);
    value.insert("pageEnd", entry.pageEnd);
    value.insert("sourceLanguage", entry.language);
    value.insert("sourceKind", entry.sourceKind == SourceKind::OfficialPatchReference
        ? "OfficialPatchReference" : "OfficialDocumentation");
    value.insert("claims", entry.claims);
    return value;
}

QStringList strings(const QJsonValue &value)
{
    QStringList result;
    for (const QJsonValue &item : value.toArray())
        result.append(item.toString());
    return result;
}

SourceKind sourceKind(const QString &value)
{
    return value == QStringLiteral("OfficialPatchReference")
        ? SourceKind::OfficialPatchReference
        : SourceKind::OfficialDocumentation;
}

QList<DocumentationEntry> loadIndex(const QString &resourcePath)
{
    QList<DocumentationEntry> result;
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly))
        return result;
    QJsonParseError error;
    const QJsonDocument json = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !json.isObject())
        return result;
    const QJsonObject root = json.object();
    const QJsonObject document = root.value("document").toObject();
    const QString documentId = document.value("id").toString();
    const QString documentTitle = document.value("title").toString();
    for (const QJsonValue &value : root.value("chunks").toArray()) {
        const QJsonObject object = value.toObject();
        DocumentationEntry entry;
        entry.id = object.value("id").toString();
        entry.documentId = object.value("documentId").toString(documentId);
        entry.title = object.value("title").toString();
        entry.aliases = strings(object.value("aliases"));
        entry.section = object.value("section").toString();
        entry.pageStart = object.value("pageStart").toInt();
        entry.pageEnd = object.value("pageEnd").toInt();
        entry.body = object.value("body").toString();
        entry.answerEn = object.value("answerEn").toString();
        entry.answerPtBr = object.value("answerPtBr").toString();
        entry.language = object.value("language").toString();
        entry.relatedTopics = strings(object.value("relatedTopics"));
        entry.relatedBlocks = strings(object.value("relatedBlocks"));
        entry.sourceDocument = documentTitle;
        entry.sourceKind = sourceKind(object.value("sourceKind").toString());
        entry.claims = object.value("claims").toObject();
        result.append(entry);
    }
    return result;
}

bool isLogicalDocumentName(const QString &name)
{
    const QString trimmed = name.trimmed();
    return !trimmed.isEmpty()
        && !QDir::isAbsolutePath(trimmed)
        && !QRegularExpression(QStringLiteral("^[A-Za-z]:[\\\\/]")).match(trimmed).hasMatch()
        && !trimmed.contains(QStringLiteral("://"));
}

} // namespace

LocalDocumentationIndex::LocalDocumentationIndex(
    const QList<DocumentationEntry> &approvedEntries)
{
    for (const DocumentationEntry &entry : approvedEntries) {
        // Reject entries without logical traceability. Filesystem paths are
        // deliberately never accepted or retained by this index.
        if (!entry.id.trimmed().isEmpty()
            && isLogicalDocumentName(entry.sourceDocument)
            && (entry.sourceKind == SourceKind::OfficialDocumentation
                || entry.sourceKind == SourceKind::OfficialPatchReference))
            entries.append(entry);
    }
}

bool LocalDocumentationIndex::isAvailable() const
{
    return !entries.isEmpty();
}

QStringList LocalDocumentationIndex::documentNames() const
{
    QStringList result;
    for (const DocumentationEntry &entry : entries) {
        if (!result.contains(entry.sourceDocument))
            result.append(entry.sourceDocument);
    }
    return result;
}

QList<DocumentationSearchResult> LocalDocumentationIndex::search(
    const QString &query, const QString &language, int maximumResults) const
{
    QList<DocumentationSearchResult> result;
    for (const DocumentationEntry &entry : entries) {
        const int score = scoreEntry(entry, query, language);
        if (score > 0)
            result.append({entry, score});
    }
    std::sort(result.begin(), result.end(),
              [](const DocumentationSearchResult &a,
                 const DocumentationSearchResult &b) {
        if (a.score != b.score) return a.score > b.score;
        return a.entry.id < b.entry.id;
    });
    if (maximumResults >= 0 && result.size() > maximumResults)
        result = result.mid(0, maximumResults);
    return result;
}

bool LocalDocumentationIndex::topic(const QString &id, const QString &language,
                                    DocumentationEntry *entry) const
{
    if (!entry) return false;
    const QString normalized = GT10CapabilityService::normalize(id);
    const DocumentationEntry *fallback = nullptr;
    for (const DocumentationEntry &candidate : entries) {
        if (GT10CapabilityService::normalize(candidate.id) != normalized)
            continue;
        if (languageMatches(candidate.language, language)) {
            *entry = candidate;
            return true;
        }
        if (!fallback) fallback = &candidate;
    }
    if (fallback) {
        *entry = *fallback;
        return true;
    }
    return false;
}

bool LocalDocumentationIndex::findByAlias(
    const QString &alias, const QString &language,
    DocumentationEntry *entry) const
{
    if (!entry) return false;
    const QString normalized = GT10CapabilityService::normalize(alias);
    for (const DocumentationEntry &candidate : entries) {
        if (!languageMatches(candidate.language, language)
            && !(language.startsWith("pt", Qt::CaseInsensitive)
                 && !candidate.answerPtBr.isEmpty()))
            continue;
        for (const QString &candidateAlias : candidate.aliases) {
            if (GT10CapabilityService::normalize(candidateAlias) == normalized) {
                *entry = candidate;
                return true;
            }
        }
    }
    return false;
}

QList<DocumentationEntry> LocalDocumentationIndex::relatedTopics(
    const QString &topicId, const QString &language) const
{
    QList<DocumentationEntry> result;
    const QString normalized = GT10CapabilityService::normalize(topicId);
    for (const DocumentationEntry &entry : entries) {
        if (!languageMatches(entry.language, language)
            && !(language.startsWith("pt", Qt::CaseInsensitive)
                 && !entry.answerPtBr.isEmpty()))
            continue;
        for (const QString &topic : entry.relatedTopics) {
            if (GT10CapabilityService::normalize(topic) == normalized) {
                result.append(entry);
                break;
            }
        }
    }
    return result;
}

QMap<QString, DocumentationCoverage>
LocalDocumentationIndex::coverageReport() const
{
    QMap<QString, DocumentationCoverage> result;
    for (const QString &topic : kCoverageTopics) {
        bool found = false;
        bool complete = false;
        const QString normalized = GT10CapabilityService::normalize(topic);
        for (const DocumentationEntry &entry : entries) {
            for (const QString &related : entry.relatedTopics) {
                if (GT10CapabilityService::normalize(related) != normalized)
                    continue;
                found = true;
                complete = complete || !entry.body.trimmed().isEmpty();
                break;
            }
        }
        if (!found) {
            result.insert(topic, DocumentationCoverage::Unavailable);
            continue;
        }
        result.insert(topic, complete ? DocumentationCoverage::Covered
                                      : DocumentationCoverage::Partial);
    }
    return result;
}

LocalKnowledgeAnswer LocalDocumentationIndex::answer(
    const QString &question, const QString &language) const
{
    LocalKnowledgeAnswer result;
    const QList<DocumentationSearchResult> matches = search(question, language, 1);
    if (matches.isEmpty())
        return result;
    const DocumentationEntry &entry = matches.first().entry;
    const bool portuguese = language.startsWith("pt", Qt::CaseInsensitive);
    if (!languageMatches(entry.language, language)
        && !(portuguese && !entry.answerPtBr.isEmpty())) {
        result.topicId = entry.id;
        result.metadata.insert("availableLanguage", entry.language);
        result.metadata.insert("documentationTrace", trace(entry));
        return result;
    }
    result.answered = true;
    result.confidence = portuguese && !languageMatches(entry.language, language)
        ? Confidence::Derived : Confidence::Authoritative;
    result.topicId = entry.id;
    if (portuguese && !entry.answerPtBr.isEmpty())
        result.text = entry.answerPtBr;
    else if (!entry.answerEn.isEmpty())
        result.text = entry.answerEn;
    else
        result.text = entry.body.left(700).trimmed();
    result.sourceKind = portuguese && !languageMatches(entry.language, language)
        ? SourceKind::DerivedFromOfficialDocumentation : entry.sourceKind;
    result.relatedBlocks = entry.relatedBlocks;
    result.metadata.insert("documentationTrace", trace(entry));
    return result;
}

LocalDocumentationIndex officialGT10DocumentationIndex()
{
    QList<DocumentationEntry> entries;
    const QStringList resources = {
        QStringLiteral(":/ai/knowledge/data/boss_gt10/owners_manual_index.json"),
        QStringLiteral(":/ai/knowledge/data/boss_gt10/patch_list_index.json"),
        QStringLiteral(":/ai/knowledge/data/boss_gt10/midi_implementation_index.json")
    };
    for (const QString &resource : resources)
        entries.append(loadIndex(resource));
    return LocalDocumentationIndex(entries);
}

} // namespace Knowledge
} // namespace Ai
