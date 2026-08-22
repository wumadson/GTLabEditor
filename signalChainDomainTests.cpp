#include "modernSignalChainModel.h"
#include "modernSignalChainMutationController.h"
#include "modernSignalChainSerializer.h"

#include <QCoreApplication>
#include <QDebug>

namespace {
using Model = modernSignalChainModel;

const QList<QString> fixture = {
    "05", "00", "0A", "0E", "01", "10", "02", "0C", "43",
    "4D", "11", "04", "0B", "06", "07", "08", "09", "0F"
};

int failures = 0;

void check(bool condition, const QString &message)
{
    if (condition)
        qInfo().noquote() << "PASS" << message;
    else {
        qCritical().noquote() << "FAIL" << message;
        ++failures;
    }
}

QString bytesText(const QList<QString> &bytes)
{
    return bytes.join(" ");
}

bool loadFixture(Model *model)
{
    Model::ChainSnapshot snapshot;
    QString error;
    return Model::parseRawBytes(fixture, &snapshot, &error, 7, "fixture:real")
        && model->replaceSnapshot(snapshot, &error);
}

void expectMove(const QString &name, int moduleId, Model::ChainRegion region,
                int index, const QString &expected)
{
    Model model;
    check(loadFixture(&model), name + " loads fixture");
    modernSignalChainMutationController controller(&model);
    const ChainMoveResult result = controller.moveModule(moduleId, region, index);
    check(result.accepted, name + " accepted: " + result.error);
    check(bytesText(result.serializedBytes) == expected,
          name + " bytes = " + bytesText(result.serializedBytes));
    check(result.before.revision == 7 && result.after.revision == 8,
          name + " preserves versioned before/after snapshots");
}

void testRoundTrip()
{
    Model::ChainSnapshot snapshot;
    QString error;
    check(Model::parseRawBytes(fixture, &snapshot, &error, 1, "fixture:real"),
          "parse real 18-byte snapshot: " + error);
    QList<QString> serialized;
    check(modernSignalChainSerializer::serialize(snapshot, &serialized, &error),
          "serialize parsed snapshot: " + error);
    check(serialized == fixture,
          "roundtrip exact = " + bytesText(serialized));
    check(snapshot.patchIdentity == "fixture:real" && snapshot.revision == 1,
          "snapshot carries patch identity and revision");
}

void testMoves()
{
    expectMove("A EQ within suffix", 0x04, Model::ChainRegion::CommonSuffix, 7,
               "05 00 0A 0E 01 10 02 0C 43 4D 11 0B 06 07 08 09 0F 04");
    expectMove("B EQ to Path A", 0x04, Model::ChainRegion::PathA, 2,
               "05 00 0A 0E 01 10 02 0C 04 43 4D 11 0B 06 07 08 09 0F");
    expectMove("C EQ to Path B", 0x04, Model::ChainRegion::PathB, 2,
               "05 00 0A 0E 01 10 02 0C 43 4D 44 11 0B 06 07 08 09 0F");
    expectMove("D CHORUS suffix to prefix", 0x08,
               Model::ChainRegion::CommonPrefix, 0,
               "08 05 00 0A 0E 01 10 02 0C 43 4D 11 04 0B 06 07 09 0F");
    expectMove("E S/R to Path A", 0x0E, Model::ChainRegion::PathA, 1,
               "05 00 0A 01 10 02 0E 0C 43 4D 11 04 0B 06 07 08 09 0F");
    expectMove("F D.OUT to Path B", 0x0F, Model::ChainRegion::PathB, 1,
               "05 00 0A 0E 01 10 02 0C 43 4F 4D 11 04 0B 06 07 08 09");
}

void testFixedMoves()
{
    const int fixedIds[] = {0x02, 0x03, 0x10, 0x11};
    for (int id : fixedIds) {
        Model model;
        check(loadFixture(&model), QString("fixed %1 loads fixture").arg(id));
        modernSignalChainMutationController controller(&model);
        const Model::ChainSnapshot before = model.snapshot();
        const ChainMoveResult result = controller.moveModule(
            id, Model::ChainRegion::CommonPrefix, 0);
        QList<QString> bytes;
        QString error;
        modernSignalChainSerializer::serialize(model.snapshot(), &bytes, &error);
        check(!result.accepted && bytes == fixture
              && model.snapshot().revision == before.revision,
              QString("reject fixed module %1 without mutation").arg(id));
    }
}

void testInvalidSnapshots()
{
    Model::ChainSnapshot base;
    QString error;
    check(Model::parseRawBytes(fixture, &base, &error), "invalid tests load base");

    Model::ChainSnapshot duplicate = base;
    duplicate.commonPrefix[0].moduleId = 0x00;
    duplicate.commonPrefix[0].rawValue = "00";
    check(!modernSignalChainSerializer::validate(duplicate, &error),
          "reject duplicated module");

    Model::ChainSnapshot missing = base;
    missing.commonSuffix.removeLast();
    check(!modernSignalChainSerializer::validate(missing, &error),
          "reject missing module / count != 18");

    Model::ChainSnapshot preampAInB = base;
    Model::Entry preampA = preampAInB.pathA.takeFirst();
    preampA.region = Model::ChainRegion::PathB;
    preampA.path = Model::PathB;
    preampA.rawValue = "42";
    preampAInB.pathB.prepend(preampA);
    check(!modernSignalChainSerializer::validate(preampAInB, &error),
          "reject PREAMP A in Path B/raw 42");

    Model::ChainSnapshot preampBInA = base;
    Model::Entry preampB = preampBInA.pathB.takeFirst();
    preampB.region = Model::ChainRegion::PathA;
    preampB.path = Model::PathA;
    preampB.rawValue = "03";
    preampBInA.pathA.append(preampB);
    check(!modernSignalChainSerializer::validate(preampBInA, &error),
          "reject PREAMP B in Path A/raw 03");

    QList<QString> shortBytes = fixture;
    shortBytes.removeLast();
    Model::ChainSnapshot parsed;
    check(!Model::parseRawBytes(shortBytes, &parsed, &error),
          "reject raw byte count != 18");

    QList<QString> inverted = fixture;
    inverted[5] = "11";
    inverted[10] = "10";
    check(!Model::parseRawBytes(inverted, &parsed, &error),
          "reject MERGE before SPLIT");
}
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    testRoundTrip();
    testMoves();
    testFixedMoves();
    testInvalidSnapshots();
    qInfo().noquote() << (failures == 0 ? "ALL SIGNAL CHAIN DOMAIN TESTS PASSED"
                                       : "SIGNAL CHAIN DOMAIN TESTS FAILED")
                      << "failures=" << failures;
    return failures == 0 ? 0 : 1;
}
