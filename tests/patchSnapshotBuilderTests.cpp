#include "ai/PatchCapture.h"
#include "ai/AIContextBuilder.h"
#include "ai/PatchJsonSerializer.h"
#include "ai/PatchParameterCatalog.h"
#include "ai/PatchSnapshotBuilder.h"
#include "ai/PatchSnapshotPreviewDialog.h"
#include "globalVariables.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

using namespace Ai;

#define AI_STRINGIFY_DETAIL(value) #value
#define AI_STRINGIFY(value) AI_STRINGIFY_DETAIL(value)

namespace {
int failures = 0;
int assertions = 0;

void expect(bool condition, const QString &name)
{
    ++assertions;
    if (!condition) {
        ++failures;
        QTextStream(stderr) << "FAIL: " << name << '\n';
    }
}

QList<QString> message(const QString &key, const QByteArray &payload)
{
    QList<QString> bytes;
    for (int i = 0; i < sysxDataOffset; ++i)
        bytes.append("00");
    bytes[sysxAddressOffset + 2] = key.left(2);
    bytes[sysxAddressOffset + 3] = key.mid(2, 2);
    for (unsigned char byte : payload)
        bytes.append(QString::number(byte, 16).rightJustified(2, '0').toUpper());
    bytes << "00" << "F7";
    return bytes;
}

SysxData source()
{
    static const int sizes[] = {
        128,128,128,128,128,86,128,128,128,100,128,128,128
    };
    SysxData data;
    for (int block = 0; block <= 0x0C; ++block) {
        const QString key = QString("%1%2").arg(block, 2, 16, QChar('0'))
                                .arg("00").toUpper();
        QByteArray payload(sizes[block], char(0));
        data.address.append(key);
        data.hex.append(message(key, payload));
    }

    // A valid 18-position dual-path chain.
    const unsigned char chain[] = {
        0x0C,0x00,0x01,0x10,0x02,0x04,0x05,0x43,0x46,
        0x11,0x07,0x08,0x09,0x0A,0x0B,0x0D,0x0E,0x0F
    };
    const int chainIndex = data.address.indexOf("0B00");
    for (int i = 0; i < 18; ++i)
        data.hex[chainIndex][sysxDataOffset + i] =
            QString::number(chain[i], 16).rightJustified(2, '0').toUpper();
    return data;
}

void setByte(SysxData *data, const QString &block, int offset, int raw)
{
    const int index = data->address.indexOf(block);
    data->hex[index][sysxDataOffset + offset] =
        QString::number(raw, 16).rightJustified(2, '0').toUpper();
}

void setTwoBytes(SysxData *data, const QString &block, int offset, int raw)
{
    setByte(data, block, offset, raw / 128);
    setByte(data, block, offset + 1, raw % 128);
}

PatchSnapshotBuildResult build(const SysxData &data,
                               int bank = 1, int slot = 1,
                               PatchProvenance provenance =
                                   PatchProvenance::DeviceTemporaryBuffer,
                               bool dirty = false)
{
    return PatchSnapshotBuilder().build(PatchCapture(
        data, "Snapshot Test", bank, slot, provenance,
        true, dirty, true));
}

const EffectSnapshot *effect(const PatchSnapshot &snapshot, const QString &id)
{
    for (const EffectSnapshot &candidate : snapshot.effects) {
        if (candidate.id == id)
            return &candidate;
    }
    return nullptr;
}

const ParameterValue *parameter(const EffectSnapshot *effect,
                                const QString &id)
{
    if (!effect)
        return nullptr;
    for (const ParameterValue &candidate : effect->parameters) {
        if (candidate.id == id)
            return &candidate;
    }
    return nullptr;
}

QJsonObject jsonEffect(const QJsonObject &root, const QString &id)
{
    for (const QJsonValue &value : root.value("effects").toArray()) {
        const QJsonObject candidate = value.toObject();
        if (candidate.value("id").toString() == id)
            return candidate;
    }
    return {};
}

QJsonObject jsonParameter(const QJsonArray &parameters, const QString &id)
{
    for (const QJsonValue &value : parameters) {
        const QJsonObject candidate = value.toObject();
        if (candidate.value("id").toString() == id)
            return candidate;
    }
    return {};
}

QJsonObject goldenProjection(const QJsonObject &root)
{
    QJsonObject projection;
    projection.insert("schemaVersion", root.value("schemaVersion"));
    projection.insert("device", root.value("device"));
    projection.insert("source", root.value("source"));
    projection.insert("patch", root.value("patch"));
    projection.insert("controllers", root.value("controllers"));
    return projection;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    SysxData base = source();
    setByte(&base, "0100", 1, 1);
    setByte(&base, "0100", 2, 0);
    setByte(&base, "0000", 0x41, 0);
    setByte(&base, "0000", 0x71, 0);
    setByte(&base, "0A00", 0x31, 0);
    setByte(&base, "0A00", 0x45, 0);
    setByte(&base, "0200", 1, 0x14);
    setByte(&base, "0600", 1, 0x14);
    setTwoBytes(&base, "0A00", 0x66, 120);

    const auto valid = build(base);
    expect(valid.ok, "1 valid capture with blocks 00-0C");

    SysxData missing = base;
    const int missingIndex = missing.address.indexOf("0500");
    missing.address.removeAt(missingIndex);
    missing.hex.removeAt(missingIndex);
    expect(!build(missing).ok, "2 missing required block fails");

    SysxData truncated = base;
    const int truncatedIndex = truncated.address.indexOf("0300");
    truncated.hex[truncatedIndex].removeAt(sysxDataOffset + 127);
    expect(!build(truncated).ok, "3 truncated block fails");

    expect(valid.snapshot.patch.location == "user"
           && valid.snapshot.patch.displayNumber == "U01-1", "4 USER metadata");
    const auto preset = build(base, 51, 2);
    expect(preset.ok && preset.snapshot.patch.location == "preset"
           && preset.snapshot.patch.displayNumber == "P01-2", "5 PRESET metadata");
    const auto imported = build(base, 1, 1,
        PatchProvenance::ImportedTemporaryBuffer);
    expect(imported.ok && imported.snapshot.patch.location
           == "importedTemporaryBuffer", "6 imported Temporary Buffer");
    expect(!valid.snapshot.patch.dirty, "7 dirty false");
    expect(build(base, 1, 1, PatchProvenance::DeviceTemporaryBuffer, true)
           .snapshot.patch.dirty, "8 dirty true");
    expect(valid.snapshot.signalChain.pathA.size() == 3
           && valid.snapshot.signalChain.pathB.size() == 2
           && !valid.snapshot.signalChain.pathA.first().raw.isEmpty()
           && !valid.snapshot.signalChain.pathA.first().normalized.isEmpty(),
           "9 Signal Chain topology/raw/normalized");
    expect(valid.snapshot.routing.size() == 4
           && valid.snapshot.signalChain.channelMode == "DUAL MONO", "10 routing");

    expect(parameter(effect(valid.snapshot, "comp"), "sustain")->available
           && !parameter(effect(valid.snapshot, "comp"), "threshold")->available,
           "11 COMP Compressor condition");
    SysxData limiterData = base;
    setByte(&limiterData, "0000", 0x41, 1);
    const auto limiter = build(limiterData);
    expect(parameter(effect(limiter.snapshot, "comp"), "threshold")->available
           && !parameter(effect(limiter.snapshot, "comp"), "sustain")->available,
           "12 COMP Limiter condition");

    expect(!parameter(effect(valid.snapshot, "odDs"), "customType")->available,
           "13 normal OD hides custom");
    SysxData customOdData = base;
    setByte(&customOdData, "0000", 0x71, 0x19);
    const auto customOd = build(customOdData);
    expect(parameter(effect(customOd.snapshot, "odDs"), "customType")->available,
           "14 Custom OD exposes custom");

    expect(!parameter(effect(valid.snapshot, "preampA"), "customType")->available,
           "15 normal PREAMP");
    SysxData customPreampData = base;
    setByte(&customPreampData, "0100", 0x10, 0x27);
    const auto customPreamp = build(customPreampData);
    expect(parameter(effect(customPreamp.snapshot, "preampA"), "customType")->available,
           "16 Custom PREAMP");
    SysxData customSpeakerData = base;
    setByte(&customSpeakerData, "0100", 0x1B, 0x09);
    const auto customSpeaker = build(customSpeakerData);
    expect(parameter(effect(customSpeaker.snapshot, "preampA"),
                     "customSpeakerSize")->available, "17 Custom Speaker");
    SysxData noBrightData = base;
    setByte(&noBrightData, "0100", 0x10, 0x03);
    expect(parameter(effect(valid.snapshot, "preampA"), "bright")->available
           && !parameter(effect(build(noBrightData).snapshot, "preampA"),
                         "bright")->available,
           "18 Bright available/unavailable shared rule");

    expect(!parameter(effect(valid.snapshot, "delay"), "dualDelay1Time")->available,
           "19 Delay Single");
    SysxData panData = base;
    setByte(&panData, "0A00", 1, 1);
    expect(parameter(effect(build(panData).snapshot, "delay"),
                     "tapTime")->available, "19b Delay Pan");
    SysxData dualData = base;
    setByte(&dualData, "0A00", 1, 3);
    expect(parameter(effect(build(dualData).snapshot, "delay"),
                     "dualDelay1Time")->available, "20 Delay Dual");
    SysxData warpData = base;
    setByte(&warpData, "0A00", 1, 9);
    expect(parameter(effect(build(warpData).snapshot, "delay"),
                     "warpState")->available, "21 Delay Warp");
    SysxData modData = base;
    setByte(&modData, "0A00", 1, 10);
    expect(parameter(effect(build(modData).snapshot, "delay"),
                     "modulateRate")->available, "22 Delay Modulate");

    SysxData springData = base;
    setByte(&springData, "0A00", 0x31, 5);
    expect(parameter(effect(build(springData).snapshot, "reverb"),
                     "springSensitivity")->available, "23 Reverb Spring");
    expect(!parameter(effect(valid.snapshot, "reverb"),
                      "springSensitivity")->available, "24 Reverb non-Spring");

    const EffectSnapshot *fx = effect(valid.snapshot, "fx1");
    expect(fx && fx->type.hasRaw && fx->type.raw == 0x14
           && fx->parameters.size() == 7, "25 FX algorithm page");
    SysxData hiddenFxData = base;
    setByte(&hiddenFxData, "0200", 0x40, 1);
    const auto hiddenFx = build(hiddenFxData);
    int visibleFxParameters = 0;
    for (const ParameterValue &value :
         effect(hiddenFx.snapshot, "fx1")->parameters) {
        if (value.available)
            ++visibleFxParameters;
    }
    expect(visibleFxParameters == 2, "25b FX visibleWhen");
    SysxData customWahData = base;
    setByte(&customWahData, "0A00", 0x45, 3);
    setByte(&customWahData, "0A00", 0x49, 6);
    expect(effect(valid.snapshot, "pedalFxFv")->type.raw == 0
           && !parameter(effect(valid.snapshot, "pedalFxFv"),
                         "footVolumeLevel")->available
           && parameter(effect(build(customWahData).snapshot, "pedalFxFv"),
                        "customWah1")->available,
           "26 P.FX Mode");

    SysxData footVolumeData = base;
    setByte(&footVolumeData, "0A00", 0x40, 1);
    setByte(&footVolumeData, "0A00", 0x45, 1);
    setByte(&footVolumeData, "0A00", 0x5A, 81);
    setByte(&footVolumeData, "0A00", 0x5B, 12);
    setByte(&footVolumeData, "0A00", 0x5C, 99);
    setByte(&footVolumeData, "0A00", 0x5D, 2);
    setByte(&footVolumeData, "0100", 0x03, 3);
    const PatchCapture footVolumeCapture(
        footVolumeData, "Snapshot Test", 29, 1,
        PatchProvenance::DeviceTemporaryBuffer, true, false, true);
    const int footVolumeBlock =
        footVolumeCapture.fileSource().address.indexOf("0A00");
    expect(footVolumeBlock >= 0
           && footVolumeCapture.fileSource()
                  .hex.at(footVolumeBlock)
                  .at(sysxDataOffset + 0x5A).toInt(nullptr, 16) == 81,
           "26b PatchCapture preserves Foot Volume Level raw 81");
    const auto footVolume = PatchSnapshotBuilder().build(footVolumeCapture);
    const EffectSnapshot *footVolumeEffect =
        effect(footVolume.snapshot, "pedalFxFv");
    const ParameterValue *footVolumeLevel =
        parameter(footVolumeEffect, "footVolumeLevel");
    expect(footVolume.ok && footVolumeLevel
           && footVolumeLevel->raw == 81
           && footVolumeLevel->display == "81",
           "26c Foot Volume Level preserves raw 81 in Snapshot");
    expect(parameter(footVolumeEffect, "footVolumeMinimum")->raw == 12
           && parameter(footVolumeEffect, "footVolumeMinimum")->display == "12"
           && parameter(footVolumeEffect, "footVolumeMaximum")->raw == 99
           && parameter(footVolumeEffect, "footVolumeMaximum")->display == "99"
           && parameter(footVolumeEffect, "footVolumeCurve")->raw == 2
           && parameter(footVolumeEffect, "footVolumeCurve")->enumText == "Normal",
           "26d Foot Volume Min/Max/Curve preserve backend values");
    const QJsonObject snapshotFootVolume = jsonParameter(
        jsonEffect(PatchJsonSerializer::toJson(footVolume.snapshot),
                   "pedalFxFv").value("parameters").toArray(),
        "footVolumeLevel");
    expect(snapshotFootVolume.value("raw").toInt() == 81
           && snapshotFootVolume.value("display") == "81",
           "26e Snapshot JSON preserves Foot Volume Level 81");
    const AIContextBuilder contextBuilder;
    const AIContext detailedFootVolume = contextBuilder.build(
        footVolume.snapshot, AIContextDetail::Detailed);
    const AIContext compactFootVolume = contextBuilder.build(
        footVolume.snapshot, AIContextDetail::Compact);
    const QJsonObject detailedFootVolumeValue = jsonParameter(
        jsonEffect(detailedFootVolume.root, "pedalFxFv")
            .value("parameters").toArray(), "footVolumeLevel");
    expect(detailedFootVolumeValue.value("raw").toInt() == 81
           && detailedFootVolumeValue.value("value") == "81",
           "26f Detailed preserves Foot Volume Level 81");
    const QJsonObject compactFootVolumeEffect =
        jsonEffect(compactFootVolume.root, "pedalFxFv");
    expect(compactFootVolumeEffect.value("footVolumeLevel") == "81"
           && compactFootVolumeEffect.value("footVolumeMinimum") == "12"
           && compactFootVolumeEffect.value("footVolumeMaximum") == "99"
           && compactFootVolumeEffect.value("footVolumeCurve") == "Normal",
           "26g Compact preserves Foot Volume family values");
    const QJsonObject detailedRouting =
        detailedFootVolume.root.value("routing").toObject();
    expect(jsonParameter(detailedRouting.value("parameters").toArray(),
                         "channelDelay").value("value") == "3ms",
           "26h Detailed retains technical Channel Delay display");
    expect(compactFootVolume.root.value("routing").toObject()
               .value("channelDelay") == "3 ms",
           "26i Compact normalizes Channel Delay unit spacing");
    expect(valid.snapshot.patchParameters.at(1).raw == 120
           && valid.snapshot.patchParameters.at(1).rawHex == "00 78",
           "27 two-byte value");
    expect(!valid.snapshot.routing.at(0).enumText.isEmpty(), "28 enum text");
    expect(!valid.snapshot.patchParameters.at(0).display.isEmpty()
           && valid.snapshot.routing.at(2).unit == "ms",
           "29 display/unit source");
    expect(physicalUnitFromDisplay("Db").isEmpty(),
           "29b musical note Db is not inferred as dB");
    expect(physicalUnitFromDisplay("0 dB") == "dB",
           "29c exact dB suffix is inferred");
    const QStringList musicalNotes = {
        "C", "Db", "D", "Eb", "E", "F", "F#", "G",
        "Ab", "A", "Bb", "B"};
    bool notesHaveNoUnits = true;
    for (const QString &note : musicalNotes)
        notesHaveNoUnits = notesHaveNoUnits
            && physicalUnitFromDisplay(note).isEmpty();
    expect(notesHaveNoUnits,
           "29c2 Harmonist musical notes never infer physical units");
    expect(valid.snapshot.patchParameters.at(1).unit == "BPM",
           "29d Master BPM uses catalog unit metadata");
    expect(parameter(effect(valid.snapshot, "reverb"), "reverbTime")->unit
               == "sec"
           && parameter(effect(valid.snapshot, "reverb"), "preDelay")->unit
               == "ms",
           "29e Reverb time units use catalog metadata");
    expect(PatchParameterCatalog::canonicalId("Feed Back", "fallback")
               == "feedback",
           "29f generated canonical IDs normalize internal case");

    const QJsonObject first = PatchJsonSerializer::toJson(valid.snapshot);
    const QJsonObject second = PatchJsonSerializer::toJson(build(base).snapshot);
    expect(first == second, "30 duplicate build is semantically identical");
    const QJsonObject beforeSerialization =
        PatchJsonSerializer::toJson(valid.snapshot);
    const QByteArray serialized =
        PatchJsonSerializer::serialize(valid.snapshot, true);
    expect(!serialized.isEmpty()
           && PatchJsonSerializer::toJson(valid.snapshot)
               == beforeSerialization,
           "30b serializer does not mutate snapshot");
    expect(PatchSnapshotPreviewDialog::suggestedFileName(
               "U29-1", "GN WSHP LEAD")
               == "U29-1_GN_WSHP_LEAD_snapshot.json",
           "30c suggested export filename");
    const QString unsafeFileName =
        PatchSnapshotPreviewDialog::suggestedFileName("U01-1", "A/B:*? Lead");
    expect(!unsafeFileName.contains('/') && !unsafeFileName.contains(':')
           && !unsafeFileName.contains('*') && !unsafeFileName.contains('?')
           && unsafeFileName.endsWith("_snapshot.json"),
           "30d export filename sanitization");
    expect(PatchSnapshotPreviewDialog::suggestedFileName(QString(), QString())
               == "gtlab_patch_snapshot.json",
           "30e empty export filename fallback");
    const auto offline = PatchSnapshotBuilder().build(PatchCapture(
        base, "Offline Capture", 1, 1,
        PatchProvenance::ImportedTemporaryBuffer,
        false, false, false));
    expect(offline.ok && !offline.snapshot.device.connected,
           "31 builder completes from capture without live MIDI/SysEx");

    QFile fixture(QString::fromLatin1(AI_STRINGIFY(PATCH_SNAPSHOT_FIXTURE_DIR))
                      + "/snapshot_v1_golden.json");
    expect(fixture.open(QIODevice::ReadOnly), "golden fixture opens");
    const QJsonDocument expected = QJsonDocument::fromJson(fixture.readAll());
    expect(expected.isObject()
           && goldenProjection(first) == expected.object(),
           "golden JSON semantic comparison");

    QTextStream(stdout) << "Patch Snapshot Builder tests: assertions="
                        << assertions << " failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
