#include "backupCoordinator.h"

#include "SysxIO.h"
#include "floorBoard.h"
#include "patchBackupCodec.h"

#include <QSaveFile>

BackupCoordinator::BackupCoordinator(floorBoard *backend, QObject *parent)
    : QObject(parent), backend(backend)
{
    timeoutTimer.setSingleShot(true);
    timeoutTimer.setInterval(6000);
    connect(&timeoutTimer, &QTimer::timeout,
            this, &BackupCoordinator::operationTimedOut);
}

bool BackupCoordinator::begin(Operation operation)
{
    SysxIO *sysxIO = SysxIO::Instance();
    if (currentOperation != Idle || !backend
        || !backend->canStartExclusiveMemoryOperation()
        || !sysxIO->isConnected() || !sysxIO->deviceReady())
        return false;

    currentOperation = operation;
    patchIndex = 0;
    attempt = 0;
    cancelRequested = false;
    retryPending = false;
    elapsed.start();
    backend->setExclusiveMemoryOperation(true);
    sysxIO->setDeviceReady(false);
    connect(sysxIO, SIGNAL(sysxReply(QString)),
            this, SLOT(replyReceived(QString)), Qt::UniqueConnection);
    return true;
}

bool BackupCoordinator::startUserBackup(const QString &filePath)
{
    if (filePath.isEmpty() || !begin(UserBackup))
        return false;
    backupFilePath = filePath;
    patches.clear();
    patches.reserve(PatchCount);
    currentStage = Reading;
    issueCurrentStep();
    return true;
}

bool BackupCoordinator::startUserRestore(const QVector<DecodedPatch> &source)
{
    if (source.size() != PatchCount || !begin(UserRestore))
        return false;
    patches = source;
    currentStage = Writing;
    issueCurrentStep();
    return true;
}

void BackupCoordinator::cancel()
{
    if (currentOperation == Idle)
        return;
    cancelRequested = true;
    emit progressChanged(currentPatchNumber(), patchIndex, PatchCount,
                         tr("CANCELING"),
                         tr("Waiting for the current transfer to finish safely…"));
}

int BackupCoordinator::currentBank() const
{
    return patchIndex / 4 + 1;
}

int BackupCoordinator::currentPatch() const
{
    return patchIndex % 4 + 1;
}

QString BackupCoordinator::currentPatchNumber() const
{
    return QString("U%1-%2").arg(currentBank(), 2, 10, QChar('0'))
        .arg(currentPatch());
}

void BackupCoordinator::issueCurrentStep()
{
    if (cancelRequested) {
        finish(Canceled,
               currentOperation == UserBackup ? tr("BACKUP CANCELED")
                                              : tr("RESTORE CANCELED"),
               currentOperation == UserBackup
                   ? tr("No backup file was written.")
                   : tr("%1 patch(es) were verified before cancellation.")
                         .arg(patchIndex));
        return;
    }
    if (currentOperation == UserBackup)
        issueBackupRead();
    else if (currentStage == Writing)
        issueRestoreWrite();
    else
        issueRestoreVerify();
}

void BackupCoordinator::issueBackupRead()
{
    SysxIO *sysxIO = SysxIO::Instance();
    sysxIO->setDeviceReady(false);
    emit progressChanged(currentPatchNumber(), patchIndex, PatchCount,
                         attempt ? tr("RETRYING") : tr("READING"),
                         tr("Reading patch… attempt %1/%2")
                             .arg(attempt + 1).arg(MaximumAttempts));
    timeoutTimer.start();
    sysxIO->requestPatch(currentBank(), currentPatch());
}

void BackupCoordinator::issueRestoreWrite()
{
    QString error;
    const QString message = PatchTransferCodec::buildUserWriteMessage00To0C(
        patches.at(patchIndex), currentBank(), currentPatch(), &error);
    if (message.isEmpty()) {
        finish(Failed, tr("RESTORE INCOMPLETE"), error);
        return;
    }
    SysxIO *sysxIO = SysxIO::Instance();
    sysxIO->setDeviceReady(false);
    emit progressChanged(currentPatchNumber(), patchIndex, PatchCount,
                         attempt ? tr("RETRYING") : tr("WRITING"),
                         tr("Writing patch… attempt %1/%2")
                             .arg(attempt + 1).arg(MaximumAttempts));
    timeoutTimer.start();
    sysxIO->sendSysx(message);
}

void BackupCoordinator::issueRestoreVerify()
{
    SysxIO *sysxIO = SysxIO::Instance();
    sysxIO->setDeviceReady(false);
    emit progressChanged(currentPatchNumber(), patchIndex, PatchCount,
                         tr("VERIFYING"), tr("Reading destination for comparison…"));
    timeoutTimer.start();
    sysxIO->requestPatch(currentBank(), currentPatch());
}

void BackupCoordinator::replyReceived(QString reply)
{
    if (currentOperation == Idle || retryPending)
        return;
    timeoutTimer.stop();
    if (cancelRequested) {
        issueCurrentStep();
        return;
    }

    if (currentOperation == UserBackup) {
        const DecodedPatch decoded = PatchBackupCodec::decodeDeviceReply(reply);
        if (!decoded.valid) {
            handleFailure(decoded.error);
            return;
        }
        patches.append(decoded);
        advancePatch();
        return;
    }

    if (currentStage == Writing) {
        currentStage = Verifying;
        issueCurrentStep();
        return;
    }

    const DecodedPatch decoded = PatchBackupCodec::decodeDeviceReply(reply);
    if (!decoded.valid) {
        handleFailure(decoded.error);
        return;
    }
    if (decoded.logicalBlocks00To0C
        != patches.at(patchIndex).logicalBlocks00To0C) {
        handleFailure(tr("Readback differs from the source backup."));
        return;
    }
    emit patchVerified(currentBank(), currentPatch(), decoded.verifiedName);
    advancePatch();
}

void BackupCoordinator::operationTimedOut()
{
    if (currentOperation != Idle)
        handleFailure(tr("The GT-10 did not complete the transfer in time."));
}

void BackupCoordinator::handleFailure(const QString &reason)
{
    ++attempt;
    if (cancelRequested) {
        issueCurrentStep();
        return;
    }
    if (attempt >= MaximumAttempts) {
        const QString detail = currentOperation == UserRestore
            ? tr("Verified: %1\nFailed: %2\nRemaining: %3\n\n%4")
                  .arg(patchIndex).arg(currentPatchNumber())
                  .arg(PatchCount - patchIndex).arg(reason)
            : tr("Backup stopped at %1 after %2 attempts. No file was written.\n\n%3")
                  .arg(currentPatchNumber()).arg(MaximumAttempts).arg(reason);
        finish(Failed,
               currentOperation == UserRestore ? tr("RESTORE INCOMPLETE")
                                               : tr("BACKUP FAILED"),
               detail);
        return;
    }
    if (currentOperation == UserRestore)
        currentStage = Writing;
    retryPending = true;
    QTimer::singleShot(150, this, [this]() {
        retryPending = false;
        issueCurrentStep();
    });
}

void BackupCoordinator::advancePatch()
{
    ++patchIndex;
    attempt = 0;
    if (patchIndex < PatchCount) {
        if (currentOperation == UserRestore)
            currentStage = Writing;
        issueCurrentStep();
        return;
    }

    if (currentOperation == UserBackup) {
        QString error;
        const QByteArray data = PatchBackupCodec::serialize(patches, &error);
        QSaveFile file(backupFilePath);
        if (data.isEmpty() || !file.open(QIODevice::WriteOnly)
            || file.write(data) != data.size() || !file.commit()) {
            finish(Failed, tr("BACKUP FAILED"),
                   error.isEmpty() ? tr("The backup file could not be written atomically.")
                                   : error);
            return;
        }
        finish(Complete, tr("BACKUP COMPLETE"),
               tr("200 patches saved to:\n%1\n\nDuration: %2 s")
                   .arg(backupFilePath).arg(elapsed.elapsed() / 1000.0, 0, 'f', 1));
    } else {
        finish(Complete, tr("RESTORE COMPLETE"),
               tr("200 patches were written and verified.\n\n"
                  "The currently loaded patch may differ from restored User memory. "
                  "Reload the patch to synchronize."));
    }
}

void BackupCoordinator::finish(Result result, const QString &title,
                               const QString &detail)
{
    const Operation completedOperation = currentOperation;
    timeoutTimer.stop();
    disconnect(SysxIO::Instance(), SIGNAL(sysxReply(QString)),
               this, SLOT(replyReceived(QString)));
    currentOperation = Idle;
    backend->setExclusiveMemoryOperation(false);
    SysxIO::Instance()->setDeviceReady(true);
    patches.clear();
    backupFilePath.clear();
    cancelRequested = false;
    retryPending = false;
    emit finished(result, title,
                  completedOperation == UserRestore && result != Complete
                      ? detail + tr("\n\nPreviously verified patches remain overwritten.")
                      : detail);
}
