#ifndef PATCHTRANSFERCODEC_H
#define PATCHTRANSFERCODEC_H

#include <QMap>
#include <QString>
#include <QStringList>

#include "SysxIO.h"

struct DecodedPatch
{
    bool valid = false;
    bool quickFx = false;
    QString error;
    QString verifiedName;
    QStringList messages00To0C;
    QMap<QString, QString> logicalBlocks00To0C;

    QString serialized00To0C() const;
};

class PatchTransferCodec
{
public:
    static DecodedPatch decodePatchReply(const QString &rawReply);
    static QMap<QString, QString> comparableBlocks00To0C(
        const SysxData &source, QString *error = nullptr);
    static QString buildUserWriteMessage(const SysxData &source,
                                         int targetBank,
                                         int targetPatch,
                                         QString *error = nullptr);
    static QString buildLegacyWriteMessage(const SysxData &source,
                                           int bank,
                                           int patch,
                                           QString *error = nullptr);
};

#endif // PATCHTRANSFERCODEC_H
