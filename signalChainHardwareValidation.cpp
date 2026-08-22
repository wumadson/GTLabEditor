#include "signalChainHardwareValidation.h"

#include "SysxIO.h"
#include "MidiTable.h"
#include "modernSignalChainMutationController.h"
#include "modernSignalChainSerializer.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QEventLoop>
#include <QFile>
#include <QTextStream>
#include <QTimer>
#include <QStringList>

namespace {
using Model = modernSignalChainModel;

QString spaced(const QList<QString> &bytes)
{
    return bytes.join(" ");
}

QByteArray hexToBytes(const QString &hex)
{
    return QByteArray::fromHex(hex.toLatin1());
}

bool isSimpleMovable(int moduleId)
{
    return moduleId == 0x04 || moduleId == 0x08
        || moduleId == 0x0C || moduleId == 0x0D;
}

quint32 rolandAddress(quint8 a, quint8 b, quint8 c, quint8 d)
{
    return (((quint32(a) * 0x80u) + b) * 0x80u + c) * 0x80u + d;
}

QList<Model::Entry> entriesForRegion(const Model::ChainSnapshot &snapshot,
                                     Model::ChainRegion region)
{
    switch (region) {
    case Model::ChainRegion::CommonPrefix: return snapshot.commonPrefix;
    case Model::ChainRegion::PathA: return snapshot.pathA;
    case Model::ChainRegion::PathB: return snapshot.pathB;
    case Model::ChainRegion::CommonSuffix: return snapshot.commonSuffix;
    }
    return QList<Model::Entry>();
}
}

SignalChainHardwareValidation::SignalChainHardwareValidation()
{
    const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz");
    savedBackupPath = "/tmp/gt10-chain-hardware-backup-" + stamp + ".syx";
    savedLogPath = "/tmp/gt10-chain-hardware-test-" + stamp + ".log";
}

bool SignalChainHardwareValidation::sendAndWait(
    const QString &sysx, QString *reply, int timeoutMs, QString *error) const
{
    SysxIO *io = SysxIO::Instance();
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    bool received = false;
    QString physicalReply;
    const QMetaObject::Connection connection = QObject::connect(
        io, &SysxIO::sysxReply, &loop, [&](const QString &value) {
            received = true;
            physicalReply = value;
            loop.quit();
        });
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    io->sendSysx(sysx);
    loop.exec();
    QObject::disconnect(connection);
    if (!received) {
        if (error)
            *error = QString("MIDI timeout after %1 ms").arg(timeoutMs);
        return false;
    }
    if (reply)
        *reply = physicalReply;
    if (error)
        error->clear();
    return true;
}

bool SignalChainHardwareValidation::parseTemporaryPatch(
    const QString &reply, PatchDump *dump, QString *error) const
{
    if (!dump) {
        if (error) *error = "Patch dump output is null";
        return false;
    }
    QString normalized = reply;
    normalized.remove(' ');
    normalized.remove('\n');
    normalized.remove('\r');
    normalized = normalized.toUpper();
    if (normalized.size() / 2 != 1784) {
        if (error)
            *error = QString("Temporary patch readback has %1 bytes; expected 1784")
                         .arg(normalized.size() / 2);
        return false;
    }

    QByteArray canonical = hexToBytes(normalized);
    QList<QString> structure;
    QString name;
    QStringList seenAddresses;
    int cursor = 0;
    bool structureFound = false;
    while (cursor < canonical.size()) {
        const int start = canonical.indexOf(char(0xF0), cursor);
        if (start < 0)
            break;
        const int end = canonical.indexOf(char(0xF7), start + 1);
        if (end < 0) {
            if (error) *error = "Unterminated SysEx frame in temporary patch";
            return false;
        }
        QByteArray frame = canonical.mid(start, end - start + 1);
        cursor = end + 1;
        if (frame.size() < 13
            || quint8(frame.at(1)) != 0x41
            || quint8(frame.at(5)) != 0x2F
            || quint8(frame.at(6)) != 0x12)
            continue;

        seenAddresses.append(QString("%1 %2 %3 %4 (%5 data)")
            .arg(quint8(frame.at(7)), 2, 16, QChar('0'))
            .arg(quint8(frame.at(8)), 2, 16, QChar('0'))
            .arg(quint8(frame.at(9)), 2, 16, QChar('0'))
            .arg(quint8(frame.at(10)), 2, 16, QChar('0'))
            .arg(frame.size() - 13).toUpper());

        int checksumSum = 0;
        for (int i = 7; i < frame.size() - 1; ++i)
            checksumSum += quint8(frame.at(i));
        if ((checksumSum & 0x7F) != 0) {
            if (error) *error = "Invalid Roland checksum in temporary patch readback";
            return false;
        }

        const quint32 frameAddress = rolandAddress(
            quint8(frame.at(7)), quint8(frame.at(8)),
            quint8(frame.at(9)), quint8(frame.at(10)));
        const int dataCount = frame.size() - 13;
        const quint32 patchHeadAddress = rolandAddress(0x60, 0x00, 0x00, 0x00);
        const quint32 structureAddress = rolandAddress(0x60, 0x00, 0x0B, 0x00);
        const bool containsPatchHead = patchHeadAddress >= frameAddress
            && patchHeadAddress + 16 <= frameAddress + quint32(dataCount);
        const bool containsStructure = structureAddress >= frameAddress
            && structureAddress + 18 <= frameAddress + quint32(dataCount);
        if (containsPatchHead && name.isEmpty()) {
            const int dataOffset = int(patchHeadAddress - frameAddress);
            for (int i = 0; i < 16; ++i)
                name.append(QChar(quint8(frame.at(11 + dataOffset + i))));
            name = name.trimmed();
        }
        if (containsStructure) {
            const int dataOffset = int(structureAddress - frameAddress);
            for (int i = 0; i < 18; ++i)
                structure.append(QString("%1").arg(
                    quint8(frame.at(11 + dataOffset + i)), 2, 16,
                                                    QChar('0')).toUpper());
            for (int i = 0; i < 18; ++i)
                canonical[start + 11 + dataOffset + i] = char(0);
            canonical[start + frame.size() - 2] = char(0);
            structureFound = true;
        }
    }
    if (!structureFound || structure.size() != 18) {
        if (error)
            *error = "Structure 60 00 0B 00 was not found; received frames: "
                + seenAddresses.join(", ");
        return false;
    }

    dump->rawHex = normalized;
    dump->structure = structure;
    dump->name = name;
    dump->identity = QString::fromLatin1(
        QCryptographicHash::hash(canonical, QCryptographicHash::Sha256).toHex());
    if (error)
        error->clear();
    return true;
}

bool SignalChainHardwareValidation::requestTemporaryPatch(PatchDump *dump,
                                                           QString *error)
{
    QString reply;
    const QString rq1 = MidiTable::Instance()->patchRequest(0, 0);
    if (!sendAndWait(rq1, &reply, 15000, error))
        return false;
    if (reply.isEmpty()) {
        if (error) *error = "GT-10 returned an empty temporary patch response";
        return false;
    }
    return parseTemporaryPatch(reply, dump, error);
}

bool SignalChainHardwareValidation::captureOriginal(QString *error)
{
    if (!requestTemporaryPatch(&originalDump, error))
        return false;
    if (!Model::parseRawBytes(originalDump.structure, &originalSnapshot, error, 1,
                              originalDump.identity))
        return false;

    QFile backup(savedBackupPath);
    if (!backup.open(QIODevice::WriteOnly)
        || backup.write(hexToBytes(originalDump.rawHex)) != originalDump.rawHex.size() / 2) {
        if (error) *error = "Unable to save complete temporary patch backup: " + savedBackupPath;
        return false;
    }
    backup.close();
    appendLog("PATCH=" + originalDump.name + " identity=" + originalDump.identity);
    appendLog("BACKUP=" + savedBackupPath);
    appendLog("ORIGINAL=" + spaced(originalDump.structure));
    return true;
}

bool SignalChainHardwareValidation::executeLiveTransaction(
    const QList<QString> &before, const QList<QString> &requested,
    ChainLiveTransactionResult *result, QString *error)
{
    if (!result) {
        if (error) *error = "Live transaction output is null";
        return false;
    }
    *result = ChainLiveTransactionResult();
    result->before = before;
    result->requested = requested;
    if (before.size() != 18 || requested.size() != 18) {
        result->error = "Live transaction requires two 18-byte structures";
        if (error) *error = result->error;
        return false;
    }

    PatchDump preflight;
    QString failure;
    if (!requestTemporaryPatch(&preflight, &failure)) {
        result->error = "Preflight readback failed: " + failure;
        if (error) *error = result->error;
        return false;
    }
    result->patchIdentity = preflight.identity;
    if (preflight.structure != before) {
        result->readback = preflight.structure;
        result->error = "Local chain snapshot is stale; no Structure write was sent";
        if (error) *error = result->error;
        return false;
    }

    const QString dt1 = buildStructureDt1(requested, &failure);
    QString ignoredReply;
    const bool writeCompleted = !dt1.isEmpty()
        && sendAndWait(dt1, &ignoredReply, 5000, &failure);

    PatchDump physical;
    if (!requestTemporaryPatch(&physical, &failure)) {
        result->error = "Physical confirmation failed; rollback was blocked because the current patch could not be identified: "
            + failure;
        if (error) *error = result->error;
        return false;
    }
    result->readback = physical.structure;
    if (physical.identity != preflight.identity) {
        result->patchChanged = true;
        result->error = "Patch changed during transaction; rollback intentionally blocked";
        if (error) *error = result->error;
        return false;
    }

    result->match = writeCompleted && physical.structure == requested;
    if (result->match) {
        if (error) error->clear();
        return true;
    }

    result->rollbackAttempted = true;
    QString rollbackError;
    const QString rollbackDt1 = buildStructureDt1(before, &rollbackError);
    QString rollbackReply;
    const bool rollbackWrite = !rollbackDt1.isEmpty()
        && sendAndWait(rollbackDt1, &rollbackReply, 5000, &rollbackError);
    PatchDump restored;
    const bool rollbackRead = rollbackWrite
        && requestTemporaryPatch(&restored, &rollbackError);
    if (rollbackRead)
        result->rollbackReadback = restored.structure;
    result->rollbackMatch = rollbackRead
        && restored.identity == preflight.identity
        && restored.structure == before;
    result->error = result->rollbackMatch
        ? "Hardware readback differed from the requested structure; rollback confirmed"
        : "Hardware readback differed and rollback could not be confirmed: "
            + rollbackError;
    if (error) *error = result->error;
    return false;
}

QString SignalChainHardwareValidation::buildStructureDt1(
    const QList<QString> &bytes, QString *error) const
{
    if (bytes.size() != 18) {
        if (error) *error = "DT1 requires exactly 18 structure bytes";
        return QString();
    }
    QList<int> body = {0x60, 0x00, 0x0B, 0x00};
    QString message = "F0410000002F12";
    for (int value : body)
        message.append(QString("%1").arg(value, 2, 16, QChar('0')).toUpper());
    int checksumSum = 0x60 + 0x0B;
    for (const QString &byte : bytes) {
        bool ok = false;
        const int value = byte.toInt(&ok, 16);
        if (!ok || value < 0 || value > 0x7F) {
            if (error) *error = "Invalid 7-bit structure byte: " + byte;
            return QString();
        }
        checksumSum += value;
        message.append(QString("%1").arg(value, 2, 16, QChar('0')).toUpper());
    }
    const int checksum = (0x80 - (checksumSum & 0x7F)) & 0x7F;
    message.append(QString("%1").arg(checksum, 2, 16, QChar('0')).toUpper());
    message.append("F7");
    if (error)
        error->clear();
    return message;
}

bool SignalChainHardwareValidation::restoreOriginal(
    const PatchDump *currentReadback, ChainHardwareTestResult *result, QString *error)
{
    if (!currentReadback || currentReadback->identity != originalDump.identity) {
        if (error)
            *error = "Rollback blocked: current hardware patch identity is not confirmed";
        return false;
    }
    result->rollbackRequested = originalDump.structure;
    QString buildError;
    const QString rollbackDt1 = buildStructureDt1(originalDump.structure, &buildError);
    QString ignoredReply;
    if (rollbackDt1.isEmpty()
        || !sendAndWait(rollbackDt1, &ignoredReply, 5000, &buildError)) {
        if (error) *error = "Rollback write failed: " + buildError;
        return false;
    }
    PatchDump rollback;
    if (!requestTemporaryPatch(&rollback, &buildError)) {
        if (error) *error = "Rollback readback failed: " + buildError;
        return false;
    }
    result->rollbackReadback = rollback.structure;
    result->rollbackMatch = rollback.identity == originalDump.identity
        && rollback.structure == originalDump.structure;
    if (!result->rollbackMatch) {
        if (error) *error = "Rollback readback does not match the original snapshot";
        return false;
    }
    return true;
}

bool SignalChainHardwareValidation::performChainWriteTest(
    const QString &name, const Model::ChainSnapshot &after, QString *error)
{
    if (transactionActive) {
        if (error) *error = "Another chain transaction is active";
        return false;
    }
    transactionActive = true;
    ChainHardwareTestResult result;
    result.name = name;
    result.patchIdentity = originalDump.identity;
    result.before = originalDump.structure;

    QString failure;
    if (!modernSignalChainSerializer::serialize(after, &result.requested, &failure)) {
        result.error = failure;
        testResults.append(result);
        logResult(result);
        transactionActive = false;
        if (error) *error = failure;
        return false;
    }
    result.dt1 = buildStructureDt1(result.requested, &failure);
    QString writeReply;
    bool wrote = !result.dt1.isEmpty()
        && sendAndWait(result.dt1, &writeReply, 5000, &failure);

    PatchDump readback;
    bool hasReadback = false;
    if (wrote) {
        hasReadback = requestTemporaryPatch(&readback, &failure);
    } else {
        // A DT1 completion timeout is ambiguous. Read the hardware before any
        // rollback attempt so a newly selected patch can never be overwritten.
        hasReadback = requestTemporaryPatch(&readback, &failure);
    }
    if (hasReadback) {
        result.readback = readback.structure;
        if (readback.identity != originalDump.identity) {
            result.error = "Patch changed during transaction; rollback intentionally blocked";
        } else {
            result.match = wrote && readback.structure == result.requested;
            if (!result.match && failure.isEmpty())
                failure = "Hardware readback differs from requested structure";
            QString rollbackError;
            if (!restoreOriginal(&readback, &result, &rollbackError)) {
                result.error = rollbackError;
                failure = rollbackError;
            }
        }
    } else {
        result.error = "No physical readback; rollback blocked to protect a possible new patch: "
            + failure;
    }
    if (result.error.isEmpty() && !result.match)
        result.error = failure;

    testResults.append(result);
    logResult(result);
    transactionActive = false;
    const bool success = result.match && result.rollbackMatch;
    if (!success && error)
        *error = result.error;
    return success;
}

bool SignalChainHardwareValidation::buildMove(
    int moduleId, Model::ChainRegion region, int index,
    Model::ChainSnapshot *after, QString *error) const
{
    Model model;
    if (!model.replaceSnapshot(originalSnapshot, error))
        return false;
    modernSignalChainMutationController controller(&model);
    const ChainMoveResult move = controller.moveModule(moduleId, region, index);
    if (!move.accepted) {
        if (error) *error = move.error;
        return false;
    }
    *after = move.after;
    return true;
}

bool SignalChainHardwareValidation::chooseCommonModule(
    int *moduleId, Model::ChainRegion *region, int *index, QString *error) const
{
    const Model::ChainRegion common[] = {
        Model::ChainRegion::CommonPrefix, Model::ChainRegion::CommonSuffix
    };
    const int preferred[] = {0x04, 0x08, 0x0C, 0x0D};
    for (int candidate : preferred) {
        for (Model::ChainRegion candidateRegion : common) {
            const QList<Model::Entry> entries = entriesForRegion(originalSnapshot,
                                                                  candidateRegion);
            for (int i = 0; i < entries.size(); ++i) {
                if (entries.at(i).moduleId == candidate) {
                    *moduleId = candidate;
                    *region = candidateRegion;
                    *index = i;
                    return true;
                }
            }
        }
    }
    if (error) *error = "No EQ/CHORUS/NS module is currently in Common";
    return false;
}

bool SignalChainHardwareValidation::choosePathBModule(int *moduleId,
                                                       QString *error) const
{
    for (const Model::Entry &entry : originalSnapshot.pathB) {
        if (entry.moduleId != 0x03 && isSimpleMovable(entry.moduleId)) {
            *moduleId = entry.moduleId;
            return true;
        }
    }
    if (error) *error = "No simple movable module is currently in Path B";
    return false;
}

bool SignalChainHardwareValidation::runApprovedTests(QString *error)
{
    if (originalDump.structure.size() != 18) {
        if (error) *error = "Original hardware snapshot has not been captured";
        return false;
    }
    if (!performChainWriteTest("TEST 0 - ROUNDTRIP", originalSnapshot, error))
        return false;

    int commonId = -1;
    int commonIndex = -1;
    Model::ChainRegion commonRegion = Model::ChainRegion::CommonSuffix;
    if (!chooseCommonModule(&commonId, &commonRegion, &commonIndex, error))
        return false;
    const QList<Model::Entry> commonEntries = entriesForRegion(originalSnapshot,
                                                                commonRegion);
    if (commonEntries.size() < 2) {
        if (error) *error = "Selected Common region has no adjacent move position";
        return false;
    }
    const int adjacentDestination = commonIndex < commonEntries.size() - 1
        ? commonIndex + 2 : commonIndex - 1;
    Model::ChainSnapshot moved;
    if (!buildMove(commonId, commonRegion, adjacentDestination, &moved, error)
        || !performChainWriteTest("TEST 1 - MOVE WITHIN COMMON", moved, error))
        return false;

    if (!buildMove(commonId, Model::ChainRegion::PathA,
                   originalSnapshot.pathA.size(), &moved, error)
        || !performChainWriteTest("TEST 2 - COMMON TO PATH A", moved, error))
        return false;
    if (!buildMove(commonId, Model::ChainRegion::PathB,
                   originalSnapshot.pathB.size(), &moved, error)
        || !performChainWriteTest("TEST 3 - COMMON TO PATH B", moved, error))
        return false;

    if (!buildMove(0x08, Model::ChainRegion::CommonPrefix,
                   originalSnapshot.commonPrefix.size(), &moved, error)
        || !performChainWriteTest("TEST 4 - CHORUS TO COMMON PREFIX", moved,
                                  error))
        return false;
    if (!buildMove(0x0E, Model::ChainRegion::CommonSuffix,
                   originalSnapshot.commonSuffix.size(), &moved, error)
        || !performChainWriteTest("TEST 5 - S/R TO COMMON SUFFIX", moved,
                                  error))
        return false;
    if (!buildMove(0x0F, Model::ChainRegion::PathA,
                   originalSnapshot.pathA.size(), &moved, error)
        || !performChainWriteTest("TEST 6 - D.OUT TO PATH A", moved, error))
        return false;

    int pathBId = -1;
    QString pathBError;
    if (!choosePathBModule(&pathBId, &pathBError)) {
        appendLog("TEST 7 - PATH B TO COMMON=SKIPPED: " + pathBError);
        if (error)
            error->clear();
        return true;
    }
    if (!buildMove(pathBId, Model::ChainRegion::CommonSuffix,
                   originalSnapshot.commonSuffix.size(), &moved, error)
        || !performChainWriteTest("TEST 7 - PATH B TO COMMON", moved, error))
        return false;
    return true;
}

void SignalChainHardwareValidation::appendLog(const QString &line) const
{
    QFile file(savedLogPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;
    QTextStream stream(&file);
    stream << line << '\n';
}

void SignalChainHardwareValidation::logResult(
    const ChainHardwareTestResult &result) const
{
    appendLog("TEST NAME=" + result.name);
    appendLog("PATCH=" + result.patchIdentity);
    appendLog("BEFORE=" + spaced(result.before));
    appendLog("REQUESTED=" + spaced(result.requested));
    appendLog("DT1=" + result.dt1);
    appendLog("READBACK=" + spaced(result.readback));
    appendLog(QString("MATCH=%1").arg(result.match ? "true" : "false"));
    appendLog("ROLLBACK REQUESTED=" + spaced(result.rollbackRequested));
    appendLog("ROLLBACK READBACK=" + spaced(result.rollbackReadback));
    appendLog(QString("ROLLBACK MATCH=%1").arg(result.rollbackMatch ? "true" : "false"));
    if (!result.error.isEmpty())
        appendLog("ERROR=" + result.error);
    appendLog(QString());
}

QString SignalChainHardwareValidation::backupPath() const { return savedBackupPath; }
QString SignalChainHardwareValidation::logPath() const { return savedLogPath; }
QString SignalChainHardwareValidation::patchIdentity() const { return originalDump.identity; }
QString SignalChainHardwareValidation::patchName() const { return originalDump.name; }
QList<QString> SignalChainHardwareValidation::originalStructure() const
{ return originalDump.structure; }
QList<ChainHardwareTestResult> SignalChainHardwareValidation::results() const
{ return testResults; }
