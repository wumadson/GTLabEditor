#include "modernSignalChainSerializer.h"

#include <QSet>
#include <QStringList>

namespace {
using Model = modernSignalChainModel;

bool fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return false;
}

bool validateRegion(const QList<Model::Entry> &entries,
                    Model::ChainRegion region,
                    QSet<int> *seen,
                    QString *error)
{
    for (const Model::Entry &entry : entries) {
        if (entry.moduleId < 0 || entry.moduleId > 0x11)
            return fail(error, QString("Unsupported module ID %1").arg(entry.moduleId));
        if (entry.moduleId == 0x10 || entry.moduleId == 0x11)
            return fail(error, "SPLIT/MERGE cannot appear inside a module region");
        if (seen->contains(entry.moduleId))
            return fail(error, QString("Module %1 is duplicated").arg(entry.moduleId));
        seen->insert(entry.moduleId);

        if (entry.region != region)
            return fail(error, QString("Module %1 has inconsistent region metadata")
                        .arg(entry.moduleId));
        const QString expected = modernSignalChainSerializer::rawForModule(
            entry.moduleId, region);
        if (expected.isEmpty())
            return fail(error, QString("Module %1 is not allowed in this region")
                        .arg(entry.moduleId));
        if (entry.rawValue.toUpper() != expected)
            return fail(error,
                        QString("Module %1 raw %2 is invalid in this region; expected %3")
                        .arg(entry.moduleId).arg(entry.rawValue, expected));
    }
    return true;
}
}

QString modernSignalChainSerializer::rawForModule(
    int moduleId, modernSignalChainModel::ChainRegion region)
{
    if (moduleId < 0 || moduleId > 0x11)
        return QString();
    if (moduleId == 0x10)
        return "10";
    if (moduleId == 0x11)
        return "11";
    if (moduleId == 0x02)
        return region == modernSignalChainModel::ChainRegion::PathA
            ? QString("02") : QString();
    if (moduleId == 0x03)
        return region == modernSignalChainModel::ChainRegion::PathB
            ? QString("43") : QString();

    const int raw = region == modernSignalChainModel::ChainRegion::PathB
        ? moduleId + 0x40 : moduleId;
    return QString("%1").arg(raw, 2, 16, QChar('0')).toUpper();
}

bool modernSignalChainSerializer::validate(
    const modernSignalChainModel::ChainSnapshot &snapshot, QString *error)
{
    const int count = snapshot.commonPrefix.size() + snapshot.pathA.size()
        + snapshot.pathB.size() + snapshot.commonSuffix.size() + 2;
    if (count != 18)
        return fail(error, QString("Signal chain requires exactly 18 modules; received %1")
                    .arg(count));
    if (snapshot.split.moduleId != 0x10 || snapshot.split.rawValue != "10")
        return fail(error, "Snapshot requires one canonical SPLIT (raw 10)");
    if (snapshot.merge.moduleId != 0x11 || snapshot.merge.rawValue != "11")
        return fail(error, "Snapshot requires one canonical MERGE (raw 11)");

    QSet<int> seen;
    seen.insert(0x10);
    seen.insert(0x11);
    if (!validateRegion(snapshot.commonPrefix, Model::ChainRegion::CommonPrefix,
                        &seen, error)
        || !validateRegion(snapshot.pathA, Model::ChainRegion::PathA,
                           &seen, error)
        || !validateRegion(snapshot.pathB, Model::ChainRegion::PathB,
                           &seen, error)
        || !validateRegion(snapshot.commonSuffix, Model::ChainRegion::CommonSuffix,
                           &seen, error))
        return false;

    if (seen.size() != 18) {
        QStringList missing;
        for (int id = 0; id < 18; ++id)
            if (!seen.contains(id))
                missing.append(QString::number(id));
        return fail(error, QString("Signal chain is missing module IDs: %1")
                    .arg(missing.join(", ")));
    }

    bool preampAInA = false;
    for (const Model::Entry &entry : snapshot.pathA)
        preampAInA |= entry.moduleId == 0x02 && entry.rawValue == "02";
    bool preampBInB = false;
    for (const Model::Entry &entry : snapshot.pathB)
        preampBInB |= entry.moduleId == 0x03 && entry.rawValue == "43";
    if (!preampAInA)
        return fail(error, "PREAMP A must remain in Path A as raw 02");
    if (!preampBInB)
        return fail(error, "PREAMP B must remain in Path B as raw 43");

    if (error)
        error->clear();
    return true;
}

bool modernSignalChainSerializer::serialize(
    const modernSignalChainModel::ChainSnapshot &snapshot,
    QList<QString> *bytes,
    QString *error)
{
    if (!bytes)
        return fail(error, "Serialized byte output is null");
    if (!validate(snapshot, error))
        return false;

    QList<QString> serialized;
    const auto append = [&serialized](const QList<Model::Entry> &entries) {
        for (const Model::Entry &entry : entries)
            serialized.append(entry.rawValue.toUpper());
    };
    append(snapshot.commonPrefix);
    serialized.append("10");
    append(snapshot.pathA);
    append(snapshot.pathB);
    serialized.append("11");
    append(snapshot.commonSuffix);
    if (serialized.size() != 18)
        return fail(error, "Serializer did not produce exactly 18 bytes");

    *bytes = serialized;
    if (error)
        error->clear();
    return true;
}
