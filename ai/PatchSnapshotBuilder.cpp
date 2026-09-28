#include "ai/PatchSnapshotBuilder.h"

#include "MidiTable.h"
#include "ai/PatchParameterCatalog.h"
#include "globalVariables.h"
#include "modernSignalChainModel.h"
#include "patchTransferCodec.h"

#include <QMap>
#include <QRegExp>
#include <QSet>

namespace Ai {
namespace {

const char kStructure[] = "Structure";

int readRaw(const QMap<QString, QString> &blocks,
            const ParameterAddress &address, bool *ok = nullptr)
{
    bool offsetOk = false;
    const int offset = address.offset.toInt(&offsetOk, 16);
    const QString payload = blocks.value(address.block);
    bool valid = offsetOk && offset >= 0
        && payload.size() >= (offset + (address.twoByte ? 2 : 1)) * 2;
    int raw = 0;
    if (valid) {
        bool highOk = false;
        raw = payload.mid(offset * 2, 2).toInt(&highOk, 16);
        valid = highOk;
        if (address.twoByte) {
            bool lowOk = false;
            const int low = payload.mid((offset + 1) * 2, 2)
                                .toInt(&lowOk, 16);
            raw = raw * 128 + low;
            valid = valid && lowOk;
        }
    }
    if (ok)
        *ok = valid;
    return raw;
}

QString rawHex(int raw, bool twoByte)
{
    if (!twoByte)
        return QString::number(raw, 16).rightJustified(2, '0').toUpper();
    return QString("%1 %2")
        .arg(raw / 128, 2, 16, QChar('0'))
        .arg(raw % 128, 2, 16, QChar('0')).toUpper();
}

QString unitFromDisplay(const QString &display)
{
    const QStringList units = {"BPM", "ms", "Hz", "kHz", "dB", "%"};
    for (const QString &unit : units) {
        if (display.trimmed().endsWith(unit, Qt::CaseInsensitive))
            return unit;
    }
    return QString();
}

QString enumText(const Midi &map, int raw, bool twoByte)
{
    if (twoByte)
        return QString();
    const QString hex = QString::number(raw, 16).rightJustified(2, '0').toUpper();
    const int index = map.id.indexOf(hex);
    if (index < 0 || index >= map.level.size())
        return QString();
    return map.level.at(index).name.trimmed();
}

ParameterValue valueFor(const ParameterDescriptor &descriptor,
                        const QMap<QString, QString> &blocks)
{
    ParameterValue value;
    value.id = descriptor.id;
    value.address = descriptor.address.text();
    value.available = PatchParameterCatalog::isAvailable(descriptor, blocks);
    value.unavailableReason = value.available
        ? QString() : descriptor.unavailableReason;
    if (!value.available)
        return value;

    bool ok = false;
    const int raw = readRaw(blocks, descriptor.address, &ok);
    if (!ok) {
        value.available = false;
        value.unavailableReason = "address_unavailable";
        return value;
    }

    MidiTable *table = MidiTable::Instance();
    const QString bank = descriptor.address.block.left(2);
    const QString middle = descriptor.address.block.mid(2, 2);
    const Midi map = table->getMidiMap(kStructure, bank, middle,
                                       descriptor.address.offset);
    if (value.id.startsWith("parameter")) {
        QString label = !map.customdesc.trimmed().isEmpty()
            ? map.customdesc : (!map.desc.trimmed().isEmpty()
                ? map.desc : map.name);
        value.id = PatchParameterCatalog::canonicalId(
            label, value.id + descriptor.address.offset);
    }
    value.hasRaw = true;
    value.raw = raw;
    value.rawHex = rawHex(raw, descriptor.address.twoByte);
    value.display = table->getValue(kStructure, bank, middle,
        descriptor.address.offset,
        QString::number(raw, 16).toUpper());
    value.unit = unitFromDisplay(value.display);
    value.enumText = enumText(map, raw, descriptor.address.twoByte);
    return value;
}

QString patchNumber(int bank, int slot)
{
    if (bank < 1 || bank > bankTotalAll
        || slot < 1 || slot > patchPerBank)
        return QString::fromUtf8("—");
    const bool user = bank <= bankTotalUser;
    return QString("%1%2-%3")
        .arg(user ? QChar('U') : QChar('P'))
        .arg(user ? bank : bank - bankTotalUser, 2, 10, QChar('0'))
        .arg(slot);
}

ParameterAddress enabledAddress(int moduleId)
{
    switch (moduleId) {
    case 0x00: return {"0000", "40", false};
    case 0x01: return {"0000", "70", false};
    case 0x02:
    case 0x03: return {"0100", "00", false};
    case 0x04: return {"0100", "70", false};
    case 0x05: return {"0200", "00", false};
    case 0x06: return {"0600", "00", false};
    case 0x07: return {"0A00", "00", false};
    case 0x08: return {"0A00", "20", false};
    case 0x09: return {"0A00", "30", false};
    case 0x0A:
    case 0x0B: return {"0A00", "40", false};
    case 0x0C: return {"0A00", "71", false};
    case 0x0D: return {"0A00", "75", false};
    case 0x0E: return {"0A00", "79", false};
    default: return {};
    }
}

SignalChainModule chainModule(const modernSignalChainModel::Entry &entry,
                              const QMap<QString, QString> &blocks)
{
    SignalChainModule module;
    module.id = QString("module%1").arg(entry.moduleId, 2, 16, QChar('0'));
    module.name = modernSignalChainModel::displayName(entry);
    module.raw = entry.originalRawValue;
    module.normalized = entry.rawValue;
    module.position = entry.originalPosition;
    const ParameterAddress state = enabledAddress(entry.moduleId);
    if (!state.block.isEmpty()) {
        bool ok = false;
        const int raw = readRaw(blocks, state, &ok);
        module.hasEnabled = ok;
        module.enabled = ok && raw == 1;
    }
    return module;
}

QList<SignalChainModule> chainModules(
    const QList<modernSignalChainModel::Entry> &entries,
    const QMap<QString, QString> &blocks)
{
    QList<SignalChainModule> result;
    for (const auto &entry : entries)
        result.append(chainModule(entry, blocks));
    return result;
}

} // namespace

PatchSnapshotBuildResult PatchSnapshotBuilder::build(
    const PatchCapture &capture) const
{
    PatchSnapshotBuildResult result;
    QString contractError;
    const QMap<QString, QString> blocks =
        PatchTransferCodec::comparableBlocks00To0C(
            capture.fileSource(), &contractError);
    if (blocks.isEmpty()) {
        result.error = contractError;
        return result;
    }

    static const int expectedBytes[] = {
        128, 128, 128, 128, 128, 86, 128, 128, 128, 100, 128, 128, 128
    };
    for (int block = 0; block <= 0x0C; ++block) {
        const QString key = QString("%1%2")
            .arg(block, 2, 16, QChar('0')).arg("00").toUpper();
        const int bytes = blocks.value(key).size() / 2;
        if (bytes < expectedBytes[block]) {
            result.error = QString("Current patch block %1 is truncated (%2 bytes; expected %3)")
                .arg(key).arg(bytes).arg(expectedBytes[block]);
            return result;
        }
    }

    PatchSnapshot &snapshot = result.snapshot;
    snapshot.device.model = "BOSS GT-10";
    snapshot.device.connected = capture.connected();
    snapshot.device.confirmed = capture.deviceConfirmed();
    switch (capture.provenance()) {
    case PatchProvenance::DeviceTemporaryBuffer:
        snapshot.source.provenance = "deviceTemporaryBuffer";
        snapshot.source.reliability = capture.deviceConfirmed()
            ? "deviceConfirmed" : "capturedBuffer";
        break;
    case PatchProvenance::ImportedTemporaryBuffer:
        snapshot.source.provenance = "importedTemporaryBuffer";
        snapshot.source.reliability = "importedCapture";
        break;
    default:
        snapshot.source.provenance = "unknown";
        snapshot.source.reliability = "capturedBuffer";
        break;
    }

    snapshot.patch.name = capture.patchName();
    snapshot.patch.bank = capture.loadedBank();
    snapshot.patch.slot = capture.loadedPatch();
    snapshot.patch.displayNumber = patchNumber(
        capture.loadedBank(), capture.loadedPatch());
    snapshot.patch.location = capture.provenance()
            == PatchProvenance::ImportedTemporaryBuffer
        ? "importedTemporaryBuffer"
        : (capture.loadedBank() <= bankTotalUser ? "user" : "preset");
    snapshot.patch.dirty = capture.dirty();

    for (const ParameterDescriptor &descriptor
         : PatchParameterCatalog::patchParameters())
        snapshot.patchParameters.append(valueFor(descriptor, blocks));
    for (const ParameterDescriptor &descriptor
         : PatchParameterCatalog::routingParameters())
        snapshot.routing.append(valueFor(descriptor, blocks));

    for (const EffectDescriptor &descriptor
         : PatchParameterCatalog::effects(blocks)) {
        EffectSnapshot effect;
        effect.id = descriptor.id;
        if (!descriptor.state.block.isEmpty()) {
            bool ok = false;
            const int state = readRaw(blocks, descriptor.state, &ok);
            effect.hasEnabled = ok;
            effect.enabled = ok && state == 1;
        }
        if (!descriptor.type.block.isEmpty()) {
            ParameterDescriptor type;
            type.id = "type";
            type.address = descriptor.type;
            effect.type = valueFor(type, blocks);
        }
        for (const ParameterDescriptor &parameter : descriptor.parameters)
            effect.parameters.append(valueFor(parameter, blocks));
        snapshot.effects.append(effect);
    }

    QList<QString> chainBytes;
    const QString chainPayload = blocks.value("0B00");
    for (int index = 0; index < 18; ++index)
        chainBytes.append(chainPayload.mid(index * 2, 2));
    bool modeOk = false;
    bool selectOk = false;
    const int mode = readRaw(blocks, {"0100", "01", false}, &modeOk);
    const int select = readRaw(blocks, {"0100", "02", false}, &selectOk);
    modernSignalChainModel::ChainSnapshot chain;
    QString chainError;
    if (!modernSignalChainModel::parseRawBytes(
            chainBytes, &chain, &chainError, 0,
            snapshot.patch.displayNumber,
            modeOk && mode >= 0 && mode <= 3
                ? static_cast<modernSignalChainModel::ChannelMode>(mode)
                : modernSignalChainModel::UnknownMode,
            selectOk ? select : -1)) {
        result.error = "Signal Chain: " + chainError;
        return result;
    }
    snapshot.signalChain.commonPrefix = chainModules(chain.commonPrefix, blocks);
    snapshot.signalChain.split = chainModule(chain.split, blocks);
    snapshot.signalChain.pathA = chainModules(chain.pathA, blocks);
    snapshot.signalChain.pathB = chainModules(chain.pathB, blocks);
    snapshot.signalChain.merge = chainModule(chain.merge, blocks);
    snapshot.signalChain.commonSuffix = chainModules(chain.commonSuffix, blocks);
    snapshot.signalChain.channelMode = modernSignalChainModel::channelModeName(chain.channelMode);
    snapshot.signalChain.channelSelect = chain.channelSelect;
    snapshot.signalChain.sourceReliability = snapshot.source.reliability;

    result.ok = true;
    return result;
}

} // namespace Ai
