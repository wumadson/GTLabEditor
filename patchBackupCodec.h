#ifndef PATCHBACKUPCODEC_H
#define PATCHBACKUPCODEC_H

#include <QByteArray>
#include <QVector>

#include "patchTransferCodec.h"

class PatchBackupCodec
{
public:
    static DecodedPatch decodeDeviceReply(const QString &reply);
    static QByteArray serialize(const QVector<DecodedPatch> &patches,
                                QString *error = nullptr);
    static QVector<DecodedPatch> parse(const QByteArray &data,
                                       QString *error = nullptr);
    static bool validateMessageChecksum(const QByteArray &message);
};

#endif // PATCHBACKUPCODEC_H
