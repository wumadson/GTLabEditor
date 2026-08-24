#include "modernAssignModel.h"

#include "MidiTable.h"
#include "SysxIO.h"
#include "globalVariables.h"

namespace {
const QString kStructure = "Structure";
const QString kMiddleByte = "00";
const QString kTargetCatalogBank = "0B";
const QString kTargetCatalogAddress = "21";
}

ModernAssignModel::Address ModernAssignModel::baseForIndex(int index) const
{
    Address address;
    if (index < 6) {
        address.bank = "0B";
        address.offset = 0x20 + index * 0x10;
    } else {
        address.bank = "0C";
        address.offset = (index - 6) * 0x10;
    }
    return address;
}

QString ModernAssignModel::hexAddress(int value) const
{
    return QString("%1").arg(value, 2, 16, QChar('0')).toUpper();
}

bool ModernAssignModel::bufferContains(const QString &bank, int offset,
                                       int byteCount) const
{
    const SysxData source = SysxIO::Instance()->getFileSource();
    const int addressIndex = source.address.indexOf(bank + kMiddleByte);
    if (addressIndex < 0 || addressIndex >= source.hex.size())
        return false;
    const int first = sysxDataOffset + offset;
    const int last = first + byteCount - 1;
    return first >= 0 && last < source.hex.at(addressIndex).size();
}

int ModernAssignModel::readValue(const QString &bank, int offset) const
{
    return SysxIO::Instance()->getSourceValue(
        kStructure, bank, kMiddleByte, hexAddress(offset));
}

QString ModernAssignModel::displayValue(const QString &bank, int offset,
                                        int raw) const
{
    return MidiTable::Instance()->getValue(
        kStructure, bank, kMiddleByte, hexAddress(offset),
        hexAddress(raw));
}

QString ModernAssignModel::targetValueDisplay(
    const QString &targetBank, const QString &targetAddress, int raw) const
{
    return MidiTable::Instance()->getValue(
        kStructure, targetBank, kMiddleByte, targetAddress,
        hexAddress(raw));
}

ModernAssignModel::Record ModernAssignModel::readRecord(int index) const
{
    Record result;
    const Address base = baseForIndex(index);
    result.number = index + 1;
    result.bank = base.bank;
    result.baseAddress = hexAddress(base.offset);
    result.valid = bufferContains(base.bank, base.offset, 0x10);
    if (!result.valid)
        return result;

    result.enabled = readValue(base.bank, base.offset) == 1;
    result.targetId = readValue(base.bank, base.offset + 0x01);
    result.targetMin = readValue(base.bank, base.offset + 0x03);
    result.targetMax = readValue(base.bank, base.offset + 0x05);
    result.source = readValue(base.bank, base.offset + 0x07);
    result.sourceDisplay = displayValue(
        base.bank, base.offset + 0x07, result.source).trimmed();
    result.sourceModeDisplay = displayValue(
        base.bank, base.offset + 0x08,
        readValue(base.bank, base.offset + 0x08)).trimmed();
    result.activeRangeLow = readValue(base.bank, base.offset + 0x09);
    result.activeRangeHigh = readValue(base.bank, base.offset + 0x0A);

    if (result.targetId >= 0 && result.targetId <= 618) {
        const int high = result.targetId / 128;
        const int low = result.targetId % 128;
        const Midi target = MidiTable::Instance()->getMidiMap(
            kStructure, kTargetCatalogBank, kMiddleByte,
            kTargetCatalogAddress, hexAddress(high), hexAddress(low));
        result.targetName = target.name.trimmed();
        if (!target.desc.isEmpty() && !target.customdesc.isEmpty()) {
            result.targetMinDisplay = targetValueDisplay(
                target.desc, target.customdesc, result.targetMin).trimmed();
            result.targetMaxDisplay = targetValueDisplay(
                target.desc, target.customdesc, result.targetMax).trimmed();
        }
    }

    if (result.source == 0x07) {
        result.sourceKind = SourceKind::InternalPedal;
        result.internalTriggerDisplay = displayValue(
            base.bank, base.offset + 0x0B,
            readValue(base.bank, base.offset + 0x0B)).trimmed();
        result.internalTimeDisplay = displayValue(
            base.bank, base.offset + 0x0C,
            readValue(base.bank, base.offset + 0x0C)).trimmed();
        result.internalCurveDisplay = displayValue(
            base.bank, base.offset + 0x0D,
            readValue(base.bank, base.offset + 0x0D)).trimmed();
    } else if (result.source == 0x08) {
        result.sourceKind = SourceKind::WavePedal;
        result.waveRateDisplay = displayValue(
            base.bank, base.offset + 0x0E,
            readValue(base.bank, base.offset + 0x0E)).trimmed();
        result.waveFormDisplay = displayValue(
            base.bank, base.offset + 0x0F,
            readValue(base.bank, base.offset + 0x0F)).trimmed();
    } else if (result.source == 0x09) {
        result.sourceKind = SourceKind::InputLevel;
        const int sensitivity = readValue("0C", 0x20);
        result.inputSensitivityDisplay = displayValue(
            "0C", 0x20, sensitivity).trimmed();
    }
    return result;
}

void ModernAssignModel::refresh(bool backendConnected,
                                bool backendHasPatchData)
{
    available = backendConnected && backendHasPatchData
        && SysxIO::Instance()->isConnected()
        && bufferContains("0B", 0x20, 0x60)
        && bufferContains("0C", 0x00, 0x21);
    records.clear();
    records.reserve(8);
    for (int index = 0; index < 8; ++index)
        records.append(available ? readRecord(index) : Record());
}

bool ModernAssignModel::isAvailable() const
{
    return available;
}

int ModernAssignModel::count() const
{
    return records.size();
}

const ModernAssignModel::Record &ModernAssignModel::record(int index) const
{
    return records.at(index);
}
