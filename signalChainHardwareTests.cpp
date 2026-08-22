#include "signalChainHardwareValidation.h"
#include "modernSignalChainMutationController.h"
#include "modernSignalChainSerializer.h"

#include <QCoreApplication>
#include <QDebug>

namespace {
QString spaced(const QList<QString> &bytes)
{
    return bytes.join(" ");
}

void printResult(const ChainHardwareTestResult &result)
{
    qInfo().noquote() << "TEST NAME:" << result.name;
    qInfo().noquote() << "PATCH:" << result.patchIdentity;
    qInfo().noquote() << "BEFORE:" << spaced(result.before);
    qInfo().noquote() << "REQUESTED:" << spaced(result.requested);
    qInfo().noquote() << "DT1:" << result.dt1;
    qInfo().noquote() << "READBACK:" << spaced(result.readback);
    qInfo().noquote() << "MATCH=" << (result.match ? "true" : "false");
    qInfo().noquote() << "ROLLBACK REQUESTED:"
                      << spaced(result.rollbackRequested);
    qInfo().noquote() << "ROLLBACK READBACK:"
                      << spaced(result.rollbackReadback);
    qInfo().noquote() << "ROLLBACK MATCH="
                      << (result.rollbackMatch ? "true" : "false");
    if (!result.error.isEmpty())
        qCritical().noquote() << "ERROR:" << result.error;
}
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    // Reuse the exact MIDI port preferences of the main editor. The harness
    // has a different executable name, so QStandardPaths would otherwise
    // resolve a separate empty preferences directory.
    QCoreApplication::setApplicationName("GT-10FxFloorBoard");
    SignalChainHardwareValidation validation;
    QString error;
    if (!validation.captureOriginal(&error)) {
        qCritical().noquote() << "SAFETY GATE FAILED:" << error;
        qCritical().noquote() << "No structural write was attempted.";
        return 2;
    }
    qInfo().noquote() << "PATCH NAME:" << validation.patchName();
    qInfo().noquote() << "PATCH IDENTITY:" << validation.patchIdentity();
    qInfo().noquote() << "FULL TEMP PATCH BACKUP:" << validation.backupPath();
    qInfo().noquote() << "ORIGINAL STRUCTURE:"
                      << spaced(validation.originalStructure());
    qInfo().noquote() << "TRANSACTION LOG:" << validation.logPath();

    if (application.arguments().contains("--capture-only")) {
        qInfo().noquote() << "CAPTURE-ONLY SAFETY GATE PASSED";
        qInfo().noquote() << "No structural write was attempted.";
        return 0;
    }

    const bool passed = validation.runApprovedTests(&error);
    for (const ChainHardwareTestResult &result : validation.results())
        printResult(result);
    if (!passed) {
        qCritical().noquote() << "HARDWARE VALIDATION STOPPED:" << error;
        return 3;
    }

    modernSignalChainModel::ChainSnapshot liveSnapshot;
    if (!modernSignalChainModel::parseRawBytes(
            validation.originalStructure(), &liveSnapshot, &error, 1,
            validation.patchIdentity())) {
        qCritical().noquote() << "LIVE TRANSACTION SETUP FAILED:" << error;
        return 4;
    }
    modernSignalChainModel liveModel;
    liveModel.replaceSnapshot(liveSnapshot, &error);
    modernSignalChainMutationController liveController(&liveModel);
    const ChainMoveResult liveMove = liveController.moveModule(
        0x08, modernSignalChainModel::ChainRegion::CommonPrefix,
        liveSnapshot.commonPrefix.size());
    if (!liveMove.accepted) {
        qCritical().noquote() << "LIVE TRANSACTION MOVE FAILED:"
                              << liveMove.error;
        return 5;
    }
    ChainLiveTransactionResult liveResult;
    if (!validation.executeLiveTransaction(
            validation.originalStructure(), liveMove.serializedBytes,
            &liveResult, &error)) {
        qCritical().noquote() << "LIVE TRANSACTION CONFIRM FAILED:" << error;
        return 6;
    }
    qInfo().noquote() << "LIVE UI TRANSACTION REQUESTED:"
                      << spaced(liveResult.requested);
    qInfo().noquote() << "LIVE UI TRANSACTION READBACK:"
                      << spaced(liveResult.readback);
    qInfo().noquote() << "LIVE UI TRANSACTION MATCH="
                      << (liveResult.match ? "true" : "false");

    ChainLiveTransactionResult returnResult;
    if (!validation.executeLiveTransaction(
            liveMove.serializedBytes, validation.originalStructure(),
            &returnResult, &error)) {
        qCritical().noquote() << "LIVE TRANSACTION RESTORE FAILED:" << error;
        return 7;
    }
    qInfo().noquote() << "LIVE UI RESTORE READBACK:"
                      << spaced(returnResult.readback);
    qInfo().noquote() << "LIVE UI RESTORE MATCH="
                      << (returnResult.match ? "true" : "false");
    qInfo().noquote() << "ALL APPROVED HARDWARE TESTS PASSED";
    qInfo().noquote() << "FINAL STRUCTURE RESTORED AND CONFIRMED:"
                      << spaced(validation.originalStructure());
    return 0;
}
