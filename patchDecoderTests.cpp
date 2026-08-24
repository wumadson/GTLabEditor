#include <QCoreApplication>
#include <QFile>
#include <QTextStream>

#include "patchTransferCodec.h"
#include "globalVariables.h"

namespace {
QString legacyDecode00To0C(QString reply)
{
    reply = reply.remove(" ").toUpper();
    if (reply.size() / 2 != 1784)
        return QString();
    const QString header = "F0410000002F12";
    const QString footer = "00F7";
    const QString addressMsb = reply.mid(14, 4);
    QStringList parts;
    auto part = [&](const QString &address, int start, int length,
                    int startB = 0, int lengthB = 0) {
        QString payload = reply.mid(start, length);
        if (lengthB)
            payload += reply.mid(startB, lengthB);
        parts << header + addressMsb + address + payload + footer;
    };
    part("0000", 22, 256);
    part("0100", 278, 228, 532, 28);
    part("0200", 560, 256);
    part("0300", 816, 200, 1042, 56);
    part("0400", 1098, 256);
    part("0500", 1354, 172);
    part("0600", 1552, 256);
    part("0700", 1808, 228, 2062, 28);
    part("0800", 2090, 256);
    part("0900", 2346, 200);
    part("0A00", 2572, 256);
    part("0B00", 2828, 226, 3080, 30);
    part("0C00", 3110, 256);

    QString rebuilt;
    for (QString message : parts) {
        bool ok = false;
        int sum = 0;
        const int bytes = message.size() / 2;
        for (int index = checksumOffset; index < bytes - 2; ++index)
            sum += message.mid(index * 2, 2).toInt(&ok, 16);
        const int checksum = (0x80 - (sum % 0x80)) % 0x80;
        message.replace(message.size() - 4, 2,
                        QString("%1").arg(checksum, 2, 16, QChar('0')));
        rebuilt += message;
    }
    return rebuilt.toUpper();
}

bool verifyFixture(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qCritical("Cannot open fixture: %s", qPrintable(path));
        return false;
    }
    const QString raw = QString::fromLatin1(file.readAll().toHex());
    const DecodedPatch decoded = PatchTransferCodec::decodePatchReply(raw);
    if (!decoded.valid) {
        qCritical("Decoder rejected %s: %s", qPrintable(path),
                  qPrintable(decoded.error));
        return false;
    }
    const QString legacy = legacyDecode00To0C(raw);
    if (decoded.serialized00To0C() != legacy) {
        qCritical("Decoder differs from legacy parser for %s", qPrintable(path));
        return false;
    }
    qInfo("PASS %s: 00-0C are byte-identical (%d bytes)",
          qPrintable(path), legacy.size() / 2);
    return true;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const bool pathA = verifyFixture("gt-10 temp patch_pathA.syx");
    const bool trimmed = verifyFixture("gt-10 temp patch_trimmed.syx");
    return pathA && trimmed ? 0 : 1;
}
