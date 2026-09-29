#include "ai/knowledge/GT10CapabilityService.h"

#include "MidiTable.h"
#include "ai/PatchParameterCatalog.h"

#include <QMap>
#include <QRegularExpression>

namespace Ai {
namespace Knowledge {
namespace {

QStringList namesFromNode(const Midi &node)
{
    QStringList result;
    for (const Midi &value : node.level) {
        if (value.value == QLatin1String("range"))
            continue;
        const QString name = value.name.trimmed();
        if (!name.isEmpty() && !result.contains(name, Qt::CaseInsensitive))
            result.append(name);
    }
    return result;
}

void appendLeafNames(const Midi &node, QStringList *result)
{
    if (node.level.isEmpty()) {
        const QString name = node.name.trimmed();
        if (!name.isEmpty() && node.value != QLatin1String("range")
            && !result->contains(name, Qt::CaseInsensitive))
            result->append(name);
        return;
    }
    for (const Midi &child : node.level)
        appendLeafNames(child, result);
}

QStringList pathFor(const QString &block, const QString &offset)
{
    if (block.size() != 4)
        return {};
    return {block.left(2), block.mid(2, 2), offset};
}

bool matches(const QString &candidate, const QString &query)
{
    const QString a = GT10CapabilityService::normalize(candidate);
    const QString b = GT10CapabilityService::normalize(query);
    QString compactA = a;
    QString compactB = b;
    compactA.remove(' ');
    compactB.remove(' ');
    return !b.isEmpty() && (a == b || a.contains(b) || b.contains(a)
        || compactA.contains(compactB) || compactB.contains(compactA));
}

} // namespace

QString GT10CapabilityService::deviceModel() const
{
    Midi identity;
    if (MidiTable::Instance()->lookup("Header", {"03", "00"}, &identity)
        && !identity.name.trimmed().isEmpty())
        return identity.name.trimmed();
    return QString();
}

QStringList GT10CapabilityService::blockIds() const
{
    QStringList result;
    for (const EffectDescriptor &effect : PatchParameterCatalog::effects({}))
        result.append(effect.id);
    return result;
}

bool GT10CapabilityService::hasBlock(const QString &blockId) const
{
    for (const QString &id : blockIds()) {
        if (normalize(id) == normalize(blockId))
            return true;
    }
    return false;
}

GT10CapabilityService::CatalogAddress
GT10CapabilityService::typeAddress(const QString &blockId) const
{
    for (const EffectDescriptor &effect : PatchParameterCatalog::effects({})) {
        if (normalize(effect.id) == normalize(blockId)
            && !effect.type.block.isEmpty())
            return {effect.type.block, effect.type.offset};
    }
    return {};
}

QStringList GT10CapabilityService::valuesAt(
    const CatalogAddress &address) const
{
    Midi node;
    if (address.block.isEmpty()
        || !MidiTable::Instance()->lookup(
            "Structure", pathFor(address.block, address.offset), &node))
        return {};
    return namesFromNode(node);
}

QStringList GT10CapabilityService::effectTypesForBlock(
    const QString &blockId) const
{
    return valuesAt(typeAddress(blockId));
}

bool GT10CapabilityService::hasEffectType(
    const QString &blockId, const QString &effectType) const
{
    for (const QString &candidate : effectTypesForBlock(blockId)) {
        if (matches(candidate, effectType))
            return true;
    }
    return false;
}

QStringList GT10CapabilityService::blocksForEffectType(
    const QString &effectType) const
{
    QStringList result;
    for (const QString &blockId : blockIds()) {
        if (hasEffectType(blockId, effectType))
            result.append(blockId);
    }
    return result;
}

QString GT10CapabilityService::parameterLabel(
    const QString &block, const QString &offset) const
{
    Midi node;
    if (!MidiTable::Instance()->lookup(
            "Structure", pathFor(block, offset), &node))
        return {};
    const QString custom = node.customdesc.trimmed();
    return custom.isEmpty() ? node.name.trimmed() : custom;
}

QStringList GT10CapabilityService::parametersForEffect(
    const QString &blockId, const QString &effectType) const
{
    const CatalogAddress type = typeAddress(blockId);
    Midi typeNode;
    int typeRaw = -1;
    if (!type.block.isEmpty()
        && MidiTable::Instance()->lookup(
            "Structure", pathFor(type.block, type.offset), &typeNode)) {
        for (const Midi &entry : typeNode.level) {
            if (matches(entry.name, effectType)) {
                bool ok = false;
                typeRaw = entry.value.toInt(&ok, 16);
                if (!ok)
                    typeRaw = -1;
                break;
            }
        }
    }
    if (!effectType.trimmed().isEmpty() && typeRaw < 0)
        return {};

    QMap<QString, QString> blocks;
    for (int block = 0; block <= 0x0A; ++block)
        blocks.insert(QString("%1%2").arg(block, 2, 16, QChar('0')).arg("00").toUpper(),
                      QString(256, QLatin1Char('0')));
    if (!type.block.isEmpty() && typeRaw >= 0) {
        QString payload = blocks.value(type.block);
        const int offset = type.offset.toInt(nullptr, 16) * 2;
        payload.replace(offset, 2,
                        QString("%1").arg(typeRaw, 2, 16, QChar('0')).toUpper());
        blocks[type.block] = payload;
    }

    QStringList result;
    for (const EffectDescriptor &effect : PatchParameterCatalog::effects(blocks)) {
        if (normalize(effect.id) != normalize(blockId))
            continue;
        for (const ParameterDescriptor &parameter : effect.parameters) {
            if (!PatchParameterCatalog::isAvailable(parameter, blocks))
                continue;
            QString label = parameterLabel(parameter.address.block,
                                           parameter.address.offset);
            if (label.isEmpty())
                label = parameter.id;
            if (!result.contains(label, Qt::CaseInsensitive))
                result.append(label);
        }
    }
    return result;
}

ParameterCapability GT10CapabilityService::parameterCapability(
    const QString &blockId, const QString &effectType,
    const QString &parameter) const
{
    ParameterCapability result;
    result.blockId = blockId;
    result.effectType = effectType;
    for (const QString &name : parametersForEffect(blockId, effectType)) {
        if (matches(name, parameter)) {
            result.id = normalize(name).replace(' ', '_');
            result.name = name;
            result.available = true;
            result.sourceKind = SourceKind::MidiTable;
            break;
        }
    }
    if (!result.available)
        return result;

    const CatalogAddress type = typeAddress(blockId);
    int typeRaw = -1;
    Midi typeNode;
    if (MidiTable::Instance()->lookup(
            "Structure", pathFor(type.block, type.offset), &typeNode)) {
        for (const Midi &entry : typeNode.level) {
            if (matches(entry.name, effectType))
                typeRaw = entry.value.toInt(nullptr, 16);
        }
    }
    QMap<QString, QString> blocks;
    for (int block = 0; block <= 0x0A; ++block)
        blocks.insert(QString("%1%2").arg(block, 2, 16, QChar('0')).arg("00").toUpper(),
                      QString(256, QLatin1Char('0')));
    if (!type.block.isEmpty() && typeRaw >= 0) {
        QString payload = blocks.value(type.block);
        payload.replace(type.offset.toInt(nullptr, 16) * 2, 2,
                        QString("%1").arg(typeRaw, 2, 16, QChar('0')).toUpper());
        blocks[type.block] = payload;
    }
    for (const EffectDescriptor &effect : PatchParameterCatalog::effects(blocks)) {
        if (normalize(effect.id) != normalize(blockId))
            continue;
        for (const ParameterDescriptor &descriptor : effect.parameters) {
            const QString label = parameterLabel(descriptor.address.block,
                                                 descriptor.address.offset);
            if (!matches(label.isEmpty() ? descriptor.id : label, parameter))
                continue;
            Midi node;
            if (!MidiTable::Instance()->lookup(
                    "Structure", pathFor(descriptor.address.block,
                                          descriptor.address.offset), &node))
                return result;
            for (const Midi &entry : node.level) {
                if (entry.value == QLatin1String("range")) {
                    const QStringList parts = entry.name.split('/');
                    if (parts.size() >= 4) {
                        result.minimum = parts.at(2);
                        result.maximum = parts.at(3);
                    }
                } else if (!entry.name.trimmed().isEmpty()) {
                    result.enumValues.append(entry.name.trimmed());
                }
            }
            return result;
        }
    }
    return result;
}

int GT10CapabilityService::assignCount() const
{
    return 8;
}

QStringList GT10CapabilityService::assignTargets() const
{
    Midi root;
    QStringList result;
    if (MidiTable::Instance()->lookup("Structure", {"0B", "00", "21"}, &root))
        appendLeafNames(root, &result);
    return result;
}

QStringList GT10CapabilityService::assignSources() const
{
    return valuesAt({"0B00", "27"});
}

QStringList GT10CapabilityService::assignSourceModes() const
{
    return valuesAt({"0B00", "28"});
}

bool GT10CapabilityService::supportsAssignTarget(const QString &target) const
{
    for (const QString &candidate : assignTargets()) {
        if (matches(candidate, target))
            return true;
    }
    return false;
}

bool GT10CapabilityService::supportsAssignSource(const QString &source) const
{
    for (const QString &candidate : assignSources()) {
        if (matches(candidate, source))
            return true;
    }
    return false;
}

QList<ControllerCapability> GT10CapabilityService::controllers() const
{
    return {
        {"ctl1", "CTL1", DataScope::Patch, supportsAssignSource("CTL1"), "Control/Assign"},
        {"ctl2", "CTL2", DataScope::Patch, supportsAssignSource("CTL2"), "Control/Assign"},
        {"exp", "EXP", DataScope::Patch, supportsAssignSource("EXP1"), "Expression / Assign"},
        {"exp_sw", "EXP SW", DataScope::Patch, supportsAssignSource("EXP1 Pdl SW"), "Control/Assign"},
        {"ctl3", "CTL3", DataScope::Patch, supportsAssignSource("CTL3"), "External Assign Source"},
        {"ctl4", "CTL4", DataScope::Patch, supportsAssignSource("CTL4"), "External Assign Source"},
        {"exp2", "EXP2", DataScope::Patch, supportsAssignSource("EXP2"), "External Assign Source"},
        {"foot1", "FOOT 1", DataScope::System, false, "Pedalboard / System"},
        {"foot2", "FOOT 2", DataScope::System, false, "Pedalboard / System"},
        {"foot3", "FOOT 3", DataScope::System, false, "Pedalboard / System"},
        {"foot4", "FOOT 4", DataScope::System, false, "Pedalboard / System"},
        {"bank_up", "BANK UP", DataScope::System, false, "Pedalboard / System"},
        {"bank_down", "BANK DOWN", DataScope::System, false, "Pedalboard / System"}
    };
}

ControllerCapability GT10CapabilityService::controller(
    const QString &idOrAlias) const
{
    for (const ControllerCapability &item : controllers()) {
        if (matches(item.id, idOrAlias) || matches(item.name, idOrAlias))
            return item;
    }
    return {};
}

QString GT10CapabilityService::normalize(QString text)
{
    text = text.normalized(QString::NormalizationForm_D);
    text.remove(QRegularExpression(QStringLiteral("[\\x{0300}-\\x{036f}]")));
    text = text.toLower();
    text.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), " ");
    text = text.simplified();
    text.replace(QRegularExpression(QStringLiteral("\\bctrl\\b")), "ctl");
    text.replace(QRegularExpression(QStringLiteral("\\bpdl\\b")), "pedal");
    text.replace(QRegularExpression(QStringLiteral("\\bdd\\b")), "delay");
    text.replace(QRegularExpression(QStringLiteral("\\brv\\b")), "reverb");
    text.replace(QRegularExpression(QStringLiteral("\\bce\\b")), "chorus");
    text.replace(QRegularExpression(QStringLiteral("\\btr\\b")), "tremolo");
    return text;
}

QString GT10CapabilityService::scopeName(DataScope scope)
{
    switch (scope) {
    case DataScope::Patch: return QStringLiteral("Patch");
    case DataScope::System: return QStringLiteral("System");
    case DataScope::Mixed: return QStringLiteral("Mixed");
    case DataScope::Unknown: return QStringLiteral("Unknown");
    }
    return QStringLiteral("Unknown");
}

} // namespace Knowledge
} // namespace Ai
