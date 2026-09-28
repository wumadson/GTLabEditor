#ifndef AI_PATCHCAPTURE_H
#define AI_PATCHCAPTURE_H

#include "SysxIO.h"

#include <QString>

namespace Ai {

enum class PatchProvenance {
    DeviceTemporaryBuffer,
    ImportedTemporaryBuffer,
    Unknown
};

class PatchCapture
{
public:
    PatchCapture(const SysxData &fileSource,
                 const QString &patchName,
                 int loadedBank,
                 int loadedPatch,
                 PatchProvenance provenance,
                 bool connected,
                 bool dirty,
                 bool deviceConfirmed);

    const SysxData &fileSource() const;
    QString patchName() const;
    int loadedBank() const;
    int loadedPatch() const;
    PatchProvenance provenance() const;
    bool connected() const;
    bool dirty() const;
    bool deviceConfirmed() const;

private:
    const SysxData capturedFileSource;
    const QString capturedPatchName;
    const int capturedLoadedBank;
    const int capturedLoadedPatch;
    const PatchProvenance capturedProvenance;
    const bool capturedConnected;
    const bool capturedDirty;
    const bool capturedDeviceConfirmed;
};

} // namespace Ai

#endif // AI_PATCHCAPTURE_H
