#include "modernPedalboardModel.h"

#include "MidiTable.h"
#include "SysxIO.h"
#include "globalVariables.h"
#include "modernAssignModel.h"

#include <QDebug>

namespace {
const QString kSystem = "System";
const QString kStructure = "Structure";
const QString kUnavailable = QString::fromUtf8("—");

QString rawHex(int raw)
{
    return QString("%1").arg(raw, 2, 16, QChar('0')).toUpper();
}

QString scopeName(ModernPedalboardModel::DataScope scope)
{
    return scope == ModernPedalboardModel::DataScope::System
        ? "SYSTEM" : "PATCH";
}

QString stateName(ModernPedalboardModel::LogicalState state)
{
    switch (state) {
    case ModernPedalboardModel::LogicalState::Off: return "OFF";
    case ModernPedalboardModel::LogicalState::On: return "ON";
    case ModernPedalboardModel::LogicalState::Momentary: return "MOMENTARY";
    case ModernPedalboardModel::LogicalState::Unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}
}

bool ModernPedalboardModel::bufferContains(
    DataScope scope, const QString &bank, const QString &middle,
    const QString &address) const
{
    bool ok = false;
    const int offset = address.toInt(&ok, 16);
    if (!ok)
        return false;

    const SysxData source = scope == DataScope::System
        ? SysxIO::Instance()->getSystemSource()
        : SysxIO::Instance()->getFileSource();
    const int blockIndex = source.address.indexOf(bank + middle);
    return blockIndex >= 0 && blockIndex < source.hex.size()
        && sysxDataOffset + offset < source.hex.at(blockIndex).size();
}

ModernPedalboardModel::StateSource
ModernPedalboardModel::stateSourceFor(int functionRaw) const
{
    switch (functionRaw) {
    case 0x01: return {"01", "00", "02"}; // Channel A/B
    case 0x02: return {"00", "00", "77"}; // OD/DS Solo
    case 0x05: return {"00", "00", "40"}; // Compressor
    case 0x06: return {"00", "00", "70"}; // OD/DS
    case 0x07: return {"01", "00", "00"}; // PreAmp
    case 0x08: return {"01", "00", "70"}; // Equalizer
    case 0x09: return {"02", "00", "00"}; // FX-1
    case 0x0A: return {"06", "00", "00"}; // FX-2
    case 0x0B: return {"0A", "00", "00"}; // Delay
    case 0x0C: return {"0A", "00", "20"}; // Chorus
    case 0x0D: return {"0A", "00", "30"}; // Reverb
    case 0x0E: return {"0A", "00", "40"}; // Pedal FX
    case 0x0F: return {"0A", "00", "79"}; // Send/Return
    case 0x10: return {"0A", "00", "69"}; // Amp CTL
    default: return {};
    }
}

ModernPedalboardModel::LogicalState
ModernPedalboardModel::classifyFunction(int functionRaw, DataScope scope,
                                       QString *stateAddress) const
{
    if (functionRaw == 0x00)
        return LogicalState::Off;

    const StateSource source = stateSourceFor(functionRaw);
    if (!source.address.isEmpty()) {
        *stateAddress = source.bank + " " + source.middle + " " + source.address;
        if (!bufferContains(DataScope::Patch, source.bank,
                            source.middle, source.address))
            return LogicalState::Unknown;
        return SysxIO::Instance()->getSourceValue(
                   kStructure, source.bank, source.middle, source.address) != 0
            ? LogicalState::On : LogicalState::Off;
    }

    // The direct-control catalog has MANUAL at 0x12, shifting the following
    // actions by one compared with the SYSTEM Manual Mode catalog.
    if (scope == DataScope::System) {
        switch (functionRaw) {
        case 0x14: // PL Clear
        case 0x16: // BPM Tap
        case 0x17: // Delay Tap
        case 0x18: // MIDI Start
        case 0x19: // MMC Play
        case 0x1A: case 0x1B: case 0x1C: case 0x1D: // Level
        case 0x1E: case 0x1F: case 0x20: case 0x21: // Number/Bank
            return LogicalState::Momentary;
        default:
            return LogicalState::Unknown;
        }
    }

    switch (functionRaw) {
    case 0x15: // PL Clear
    case 0x17: // BPM Tap
    case 0x18: // Delay Tap
    case 0x19: // MIDI Start
    case 0x1A: // MMC Play
    case 0x1B: case 0x1C: case 0x1D: case 0x1E: // Level
    case 0x1F: case 0x20: case 0x21: case 0x22: // Number/Bank
    case 0x23: // LED momentary
        return LogicalState::Momentary;
    default:
        return LogicalState::Unknown;
    }
}

ModernPedalboardModel::ControlState ModernPedalboardModel::readControl(
    ControlId id, const QString &label, DataScope scope,
    const QString &bank, const QString &middle, const QString &address,
    NavigationTarget editor,
    bool sourceAvailable) const
{
    ControlState result;
    result.id = id;
    result.label = label;
    result.scope = scope;
    result.functionAddress = bank + " " + middle + " " + address;
    result.editor = editor;
    result.functionName = kUnavailable;
    result.dataValid = sourceAvailable
        && bufferContains(scope, bank, middle, address);
    if (!result.dataValid)
        return result;

    const QString area = scope == DataScope::System ? kSystem : kStructure;
    result.functionRaw = SysxIO::Instance()->getSourceValue(
        area, bank, middle, address);
    result.functionName = MidiTable::Instance()->getValue(
        area, bank, middle, address, rawHex(result.functionRaw)).trimmed();
    if (result.functionName.isEmpty())
        result.functionName = kUnavailable;
    result.state = classifyFunction(
        result.functionRaw, scope, &result.stateAddress);
    return result;
}

QList<int> ModernPedalboardModel::relatedAssignsFor(
    ControlId id, bool backendConnected, bool backendHasPatchData) const
{
    int sourceRaw = -1;
    switch (id) {
    case ControlId::Ctl1: sourceRaw = 0x01; break;
    case ControlId::Ctl2: sourceRaw = 0x02; break;
    case ControlId::ExpSwitch: sourceRaw = 0x03; break;
    default: return {};
    }

    ModernAssignModel assignModel;
    assignModel.refresh(backendConnected, backendHasPatchData);
    QList<int> related;
    if (!assignModel.isAvailable())
        return related;
    for (int index = 0; index < assignModel.count(); ++index) {
        const ModernAssignModel::Record &record = assignModel.record(index);
        if (record.valid && record.source == sourceRaw)
            related.append(record.number);
    }
    return related;
}

void ModernPedalboardModel::refresh(bool backendConnected,
                                    bool backendHasPatchData)
{
    controls.clear();
    controls.reserve(9);
    const bool systemAvailable = backendConnected;
    const bool patchAvailable = backendConnected && backendHasPatchData;

    controls.append(readControl(ControlId::Foot1, "FOOT 1",
        DataScope::System, "00", "02", "50", NavigationTarget::Pedalboard,
        systemAvailable));
    controls.append(readControl(ControlId::Foot2, "FOOT 2",
        DataScope::System, "00", "02", "51", NavigationTarget::Pedalboard,
        systemAvailable));
    controls.append(readControl(ControlId::Foot3, "FOOT 3",
        DataScope::System, "00", "02", "52", NavigationTarget::Pedalboard,
        systemAvailable));
    controls.append(readControl(ControlId::Foot4, "FOOT 4",
        DataScope::System, "00", "02", "53", NavigationTarget::Pedalboard,
        systemAvailable));
    controls.append(readControl(ControlId::BankUp, "BANK UP",
        DataScope::System, "00", "02", "54", NavigationTarget::Pedalboard,
        systemAvailable));
    controls.append(readControl(ControlId::BankDown, "BANK DOWN",
        DataScope::System, "00", "02", "55", NavigationTarget::Pedalboard,
        systemAvailable));
    controls.append(readControl(ControlId::Ctl1, "CTL1",
        DataScope::Patch, "0A", "00", "47", NavigationTarget::ControlAssign,
        patchAvailable));
    controls.append(readControl(ControlId::Ctl2, "CTL2",
        DataScope::Patch, "0A", "00", "48", NavigationTarget::ControlAssign,
        patchAvailable));
    controls.append(readControl(ControlId::ExpSwitch, "EXP SW",
        DataScope::Patch, "0A", "00", "46", NavigationTarget::ControlAssign,
        patchAvailable));

    for (ControlState &item : controls) {
        item.relatedAssigns = relatedAssignsFor(
            item.id, backendConnected, backendHasPatchData);
    }
    logIfChanged();
}

int ModernPedalboardModel::count() const
{
    return controls.size();
}

const ModernPedalboardModel::ControlState &
ModernPedalboardModel::control(int index) const
{
    return controls.at(index);
}

const ModernPedalboardModel::ControlState *
ModernPedalboardModel::control(ControlId id) const
{
    for (const ControlState &item : controls) {
        if (item.id == id)
            return &item;
    }
    return nullptr;
}

QString ModernPedalboardModel::snapshot() const
{
    QStringList lines;
    for (const ControlState &item : controls) {
        lines.append(QString("%1|%2|%3|%4|%5|%6|%7")
            .arg(item.label)
            .arg(item.functionRaw)
            .arg(item.functionName)
            .arg(scopeName(item.scope))
            .arg(stateName(item.state))
            .arg(item.stateAddress)
            .arg(item.dataValid));
    }
    return lines.join('\n');
}

void ModernPedalboardModel::logIfChanged()
{
    const QString current = snapshot();
    if (current == lastLoggedSnapshot)
        return;
    lastLoggedSnapshot = current;

    qDebug().noquote() << "PEDALBOARD MODEL";
    for (const ControlState &item : controls) {
        qDebug().noquote()
            << QString("%1: raw=%2 function=\"%3\" scope=%4 "
                       "state=%5 stateAddress=\"%6\" valid=%7 assigns=%8")
                .arg(item.label)
                .arg(item.functionRaw >= 0 ? rawHex(item.functionRaw) : "--")
                .arg(item.functionName)
                .arg(scopeName(item.scope))
                .arg(stateName(item.state))
                .arg(item.stateAddress)
                .arg(item.dataValid ? "true" : "false")
                .arg([&item]() {
                    QStringList values;
                    for (int number : item.relatedAssigns)
                        values.append(QString::number(number));
                    return values.isEmpty() ? "-" : values.join(',');
                }());
    }
}
