#include "modernSignalChainModel.h"

#include "MidiTable.h"
#include "SysxIO.h"
#include "globalVariables.h"
#include "modernSignalChainSerializer.h"

#include <QDebug>
#include <QStringList>

namespace {
const int chainLength = 18;

QString displayName(int moduleId, const QString &mappedName)
{
    switch (moduleId) {
    case 0x00: return "COMP";
    case 0x01: return "OD/DS";
    case 0x02: return "PREAMP A";
    case 0x03: return "PREAMP B";
    case 0x04: return "EQ";
    case 0x05: return "FX-1";
    case 0x06: return "FX-2";
    case 0x07: return "DELAY";
    case 0x08: return "CHORUS";
    case 0x09: return "REVERB";
    case 0x0A: return "PEDAL FX";
    case 0x0B: return "FOOT VOLUME";
    case 0x0C: return "NS-1";
    case 0x0D: return "NS-2";
    case 0x0E: return "SEND/RETURN";
    case 0x0F: return "DIGITAL OUT";
    case 0x10: return "SPLIT";
    case 0x11: return "MERGE";
    default: return mappedName;
    }
}

QString joinedNames(const QList<modernSignalChainModel::Entry> &entries)
{
    QStringList names;
    for (const modernSignalChainModel::Entry &entry : entries)
        names.append(displayName(entry.moduleId, entry.name));
    return names.isEmpty() ? QString("(empty)") : names.join(QString::fromUtf8(" → "));
}
}

void modernSignalChainModel::clear()
{
    valid = false;
    currentSourceState = SourceState::Unavailable;
    error.clear();
    mode = UnknownMode;
    selectedChannel = -1;
    chainEntries.clear();
    prefixEntries.clear();
    pathAEntries.clear();
    pathBEntries.clear();
    suffixEntries.clear();
    confirmedSnapshot = ChainSnapshot();
}

bool modernSignalChainModel::refreshFromLegacyBackend()
{
    const quint64 nextRevision = confirmedSnapshot.revision + 1;
    clear();

    SysxIO *sysxIO = SysxIO::Instance();
    const SysxData source = sysxIO->getFileSource();
    const int chainAddressIndex = source.address.indexOf("0B00");
    const int modeAddressIndex = source.address.indexOf("0100");
    const bool deviceConfirmed =
        sysxIO->isConnected() && sysxIO->isDevice();
    const QString sourceFileName = sysxIO->getFileName();
    const bool temporaryBuffer = !sysxIO->isDevice()
        && !sourceFileName.isEmpty()
        && sourceFileName != ":default.syx";

    if (!deviceConfirmed && !temporaryBuffer) {
        error = "legacy backend has no displayable GT-10 patch source";
        return false;
    }
    if (chainAddressIndex < 0 || chainAddressIndex >= source.hex.size()) {
        error = "Structure 0B 00 is absent from the patch buffer";
        return false;
    }

    const QList<QString> chainBlock = source.hex.at(chainAddressIndex);
    if (chainBlock.size() < sysxDataOffset + chainLength) {
        error = QString("Structure 0B 00 is truncated (%1 data bytes available)")
                    .arg(qMax(0, chainBlock.size() - sysxDataOffset));
        return false;
    }
    if (modeAddressIndex < 0 || modeAddressIndex >= source.hex.size()) {
        error = "Preamp structure 01 00 is absent from the patch buffer";
        return false;
    }

    const QList<QString> modeBlock = source.hex.at(modeAddressIndex);
    if (modeBlock.size() <= sysxDataOffset + 2) {
        error = "Preamp structure 01 00 is truncated";
        return false;
    }

    bool modeOk = false;
    const int modeValue = modeBlock.at(sysxDataOffset + 1).toInt(&modeOk, 16);
    bool channelOk = false;
    selectedChannel = modeBlock.at(sysxDataOffset + 2).toInt(&channelOk, 16);
    if (!modeOk || modeValue < 0 || modeValue > 3) {
        error = "Channel Mode 01 00 01 is invalid";
        return false;
    }
    if (!channelOk)
        selectedChannel = -1;
    mode = static_cast<ChannelMode>(modeValue);

    MidiTable *midiTable = MidiTable::Instance();
    int splitPosition = -1;
    int mergePosition = -1;

    for (int position = 0; position < chainLength; ++position) {
        const QString raw = chainBlock.at(sysxDataOffset + position).toUpper();
        bool rawOk = false;
        const int rawByte = raw.toInt(&rawOk, 16);
        if (!rawOk) {
            error = QString("Chain byte %1 is invalid: %2").arg(position).arg(raw);
            chainEntries.clear();
            return false;
        }

        const int moduleId = rawByte >= 0x40 && rawByte <= 0x4F
            ? rawByte - 0x40 : rawByte;
        if (moduleId < 0 || moduleId > 0x11) {
            error = QString("Chain byte %1 has unsupported module value %2")
                        .arg(position).arg(raw);
            chainEntries.clear();
            return false;
        }

        const Midi mapping = midiTable->getMidiMap(
            "Structure", "0B", "00", "00", raw);
        Entry entry;
        entry.rawValue = raw;
        entry.originalRawValue = raw;
        entry.moduleId = moduleId;
        entry.name = mapping.name;
        entry.originalPosition = position;
        entry.movable = moduleId != 0x02 && moduleId != 0x03
            && moduleId != 0x10 && moduleId != 0x11;
        entry.isSplit = moduleId == 0x10;
        entry.isMerge = moduleId == 0x11;
        entry.isPreampA = moduleId == 0x02;
        entry.isPreampB = moduleId == 0x03;

        if (entry.isSplit) {
            if (splitPosition >= 0) {
                error = "Signal chain contains more than one split";
                chainEntries.clear();
                return false;
            }
            splitPosition = position;
        }
        if (entry.isMerge) {
            if (mergePosition >= 0) {
                error = "Signal chain contains more than one merge";
                chainEntries.clear();
                return false;
            }
            mergePosition = position;
        }
        chainEntries.append(entry);
    }

    if (splitPosition < 0 || mergePosition < 0 || splitPosition >= mergePosition) {
        error = QString("Invalid split/merge topology (split=%1, merge=%2)")
                    .arg(splitPosition).arg(mergePosition);
        chainEntries.clear();
        return false;
    }

    for (int position = 0; position < chainEntries.size(); ++position) {
        Entry &entry = chainEntries[position];
        if (position < splitPosition) {
            entry.path = Common;
            entry.region = ChainRegion::CommonPrefix;
            prefixEntries.append(entry);
        } else if (position > splitPosition && position < mergePosition) {
            entry.path = entry.rawValue.toInt(nullptr, 16) >= 0x40 ? PathB : PathA;
            if (entry.path == PathB) {
                entry.region = ChainRegion::PathB;
                pathBEntries.append(entry);
            } else {
                entry.region = ChainRegion::PathA;
                pathAEntries.append(entry);
            }
        } else if (position > mergePosition) {
            entry.path = Common;
            entry.region = ChainRegion::CommonSuffix;
            suffixEntries.append(entry);
        } else {
            entry.path = Common;
            entry.region = entry.isSplit ? ChainRegion::CommonPrefix
                                         : ChainRegion::CommonSuffix;
        }
    }

    ChainSnapshot parsed;
    parsed.commonPrefix = prefixEntries;
    parsed.pathA = pathAEntries;
    parsed.pathB = pathBEntries;
    parsed.commonSuffix = suffixEntries;
    parsed.split = chainEntries.at(splitPosition);
    parsed.merge = chainEntries.at(mergePosition);
    parsed.split.region = ChainRegion::CommonPrefix;
    parsed.merge.region = ChainRegion::CommonSuffix;
    parsed.revision = nextRevision;
    parsed.patchIdentity = deviceConfirmed
        ? QString("%1:%2")
              .arg(sysxIO->getLoadedBank()).arg(sysxIO->getLoadedPatch())
        : QString();
    parsed.channelMode = mode;
    parsed.channelSelect = selectedChannel;

    QString validationError;
    if (!modernSignalChainSerializer::validate(parsed, &validationError)) {
        error = validationError;
        chainEntries.clear();
        prefixEntries.clear();
        pathAEntries.clear();
        pathBEntries.clear();
        suffixEntries.clear();
        return false;
    }
    applySnapshot(parsed);
    currentSourceState = deviceConfirmed
        ? SourceState::DeviceConfirmed : SourceState::TemporaryBuffer;
    return true;
}

bool modernSignalChainModel::parseRawBytes(
    const QList<QString> &rawBytes,
    ChainSnapshot *result,
    QString *parseError,
    quint64 revision,
    const QString &patchIdentity,
    ChannelMode parsedMode,
    int parsedChannelSelect)
{
    const auto fail = [parseError](const QString &message) {
        if (parseError)
            *parseError = message;
        return false;
    };
    if (!result)
        return fail("Snapshot output is null");
    if (rawBytes.size() != chainLength)
        return fail(QString("Signal chain requires exactly 18 bytes; received %1")
                    .arg(rawBytes.size()));

    QList<Entry> ordered;
    int splitPosition = -1;
    int mergePosition = -1;
    for (int position = 0; position < rawBytes.size(); ++position) {
        const QString raw = rawBytes.at(position).trimmed().toUpper();
        bool ok = false;
        const int rawByte = raw.toInt(&ok, 16);
        if (!ok || raw.size() != 2)
            return fail(QString("Chain byte %1 is invalid: %2")
                        .arg(position).arg(raw));
        const int moduleId = rawByte >= 0x40 && rawByte <= 0x4F
            ? rawByte - 0x40 : rawByte;
        if (moduleId < 0 || moduleId > 0x11)
            return fail(QString("Chain byte %1 has unsupported module value %2")
                        .arg(position).arg(raw));

        Entry entry;
        entry.rawValue = raw;
        entry.originalRawValue = raw;
        entry.moduleId = moduleId;
        entry.name = ::displayName(moduleId, QString());
        entry.originalPosition = position;
        entry.movable = isMovableModule(moduleId);
        entry.isSplit = moduleId == 0x10;
        entry.isMerge = moduleId == 0x11;
        entry.isPreampA = moduleId == 0x02;
        entry.isPreampB = moduleId == 0x03;
        if (entry.isSplit) {
            if (splitPosition >= 0)
                return fail("Signal chain contains more than one SPLIT");
            splitPosition = position;
        }
        if (entry.isMerge) {
            if (mergePosition >= 0)
                return fail("Signal chain contains more than one MERGE");
            mergePosition = position;
        }
        ordered.append(entry);
    }

    if (splitPosition < 0 || mergePosition < 0)
        return fail("Signal chain requires exactly one SPLIT and one MERGE");
    if (splitPosition >= mergePosition)
        return fail(QString("SPLIT must precede MERGE (split=%1, merge=%2)")
                    .arg(splitPosition).arg(mergePosition));

    ChainSnapshot candidate;
    candidate.revision = revision;
    candidate.patchIdentity = patchIdentity;
    candidate.channelMode = parsedMode;
    candidate.channelSelect = parsedChannelSelect;
    for (int position = 0; position < ordered.size(); ++position) {
        Entry entry = ordered.at(position);
        if (position < splitPosition) {
            entry.path = Common;
            entry.region = ChainRegion::CommonPrefix;
            candidate.commonPrefix.append(entry);
        } else if (position == splitPosition) {
            entry.path = Common;
            entry.region = ChainRegion::CommonPrefix;
            candidate.split = entry;
        } else if (position < mergePosition) {
            if (entry.rawValue.toInt(nullptr, 16) >= 0x40) {
                entry.path = PathB;
                entry.region = ChainRegion::PathB;
                candidate.pathB.append(entry);
            } else {
                entry.path = PathA;
                entry.region = ChainRegion::PathA;
                candidate.pathA.append(entry);
            }
        } else if (position == mergePosition) {
            entry.path = Common;
            entry.region = ChainRegion::CommonSuffix;
            candidate.merge = entry;
        } else {
            entry.path = Common;
            entry.region = ChainRegion::CommonSuffix;
            candidate.commonSuffix.append(entry);
        }
    }

    QString validationError;
    if (!modernSignalChainSerializer::validate(candidate, &validationError))
        return fail(validationError);
    *result = candidate;
    if (parseError)
        parseError->clear();
    return true;
}

bool modernSignalChainModel::replaceSnapshot(const ChainSnapshot &snapshot,
                                             QString *replaceError)
{
    QString validationError;
    if (!modernSignalChainSerializer::validate(snapshot, &validationError)) {
        if (replaceError)
            *replaceError = validationError;
        return false;
    }
    applySnapshot(snapshot);
    if (replaceError)
        replaceError->clear();
    return true;
}

void modernSignalChainModel::applySnapshot(const ChainSnapshot &snapshot)
{
    confirmedSnapshot = snapshot;
    prefixEntries = snapshot.commonPrefix;
    pathAEntries = snapshot.pathA;
    pathBEntries = snapshot.pathB;
    suffixEntries = snapshot.commonSuffix;
    mode = snapshot.channelMode;
    selectedChannel = snapshot.channelSelect;
    chainEntries = flattenedEntries(snapshot);
    valid = true;
    error.clear();
}

QList<modernSignalChainModel::Entry> modernSignalChainModel::flattenedEntries(
    const ChainSnapshot &snapshot)
{
    QList<Entry> result = snapshot.commonPrefix;
    result.append(snapshot.split);
    result.append(snapshot.pathA);
    result.append(snapshot.pathB);
    result.append(snapshot.merge);
    result.append(snapshot.commonSuffix);
    return result;
}

void modernSignalChainModel::logInterpretedChain() const
{
    if (!valid) {
        qWarning().noquote() << "[Modern Signal Chain] unavailable:" << error;
        return;
    }

    qInfo().noquote() << "[Modern Signal Chain] 18 bytes from Structure 0B 00 00";
    qInfo().noquote() << "  SOURCE:"
                      << (currentSourceState == SourceState::DeviceConfirmed
                              ? "DEVICE CONFIRMED" : "TEMPORARY BUFFER");
    qInfo().noquote() << "  MODE:" << channelModeName(mode)
                      << "| CHANNEL SELECT:"
                      << (selectedChannel == 0 ? "A" : selectedChannel == 1 ? "B" : "N/A");
    for (const Entry &entry : chainEntries) {
        const char *pathName = entry.path == PathA ? "A"
            : entry.path == PathB ? "B" : "COMMON";
        qInfo().noquote() << QString("  [%1] raw=%2 id=%3 path=%4 name=%5 movable=%6")
            .arg(entry.originalPosition, 2, 10, QChar('0'))
            .arg(entry.rawValue)
            .arg(entry.moduleId, 2, 16, QChar('0'))
            .arg(pathName)
            .arg(QString("%1 (%2)").arg(::displayName(entry.moduleId, entry.name), entry.name))
            .arg(entry.movable ? "yes" : "no");
    }
    qInfo().noquote() << "  COMMON PREFIX:" << joinedNames(prefixEntries);
    qInfo().noquote() << "  SPLIT";
    qInfo().noquote() << "  PATH A:" << joinedNames(pathAEntries);
    qInfo().noquote() << "  PATH B:" << joinedNames(pathBEntries);
    qInfo().noquote() << "  MERGE";
    qInfo().noquote() << "  COMMON SUFFIX:" << joinedNames(suffixEntries);
}

bool modernSignalChainModel::isValid() const { return valid; }
modernSignalChainModel::SourceState modernSignalChainModel::sourceState() const
{ return currentSourceState; }
bool modernSignalChainModel::isDeviceConfirmed() const
{ return currentSourceState == SourceState::DeviceConfirmed; }
QString modernSignalChainModel::errorString() const { return error; }
modernSignalChainModel::ChannelMode modernSignalChainModel::channelMode() const { return mode; }
int modernSignalChainModel::channelSelect() const { return selectedChannel; }
modernSignalChainModel::ChainSnapshot modernSignalChainModel::snapshot() const
{ return confirmedSnapshot; }
QList<modernSignalChainModel::Entry> modernSignalChainModel::entries() const { return chainEntries; }
QList<modernSignalChainModel::Entry> modernSignalChainModel::commonPrefix() const { return prefixEntries; }
QList<modernSignalChainModel::Entry> modernSignalChainModel::pathA() const { return pathAEntries; }
QList<modernSignalChainModel::Entry> modernSignalChainModel::pathB() const { return pathBEntries; }
QList<modernSignalChainModel::Entry> modernSignalChainModel::commonSuffix() const { return suffixEntries; }

bool modernSignalChainModel::isMovableModule(int moduleId)
{
    return moduleId >= 0 && moduleId <= 0x0F
        && moduleId != 0x02 && moduleId != 0x03;
}

QString modernSignalChainModel::channelModeName(ChannelMode channelMode)
{
    switch (channelMode) {
    case Single: return "SINGLE";
    case DualMono: return "DUAL MONO";
    case DualLR: return "DUAL L/R";
    case Dynamic: return "DYNAMIC";
    default: return "UNKNOWN";
    }
}

QString modernSignalChainModel::displayName(const Entry &entry)
{
    return ::displayName(entry.moduleId, entry.name);
}
