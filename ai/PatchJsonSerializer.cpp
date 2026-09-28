#include "ai/PatchJsonSerializer.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>

namespace Ai {
namespace {

QJsonObject parameterJson(const ParameterValue &value)
{
    QJsonObject object;
    object.insert("id", value.id);
    object.insert("address", value.address);
    object.insert("available", value.available);
    object.insert("unavailableReason", value.available
        ? QJsonValue(QJsonValue::Null) : QJsonValue(value.unavailableReason));
    object.insert("raw", value.available && value.hasRaw
        ? QJsonValue(value.raw) : QJsonValue(QJsonValue::Null));
    object.insert("rawHex", value.available && value.hasRaw
        ? QJsonValue(value.rawHex) : QJsonValue(QJsonValue::Null));
    object.insert("display", value.available && value.hasRaw
        ? QJsonValue(value.display) : QJsonValue(QJsonValue::Null));
    object.insert("unit", value.available && value.hasRaw
        && !value.unit.isEmpty() ? QJsonValue(value.unit)
                                : QJsonValue(QJsonValue::Null));
    object.insert("enumText", value.available && value.hasRaw
        && !value.enumText.isEmpty() ? QJsonValue(value.enumText)
                                    : QJsonValue(QJsonValue::Null));
    return object;
}

QJsonArray parametersJson(const QList<ParameterValue> &values)
{
    QJsonArray array;
    for (const ParameterValue &value : values)
        array.append(parameterJson(value));
    return array;
}

QJsonObject moduleJson(const SignalChainModule &module)
{
    QJsonObject object;
    object.insert("id", module.id);
    object.insert("name", module.name);
    object.insert("raw", module.raw);
    object.insert("normalized", module.normalized);
    object.insert("position", module.position);
    object.insert("enabled", module.hasEnabled
        ? QJsonValue(module.enabled) : QJsonValue(QJsonValue::Null));
    return object;
}

QJsonArray modulesJson(const QList<SignalChainModule> &modules)
{
    QJsonArray array;
    for (const SignalChainModule &module : modules)
        array.append(moduleJson(module));
    return array;
}

} // namespace

QJsonObject PatchJsonSerializer::toJson(const PatchSnapshot &snapshot)
{
    QJsonObject root;
    root.insert("schemaVersion", snapshot.schemaVersion);

    QJsonObject device;
    device.insert("model", snapshot.device.model);
    device.insert("connected", snapshot.device.connected);
    device.insert("confirmed", snapshot.device.confirmed);
    root.insert("device", device);

    QJsonObject source;
    source.insert("provenance", snapshot.source.provenance);
    source.insert("reliability", snapshot.source.reliability);
    root.insert("source", source);

    QJsonObject patch;
    patch.insert("name", snapshot.patch.name);
    patch.insert("bank", snapshot.patch.bank);
    patch.insert("slot", snapshot.patch.slot);
    patch.insert("displayNumber", snapshot.patch.displayNumber);
    patch.insert("location", snapshot.patch.location);
    patch.insert("dirty", snapshot.patch.dirty);
    root.insert("patch", patch);
    root.insert("patchParameters", parametersJson(snapshot.patchParameters));

    QJsonObject chain;
    chain.insert("commonPrefix", modulesJson(snapshot.signalChain.commonPrefix));
    chain.insert("split", moduleJson(snapshot.signalChain.split));
    chain.insert("pathA", modulesJson(snapshot.signalChain.pathA));
    chain.insert("pathB", modulesJson(snapshot.signalChain.pathB));
    chain.insert("merge", moduleJson(snapshot.signalChain.merge));
    chain.insert("commonSuffix", modulesJson(snapshot.signalChain.commonSuffix));
    chain.insert("channelMode", snapshot.signalChain.channelMode);
    chain.insert("channelSelect", snapshot.signalChain.channelSelect);
    chain.insert("sourceReliability", snapshot.signalChain.sourceReliability);
    root.insert("signalChain", chain);

    root.insert("routing", parametersJson(snapshot.routing));

    QJsonArray effects;
    for (const EffectSnapshot &effect : snapshot.effects) {
        QJsonObject object;
        object.insert("id", effect.id);
        object.insert("enabled", effect.hasEnabled
            ? QJsonValue(effect.enabled) : QJsonValue(QJsonValue::Null));
        object.insert("type", effect.type.id.isEmpty()
            ? QJsonValue(QJsonValue::Null)
            : QJsonValue(parameterJson(effect.type)));
        object.insert("parameters", parametersJson(effect.parameters));
        effects.append(object);
    }
    root.insert("effects", effects);

    QJsonObject controllers;
    controllers.insert("included", snapshot.controllers.included);
    controllers.insert("reason", snapshot.controllers.reason);
    root.insert("controllers", controllers);
    return root;
}

QByteArray PatchJsonSerializer::serialize(const PatchSnapshot &snapshot,
                                          bool indented)
{
    return QJsonDocument(toJson(snapshot)).toJson(
        indented ? QJsonDocument::Indented : QJsonDocument::Compact);
}

} // namespace Ai
