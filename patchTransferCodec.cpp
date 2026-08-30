#include "patchTransferCodec.h"

#include "globalVariables.h"

#include <QFile>
#include <QObject>

namespace {
QString normalizedHex(QString value)
{
    value.remove(' ');
    value.remove('\n');
    value.remove('\r');
    return value.toUpper();
}

QString checksumForMessage(const QString &messageWithoutChecksum)
{
    bool ok = false;
    int sum = 0;
    const int byteCount = messageWithoutChecksum.size() / 2;
    for (int byte = checksumOffset; byte < byteCount; ++byte)
        sum += messageWithoutChecksum.mid(byte * 2, 2).toInt(&ok, 16);
    const int checksum = (0x80 - (sum % 0x80)) % 0x80;
    return QString("%1").arg(checksum, 2, 16, QChar('0')).toUpper();
}

QString canonicalMessage(const QString &addressMsb,
                         const QString &logicalAddress,
                         const QString &payload)
{
    QString message = "F0410000002F12" + addressMsb + logicalAddress
        + payload;
    message += checksumForMessage(message);
    message += "F7";
    return message;
}

bool userPatchAddress(int bank, int patch, QString *address1,
                      QString *address2)
{
    if (bank < 1 || bank > bankTotalUser
        || patch < 1 || patch > patchPerBank)
        return false;
    const int patchOffset = ((bank - 1) * patchPerBank) + patch - 1;
    const int pageSize = 0x80;
    *address1 = QString("%1").arg(0x10 + patchOffset / pageSize,
                                    2, 16, QChar('0')).toUpper();
    *address2 = QString("%1").arg(patchOffset % pageSize,
                                    2, 16, QChar('0')).toUpper();
    return true;
}

QList<QString> messageBytes(const QString &message)
{
    QList<QString> bytes;
    const QString normalized = normalizedHex(message);
    for (int offset = 0; offset + 1 < normalized.size(); offset += 2)
        bytes.append(normalized.mid(offset, 2));
    return bytes;
}

QString blockKey(const QList<QString> &message);

bool appendSyntheticBlock0D(SysxData *source, QString *error)
{
    QFile file(":default.syx");
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QObject::tr("Unable to read the legacy synthetic block 0D");
        return false;
    }
    const QByteArray tail = file.readAll().mid(patchSize);
    QList<QString> message;
    bool found = false;
    for (unsigned char byte : tail) {
        message.append(QString("%1").arg(byte, 2, 16, QChar('0')).toUpper());
        if (byte != 0xF7)
            continue;
        if (blockKey(message) == "0D00") {
            source->address.append("0D00");
            source->hex.append(message);
            found = true;
        }
        message.clear();
    }
    if (!found && error)
            *error = QObject::tr(
                "The legacy synthetic block 0D is missing from default.syx");
    return found;
}

QString blockKey(const QList<QString> &message)
{
    if (message.size() < sysxDataOffset + 2)
        return QString();
    return (message.at(sysxAddressOffset + 2)
            + message.at(sysxAddressOffset + 3)).toUpper();
}

QString blockPayload(const QList<QString> &message)
{
    if (message.size() < sysxDataOffset + 2)
        return QString();
    QString payload;
    for (int index = sysxDataOffset; index < message.size() - 2; ++index)
        payload += message.at(index).rightJustified(2, '0').toUpper();
    return payload;
}

}

QString DecodedPatch::serialized00To0C() const
{
    return messages00To0C.join(QString());
}

DecodedPatch PatchTransferCodec::decodePatchReply(const QString &rawReply)
{
    DecodedPatch decoded;
    const QString reply = normalizedHex(rawReply);
    if (reply.size() / 2 != patchReplySize) {
        decoded.error = QObject::tr("Patch readback has %1 bytes; expected %2")
            .arg(reply.size() / 2).arg(patchReplySize);
        return decoded;
    }
    if (!reply.startsWith("F041") || reply.mid(12, 2) != "12") {
        decoded.error = QObject::tr(
            "Patch readback does not start with a GT-10 DT1 frame");
        return decoded;
    }

    const QString addressMsb = reply.mid(14, 4);
    struct Part { const char *address; int first; int firstLength;
                  int second; int secondLength; };
    const Part parts[] = {
        {"0000", 22, 256, 0, 0},
        {"0100", 278, 228, 532, 28},
        {"0200", 560, 256, 0, 0},
        {"0300", 816, 200, 1042, 56},
        {"0400", 1098, 256, 0, 0},
        {"0500", 1354, 172, 0, 0},
        {"0600", 1552, 256, 0, 0},
        {"0700", 1808, 228, 2062, 28},
        {"0800", 2090, 256, 0, 0},
        {"0900", 2346, 200, 0, 0},
        {"0A00", 2572, 256, 0, 0},
        {"0B00", 2828, 226, 3080, 30},
        {"0C00", 3110, 256, 0, 0}
    };

    for (const Part &part : parts) {
        QString payload = reply.mid(part.first, part.firstLength);
        if (part.secondLength > 0)
            payload += reply.mid(part.second, part.secondLength);
        if (payload.size() != part.firstLength + part.secondLength) {
            decoded.error = QObject::tr("Patch readback block %1 is truncated")
                .arg(part.address);
            decoded.messages00To0C.clear();
            decoded.logicalBlocks00To0C.clear();
            return decoded;
        }
        const QString key = QString::fromLatin1(part.address);
        decoded.messages00To0C.append(
            canonicalMessage(addressMsb, key, payload));
        decoded.logicalBlocks00To0C.insert(key, payload);
    }

    const QString namePayload = decoded.logicalBlocks00To0C.value("0000");
    QString name;
    for (int index = 0; index < nameLength; ++index) {
        bool ok = false;
        const int value = namePayload.mid(index * 2, 2).toInt(&ok, 16);
        if (!ok)
            continue;
        if (value == 0x7E)
            name.append(QChar(0x2192));
        else if (value == 0x7F)
            name.append(QChar(0x2190));
        else
            name.append(QChar(value));
    }
    decoded.verifiedName = name.trimmed();
    decoded.quickFx = reply.contains("F0410000002F1230")
        || reply.contains("F0410000002F1240");
    decoded.valid = true;
    return decoded;
}

QMap<QString, QString> PatchTransferCodec::comparableBlocks00To0C(
    const SysxData &source, QString *error)
{
    QMap<QString, QString> blocks;
    for (const QList<QString> &message : source.hex) {
        const QString key = blockKey(message);
        if (key.size() != 4 || key.left(2).toInt(nullptr, 16) > 0x0C)
            continue;
        blocks.insert(key, blockPayload(message));
    }
    for (int block = 0; block <= 0x0C; ++block) {
        const QString key = QString("%1%2")
            .arg(block, 2, 16, QChar('0')).arg(0, 2, 16, QChar('0'))
            .toUpper();
        if (!blocks.contains(key)) {
            if (error)
                *error = QObject::tr("Current patch is missing logical block %1")
                    .arg(key);
            return QMap<QString, QString>();
        }
    }
    if (error)
        error->clear();
    return blocks;
}

QString PatchTransferCodec::buildUserWriteMessage(const SysxData &source,
                                                   int targetBank,
                                                   int targetPatch,
                                                   QString *error)
{
    if (targetBank < 1 || targetBank > bankTotalUser
        || targetPatch < 1 || targetPatch > patchPerBank) {
        if (error)
            *error = QObject::tr(
                "Persistent WRITE target is outside GT-10 User memory");
        return QString();
    }
    return buildLegacyWriteMessage(source, targetBank, targetPatch, error);
}

QByteArray PatchTransferCodec::encodePatchName16(const QString &name,
                                                  QString *error)
{
    if (name.isEmpty() || name.size() > nameLength) {
        if (error)
            *error = QObject::tr("Patch name must contain 1 to 16 characters");
        return QByteArray();
    }
    QByteArray encoded;
    encoded.reserve(nameLength);
    for (const QChar character : name) {
        const ushort unicode = character.unicode();
        if (unicode == 0x2192)
            encoded.append(char(0x7E));
        else if (unicode == 0x2190)
            encoded.append(char(0x7F));
        else if (unicode >= 0x20 && unicode <= 0x7D)
            encoded.append(char(unicode));
        else {
            if (error)
            *error = QObject::tr("Patch name contains an unsupported character");
            return QByteArray();
        }
    }
    encoded.append(QByteArray(nameLength - encoded.size(), char(0x20)));
    if (error)
        error->clear();
    return encoded;
}

QString PatchTransferCodec::buildUserNameWriteMessage(
    int targetBank, int targetPatch, const QByteArray &encodedName16,
    QString *error)
{
    QString address1;
    QString address2;
    if (!userPatchAddress(targetBank, targetPatch, &address1, &address2)) {
        if (error)
            *error = QObject::tr(
                "Persistent RENAME target is outside GT-10 User memory");
        return QString();
    }
    if (encodedName16.size() != nameLength) {
        if (error)
            *error = QObject::tr(
                "Persistent RENAME requires exactly 16 name bytes");
        return QString();
    }
    QString payload;
    for (unsigned char byte : encodedName16)
        payload += QString("%1").arg(byte, 2, 16, QChar('0')).toUpper();
    if (error)
        error->clear();
    return canonicalMessage(address1 + address2, "0000", payload);
}

QString PatchTransferCodec::buildUserCopyWriteMessage(
    const DecodedPatch &decoded, int targetBank, int targetPatch,
    QString *error)
{
    if (!decoded.valid || decoded.messages00To0C.size() != 13) {
        if (error)
            *error = QObject::tr(
                "COPY source does not contain authoritative blocks 00-0C");
        return QString();
    }
    SysxData source;
    for (const QString &message : decoded.messages00To0C) {
        const QList<QString> bytes = messageBytes(message);
        const QString key = blockKey(bytes);
        if (key.isEmpty()) {
            if (error)
            *error = QObject::tr("COPY source contains an invalid SysEx block");
            return QString();
        }
        source.address.append(key);
        source.hex.append(bytes);
    }
    // RQ1 does not return 0D. Match the legacy loader by appending the
    // standard synthetic/non-verifiable block from default.syx.
    if (!appendSyntheticBlock0D(&source, error))
        return QString();
    return buildUserWriteMessage(source, targetBank, targetPatch, error);
}

QString PatchTransferCodec::buildUserWriteMessage00To0C(
    const DecodedPatch &decoded, int targetBank, int targetPatch,
    QString *error)
{
    QString address1;
    QString address2;
    if (!userPatchAddress(targetBank, targetPatch, &address1, &address2)) {
        if (error)
            *error = QObject::tr(
                "Backup restore target is outside GT-10 User memory");
        return QString();
    }
    if (!decoded.valid || decoded.logicalBlocks00To0C.size() != 13) {
        if (error)
            *error = QObject::tr(
                "Backup patch does not contain authoritative blocks 00-0C");
        return QString();
    }

    QString message;
    for (int block = 0; block <= 0x0C; ++block) {
        const QString key = QString("%1%2")
            .arg(block, 2, 16, QChar('0')).arg(0, 2, 16, QChar('0'))
            .toUpper();
        const QString payload = decoded.logicalBlocks00To0C.value(key);
        if (payload.isEmpty()) {
            if (error)
                *error = QObject::tr("Backup patch is missing logical block %1")
                    .arg(key);
            return QString();
        }
        message += canonicalMessage(address1 + address2, key, payload);
    }
    if (error)
        error->clear();
    return message;
}

QString PatchTransferCodec::buildLegacyWriteMessage(const SysxData &source,
                                                     int bank,
                                                     int patch,
                                                     QString *error)
{
    if (source.hex.isEmpty()) {
        if (error)
            *error = QObject::tr("Current patch buffer is empty");
        return QString();
    }

    int patchOffset = (((bank - 1) * patchPerBank) + patch) - 1;
    QString address1;
    QString address2;
    if (bank >= 1 && bank <= bankTotalUser) {
        userPatchAddress(bank, patch, &address1, &address2);
    } else if (bank < 101) {
        const int memorySize = 0x80;
        const int emptyAddresses = memorySize
            - ((bankTotalUser * patchPerBank) - memorySize);
        if (bank > bankTotalUser)
            patchOffset += emptyAddresses;
        const int addressPage = patchOffset / memorySize;
        address1 = QString("%1").arg(0x10 + addressPage, 2, 16,
                                      QChar('0')).toUpper();
        address2 = QString("%1").arg(patchOffset - memorySize * addressPage,
                                      2, 16, QChar('0')).toUpper();
    } else {
        address1 = "30";
        address2 = QString("%1").arg(patch - 1, 2, 16, QChar('0'))
            .toUpper();
    }

    QString writeMessage;
    for (const QList<QString> &sourceMessage : source.hex) {
        QList<QString> message = sourceMessage;
        if (message.size() <= sysxAddressOffset + 1) {
            if (error)
            *error = QObject::tr(
                "Current patch contains an invalid SysEx block");
            return QString();
        }
        message[sysxAddressOffset] = address1;
        message[sysxAddressOffset + 1] = address2;
        writeMessage += message.join(QString());
    }
    if (error)
        error->clear();
    return writeMessage.toUpper();
}
