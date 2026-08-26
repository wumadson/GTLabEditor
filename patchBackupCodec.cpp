#include "patchBackupCodec.h"

#include "globalVariables.h"

namespace {

QByteArray hexToBytes(const QString &hex, bool *ok)
{
    QByteArray output;
    *ok = hex.size() % 2 == 0;
    for (int offset = 0; *ok && offset < hex.size(); offset += 2) {
        bool byteOk = false;
        const int value = hex.mid(offset, 2).toInt(&byteOk, 16);
        *ok = byteOk;
        if (byteOk)
            output.append(char(value));
    }
    return output;
}

QString bytesToHex(const QByteArray &bytes)
{
    QString result;
    result.reserve(bytes.size() * 2);
    for (unsigned char byte : bytes)
        result += QString("%1").arg(byte, 2, 16, QChar('0')).toUpper();
    return result;
}

QString logicalKey(const QByteArray &message)
{
    if (message.size() < sysxDataOffset + 2)
        return QString();
    return QString("%1%2")
        .arg(static_cast<unsigned char>(message.at(sysxAddressOffset + 2)),
             2, 16, QChar('0'))
        .arg(static_cast<unsigned char>(message.at(sysxAddressOffset + 3)),
             2, 16, QChar('0')).toUpper();
}

QString payload(const QByteArray &message)
{
    return bytesToHex(message.mid(sysxDataOffset,
                                  message.size() - sysxDataOffset - 2));
}

}

bool PatchBackupCodec::validateMessageChecksum(const QByteArray &message)
{
    if (message.size() < sysxDataOffset + 2
        || static_cast<unsigned char>(message.front()) != 0xF0
        || static_cast<unsigned char>(message.back()) != 0xF7)
        return false;
    int sum = 0;
    for (int index = checksumOffset; index < message.size() - 1; ++index)
        sum += static_cast<unsigned char>(message.at(index));
    return (sum % 0x80) == 0;
}

DecodedPatch PatchBackupCodec::decodeDeviceReply(const QString &reply)
{
    DecodedPatch invalid;
    QString normalized = reply;
    normalized.remove(' ');
    normalized = normalized.toUpper();
    bool ok = false;
    const QByteArray data = hexToBytes(normalized, &ok);
    if (!ok || data.size() != patchReplySize) {
        invalid.error = QString("Expected %1 reply bytes, received %2")
            .arg(patchReplySize).arg(data.size());
        return invalid;
    }

    int offset = 0;
    int messageNumber = 0;
    while (offset < data.size()) {
        const int end = data.indexOf(char(0xF7), offset);
        if (static_cast<unsigned char>(data.at(offset)) != 0xF0 || end < 0) {
            invalid.error = QString("Malformed device reply at byte %1").arg(offset);
            return invalid;
        }
        const QByteArray message = data.mid(offset, end - offset + 1);
        ++messageNumber;
        if (message.left(7) != QByteArray::fromHex("F0410000002F12")) {
            invalid.error = QString("Unexpected SysEx message %1 in device reply")
                .arg(messageNumber);
            return invalid;
        }
        if (!validateMessageChecksum(message)) {
            invalid.error = QString("Invalid checksum in device reply message %1")
                .arg(messageNumber);
            return invalid;
        }
        offset = end + 1;
    }
    return PatchTransferCodec::decodePatchReply(normalized);
}

QByteArray PatchBackupCodec::serialize(const QVector<DecodedPatch> &patches,
                                       QString *error)
{
    if (patches.size() != bankTotalUser * patchPerBank) {
        if (error)
            *error = QString("Backup requires exactly %1 patches")
                .arg(bankTotalUser * patchPerBank);
        return QByteArray();
    }
    QByteArray output;
    output.reserve(patches.size() * patchSize);
    for (const DecodedPatch &patch : patches) {
        if (!patch.valid || patch.messages00To0C.size() != 13) {
            if (error)
                *error = "Backup contains an incomplete patch";
            return QByteArray();
        }
        for (const QString &hex : patch.messages00To0C) {
            bool ok = false;
            const QByteArray message = hexToBytes(hex, &ok);
            if (!ok || !validateMessageChecksum(message)) {
                if (error)
                    *error = "Backup contains an invalid DT1 checksum";
                return QByteArray();
            }
            output += message;
        }
    }
    QString validationError;
    if (parse(output, &validationError).size() != patches.size()) {
        if (error)
            *error = validationError;
        return QByteArray();
    }
    if (error)
        error->clear();
    return output;
}

QVector<DecodedPatch> PatchBackupCodec::parse(const QByteArray &data,
                                              QString *error)
{
    QVector<QByteArray> messages;
    int offset = 0;
    while (offset < data.size()) {
        if (static_cast<unsigned char>(data.at(offset)) != 0xF0) {
            if (error)
                *error = QString("Unexpected byte at file offset %1").arg(offset);
            return QVector<DecodedPatch>();
        }
        const int end = data.indexOf(char(0xF7), offset);
        if (end < 0) {
            if (error)
                *error = "Backup contains a truncated SysEx message";
            return QVector<DecodedPatch>();
        }
        const QByteArray message = data.mid(offset, end - offset + 1);
        if (message.left(7) != QByteArray::fromHex("F0410000002F12")) {
            if (error)
                *error = "Backup is not a BOSS GT-10 DT1 stream";
            return QVector<DecodedPatch>();
        }
        if (!validateMessageChecksum(message)) {
            if (error)
                *error = QString("Invalid checksum in SysEx message %1")
                    .arg(messages.size() + 1);
            return QVector<DecodedPatch>();
        }
        messages.append(message);
        offset = end + 1;
    }

    const int expectedPatches = bankTotalUser * patchPerBank;
    if (messages.size() != expectedPatches * 13) {
        if (error)
            *error = QString("Backup contains %1 patches; exactly %2 are required")
                .arg(messages.size() / 13).arg(expectedPatches);
        return QVector<DecodedPatch>();
    }

    QVector<DecodedPatch> patches;
    patches.reserve(expectedPatches);
    for (int patchIndex = 0; patchIndex < expectedPatches; ++patchIndex) {
        DecodedPatch patch;
        const int expectedPage = 0x10 + patchIndex / 0x80;
        const int expectedOffset = patchIndex % 0x80;
        for (int block = 0; block <= 0x0C; ++block) {
            const QByteArray message = messages.at(patchIndex * 13 + block);
            const int page = static_cast<unsigned char>(message.at(sysxAddressOffset));
            const int slot = static_cast<unsigned char>(message.at(sysxAddressOffset + 1));
            const QString expectedKey = QString("%1%2")
                .arg(block, 2, 16, QChar('0')).arg(0, 2, 16, QChar('0'))
                .toUpper();
            if (page != expectedPage || slot != expectedOffset
                || logicalKey(message) != expectedKey) {
                if (error)
                    *error = QString("Patch %1 has an invalid User address or block order")
                        .arg(patchIndex + 1);
                return QVector<DecodedPatch>();
            }
            const QString blockPayload = payload(message);
            const int expectedPayloadBytes = block == 0x05 ? 86
                : block == 0x09 ? 100 : 128;
            if (blockPayload.size() / 2 != expectedPayloadBytes) {
                if (error)
                    *error = QString("Patch %1 block %2 has an invalid payload size")
                        .arg(patchIndex + 1).arg(expectedKey);
                return QVector<DecodedPatch>();
            }
            patch.messages00To0C.append(bytesToHex(message));
            patch.logicalBlocks00To0C.insert(expectedKey, blockPayload);
        }
        const QString namePayload = patch.logicalBlocks00To0C.value("0000");
        for (int index = 0; index < nameLength; ++index) {
            bool ok = false;
            const int value = namePayload.mid(index * 2, 2).toInt(&ok, 16);
            if (ok)
                patch.verifiedName.append(value == 0x7E ? QChar(0x2192)
                    : value == 0x7F ? QChar(0x2190) : QChar(value));
        }
        patch.verifiedName = patch.verifiedName.trimmed();
        patch.valid = true;
        patches.append(patch);
    }
    if (error)
        error->clear();
    return patches;
}
