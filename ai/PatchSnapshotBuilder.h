#ifndef AI_PATCHSNAPSHOTBUILDER_H
#define AI_PATCHSNAPSHOTBUILDER_H

#include "ai/PatchCapture.h"
#include "ai/PatchSnapshot.h"

namespace Ai {

struct PatchSnapshotBuildResult
{
    bool ok = false;
    QString error;
    PatchSnapshot snapshot;
};

class PatchSnapshotBuilder
{
public:
    PatchSnapshotBuildResult build(const PatchCapture &capture) const;
};

} // namespace Ai

#endif // AI_PATCHSNAPSHOTBUILDER_H
