#ifndef AI_KNOWLEDGE_GT10CAPABILITYSERVICE_H
#define AI_KNOWLEDGE_GT10CAPABILITYSERVICE_H

#include "ai/knowledge/KnowledgeModel.h"

#include <QList>

namespace Ai {
namespace Knowledge {

class GT10CapabilityService
{
public:
    QString deviceModel() const;
    QStringList blockIds() const;
    bool hasBlock(const QString &blockId) const;
    QStringList effectTypesForBlock(const QString &blockId) const;
    QStringList blocksForEffectType(const QString &effectType) const;
    bool hasEffectType(const QString &blockId,
                       const QString &effectType) const;
    QStringList parametersForEffect(const QString &blockId,
                                    const QString &effectType) const;
    ParameterCapability parameterCapability(const QString &blockId,
                                            const QString &effectType,
                                            const QString &parameter) const;

    int assignCount() const;
    QStringList assignTargets() const;
    QStringList assignSources() const;
    QStringList assignSourceModes() const;
    bool supportsAssignTarget(const QString &target) const;
    bool supportsAssignSource(const QString &source) const;
    QList<ControllerCapability> controllers() const;
    ControllerCapability controller(const QString &idOrAlias) const;

    static QString normalize(QString text);
    static QString scopeName(DataScope scope);

private:
    struct CatalogAddress {
        QString block;
        QString offset;
    };

    CatalogAddress typeAddress(const QString &blockId) const;
    QStringList valuesAt(const CatalogAddress &address) const;
    QString parameterLabel(const QString &block,
                           const QString &offset) const;
};

} // namespace Knowledge
} // namespace Ai

#endif
