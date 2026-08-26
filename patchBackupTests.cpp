#include <QCoreApplication>
#include <QFile>
#include <QVector>

#include "globalVariables.h"
#include "patchBackupCodec.h"
#include "patchTransferCodec.h"

namespace {

QString fixtureReply()
{
    QFile file("gt-10 temp patch_pathA.syx");
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromLatin1(file.readAll().toHex()).toUpper();
}

QVector<QByteArray> splitMessages(const QString &stream)
{
    const QByteArray data = QByteArray::fromHex(stream.toLatin1());
    QVector<QByteArray> result;
    int offset = 0;
    while (offset < data.size()) {
        const int end = data.indexOf(char(0xF7), offset);
        if (end < 0)
            return QVector<QByteArray>();
        result.append(data.mid(offset, end - offset + 1));
        offset = end + 1;
    }
    return result;
}

bool verifyDestination(const DecodedPatch &source, int bank, int patch)
{
    QString error;
    const QString stream = PatchTransferCodec::buildUserWriteMessage00To0C(
        source, bank, patch, &error);
    const QVector<QByteArray> messages = splitMessages(stream);
    if (!error.isEmpty() || messages.size() != 13) {
        qCritical("U%02d-%d did not produce exactly 13 frames: %s",
                  bank, patch, qPrintable(error));
        return false;
    }
    const int patchOffset = (bank - 1) * patchPerBank + patch - 1;
    const int expectedPage = 0x10 + patchOffset / 0x80;
    const int expectedSlot = patchOffset % 0x80;
    for (int block = 0; block < messages.size(); ++block) {
        const QByteArray &message = messages.at(block);
        const int page = static_cast<unsigned char>(message.at(sysxAddressOffset));
        const int slot = static_cast<unsigned char>(message.at(sysxAddressOffset + 1));
        const int logicalBlock =
            static_cast<unsigned char>(message.at(sysxAddressOffset + 2));
        if (page != expectedPage || slot != expectedSlot
            || logicalBlock != block
            || !PatchBackupCodec::validateMessageChecksum(message)) {
            qCritical("Invalid destination/order/checksum for U%02d-%d block %02X",
                      bank, patch, block);
            return false;
        }
        const QString key = QString("%1%2")
            .arg(block, 2, 16, QChar('0')).arg(0, 2, 16, QChar('0')).toUpper();
        const QString actualPayload = QString::fromLatin1(
            message.mid(sysxDataOffset, message.size() - sysxDataOffset - 2)
                .toHex()).toUpper();
        if (actualPayload != source.logicalBlocks00To0C.value(key)) {
            qCritical("Payload changed for U%02d-%d block %02X", bank, patch, block);
            return false;
        }
    }
    return true;
}

bool expectRejected(const QByteArray &data, const char *description)
{
    QString error;
    if (!PatchBackupCodec::parse(data, &error).isEmpty()) {
        qCritical("Invalid fixture accepted: %s", description);
        return false;
    }
    qInfo("PASS rejected %s: %s", description, qPrintable(error));
    return true;
}

}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    bool passed = true;
    const QString reply = fixtureReply();
    DecodedPatch source = PatchBackupCodec::decodeDeviceReply(reply);
    if (!source.valid) {
        qCritical("Device-reply validation failed: %s", qPrintable(source.error));
        return 1;
    }

    passed &= verifyDestination(source, 1, 1);
    passed &= verifyDestination(source, 1, 4);
    passed &= verifyDestination(source, 25, 2);
    passed &= verifyDestination(source, 50, 4);
    QString presetError;
    passed &= PatchTransferCodec::buildUserWriteMessage00To0C(
        source, 51, 1, &presetError).isEmpty();

    QVector<DecodedPatch> patches;
    patches.reserve(bankTotalUser * patchPerBank);
    for (int index = 0; index < bankTotalUser * patchPerBank; ++index) {
        const int bank = index / patchPerBank + 1;
        const int patch = index % patchPerBank + 1;
        const QString stream = PatchTransferCodec::buildUserWriteMessage00To0C(
            source, bank, patch);
        DecodedPatch stored = source;
        stored.messages00To0C.clear();
        for (const QByteArray &message : splitMessages(stream))
            stored.messages00To0C.append(
                QString::fromLatin1(message.toHex()).toUpper());
        patches.append(stored);
    }

    QString error;
    const QByteArray backup = PatchBackupCodec::serialize(patches, &error);
    if (backup.size() != bankTotalUser * patchPerBank * patchSize) {
        qCritical("Unexpected backup size: %d (%s)", backup.size(), qPrintable(error));
        passed = false;
    }
    const QVector<DecodedPatch> parsed = PatchBackupCodec::parse(backup, &error);
    if (parsed.size() != bankTotalUser * patchPerBank
        || parsed.first().logicalBlocks00To0C != source.logicalBlocks00To0C
        || parsed.last().logicalBlocks00To0C != source.logicalBlocks00To0C) {
        qCritical("Valid 200-patch backup did not round-trip: %s", qPrintable(error));
        passed = false;
    }

    QByteArray corrupt = backup;
    corrupt[sysxDataOffset] = char(static_cast<unsigned char>(
        corrupt.at(sysxDataOffset)) ^ 0x01);
    passed &= expectRejected(corrupt, "checksum corruption");
    passed &= expectRejected(backup.left(backup.size() - 1), "truncated final frame");
    passed &= expectRejected(backup.left(backup.size() - patchSize),
                             "199-patch file");

    qInfo(passed ? "PASS patch backup offline regressions"
                 : "FAIL patch backup offline regressions");
    return passed ? 0 : 1;
}
