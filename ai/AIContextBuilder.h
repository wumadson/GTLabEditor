#ifndef AI_AICONTEXTBUILDER_H
#define AI_AICONTEXTBUILDER_H

#include "ai/PatchSnapshot.h"

#include <QByteArray>
#include <QJsonObject>

namespace Ai {

enum class AIContextDetail {
    Compact,
    Detailed
};

struct AIContext
{
    AIContextDetail detail = AIContextDetail::Compact;
    QJsonObject root;
};

class AIContextBuilder
{
public:
    AIContext build(const PatchSnapshot &snapshot,
                    AIContextDetail detail) const;

    static QByteArray serialize(const AIContext &context,
                                bool indented = true);
    static QString toText(const AIContext &context);
    static QString normalizeLabel(const QString &label);
};

} // namespace Ai

#endif // AI_AICONTEXTBUILDER_H
