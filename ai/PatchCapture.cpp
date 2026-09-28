#include "ai/PatchCapture.h"

namespace Ai {

PatchCapture::PatchCapture(const SysxData &fileSource,
                           const QString &patchName,
                           int loadedBank,
                           int loadedPatch,
                           PatchProvenance provenance,
                           bool connected,
                           bool dirty,
                           bool deviceConfirmed)
    : capturedFileSource(fileSource),
      capturedPatchName(patchName),
      capturedLoadedBank(loadedBank),
      capturedLoadedPatch(loadedPatch),
      capturedProvenance(provenance),
      capturedConnected(connected),
      capturedDirty(dirty),
      capturedDeviceConfirmed(deviceConfirmed)
{
}

const SysxData &PatchCapture::fileSource() const { return capturedFileSource; }
QString PatchCapture::patchName() const { return capturedPatchName; }
int PatchCapture::loadedBank() const { return capturedLoadedBank; }
int PatchCapture::loadedPatch() const { return capturedLoadedPatch; }
PatchProvenance PatchCapture::provenance() const { return capturedProvenance; }
bool PatchCapture::connected() const { return capturedConnected; }
bool PatchCapture::dirty() const { return capturedDirty; }
bool PatchCapture::deviceConfirmed() const { return capturedDeviceConfirmed; }

} // namespace Ai
