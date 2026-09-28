#ifndef AI_PATCHJSONSERIALIZER_H
#define AI_PATCHJSONSERIALIZER_H

#include "ai/PatchSnapshot.h"

#include <QByteArray>
#include <QJsonObject>

namespace Ai {

class PatchJsonSerializer
{
public:
    static QJsonObject toJson(const PatchSnapshot &snapshot);
    static QByteArray serialize(const PatchSnapshot &snapshot,
                                bool indented = true);
};

} // namespace Ai

#endif // AI_PATCHJSONSERIALIZER_H
