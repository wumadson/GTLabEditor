#include "ai/AIContextBuilder.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

namespace Ai {
namespace {

QString humanValue(const ParameterValue &value)
{
    if (!value.available || !value.hasRaw)
        return QString();
    const QString text = !value.enumText.trimmed().isEmpty()
        ? value.enumText : value.display;
    return AIContextBuilder::normalizeLabel(
        text.isEmpty() ? QString::number(value.raw) : text);
}

QString compactValue(const ParameterValue &value)
{
    QString text = humanValue(value);
    const QString unit = value.unit.trimmed();
    if (unit.isEmpty())
        return text;

    static const QSet<QString> spacedUnits = {
        QStringLiteral("ms"), QStringLiteral("sec"),
        QStringLiteral("BPM"), QStringLiteral("dB"), QStringLiteral("%")
    };
    if (text.trimmed().endsWith(unit)) {
        if (spacedUnits.contains(unit)) {
            text.chop(unit.size());
            text = text.trimmed() + " " + unit;
        }
    } else {
        text += " " + unit;
    }
    return text;
}

QJsonObject detailedParameter(const ParameterValue &value)
{
    QJsonObject object;
    object.insert("id", value.id);
    object.insert("value", humanValue(value));
    if (value.hasRaw)
        object.insert("raw", value.raw);
    if (!value.unit.isEmpty())
        object.insert("unit", value.unit);
    static const QRegularExpression addressPattern(
        QStringLiteral("^[0-9A-Fa-f]{4}:[0-9A-Fa-f]{2}$"));
    if (addressPattern.match(value.address).hasMatch())
        object.insert("address", value.address);
    object.insert("available", value.available);
    if (!value.available) {
        object.insert("unavailableReason", value.unavailableReason);
        object.remove("value");
        object.remove("raw");
        object.remove("unit");
        return object;
    }
    object.insert("rawHex", value.rawHex);
    object.insert("display", value.display);
    if (!value.enumText.isEmpty())
        object.insert("enumText",
                      AIContextBuilder::normalizeLabel(value.enumText));
    return object;
}

QJsonArray detailedParameters(const QList<ParameterValue> &values)
{
    QJsonArray array;
    for (const ParameterValue &value : values)
        array.append(detailedParameter(value));
    return array;
}

QJsonObject compactParameters(const QList<ParameterValue> &values)
{
    QJsonObject object;
    for (const ParameterValue &value : values) {
        if (value.available && value.hasRaw)
            object.insert(value.id, compactValue(value));
    }
    return object;
}

QJsonObject module(const SignalChainModule &value,
                   AIContextDetail detail)
{
    QJsonObject object;
    object.insert("name", AIContextBuilder::normalizeLabel(value.name));
    if (value.hasEnabled)
        object.insert("enabled", value.enabled);
    if (detail == AIContextDetail::Detailed) {
        object.insert("id", value.id);
        object.insert("position", value.position);
        object.insert("raw", value.raw);
        object.insert("normalized", value.normalized);
    }
    return object;
}

QJsonArray modules(const QList<SignalChainModule> &values,
                   AIContextDetail detail)
{
    QJsonArray array;
    for (const SignalChainModule &value : values)
        array.append(module(value, detail));
    return array;
}

QString routingValue(const QList<ParameterValue> &routing,
                     const QString &id)
{
    for (const ParameterValue &value : routing) {
        if (value.id == id)
            return compactValue(value);
    }
    return QString();
}

QString moduleText(const QJsonObject &module)
{
    QString text = module.value("name").toString();
    if (module.contains("enabled"))
        text += module.value("enabled").toBool() ? " [ON]" : " [OFF]";
    return text;
}

QStringList moduleTexts(const QJsonArray &modules)
{
    QStringList result;
    for (const QJsonValue &value : modules)
        result.append(moduleText(value.toObject()));
    return result;
}

QString readableId(QString id)
{
    id.replace(QRegularExpression(QStringLiteral("([a-z0-9])([A-Z])")),
               QStringLiteral("\\1 \\2"));
    if (!id.isEmpty())
        id[0] = id.at(0).toUpper();
    return id;
}

void insertDualDelayParameters(QJsonObject *effect,
                               const QList<ParameterValue> &parameters)
{
    QJsonObject delay1;
    QJsonObject delay2;
    static const QMap<QString, QString> names = {
        {QStringLiteral("Time"), QStringLiteral("time")},
        {QStringLiteral("Feedback"), QStringLiteral("feedback")},
        {QStringLiteral("HighCut"), QStringLiteral("highCut")},
        {QStringLiteral("Level"), QStringLiteral("level")}
    };
    for (const ParameterValue &parameter : parameters) {
        if (!parameter.available || !parameter.hasRaw)
            continue;
        QJsonObject *group = nullptr;
        QString suffix;
        if (parameter.id.startsWith("dualDelay1")) {
            group = &delay1;
            suffix = parameter.id.mid(10);
        } else if (parameter.id.startsWith("dualDelay2")) {
            group = &delay2;
            suffix = parameter.id.mid(10);
        }
        if (group && names.contains(suffix))
            group->insert(names.value(suffix), compactValue(parameter));
        else
            effect->insert(parameter.id, compactValue(parameter));
    }
    if (!delay1.isEmpty())
        effect->insert("delay1", delay1);
    if (!delay2.isEmpty())
        effect->insert("delay2", delay2);
}

void appendCompactObjectText(QStringList *lines, const QJsonObject &object,
                             const QString &indent)
{
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (it.value().isObject()) {
            lines->append(indent + readableId(it.key()) + ":");
            appendCompactObjectText(lines, it.value().toObject(),
                                    indent + "  ");
        } else if (it.value().isString()) {
            lines->append(indent + readableId(it.key()) + ": "
                          + it.value().toString());
        }
    }
}

} // namespace

AIContext AIContextBuilder::build(const PatchSnapshot &snapshot,
                                  AIContextDetail detail) const
{
    AIContext context;
    context.detail = detail;
    QJsonObject &root = context.root;
    root.insert("contextVersion", 1);
    root.insert("detail", detail == AIContextDetail::Compact
        ? "compact" : "detailed");
    root.insert("device", snapshot.device.model);

    QJsonObject patch;
    patch.insert("number", snapshot.patch.displayNumber);
    patch.insert("name", snapshot.patch.name);
    patch.insert("dirty", snapshot.patch.dirty);
    root.insert("patch", patch);

    root.insert("patchParameters", detail == AIContextDetail::Compact
        ? QJsonValue(compactParameters(snapshot.patchParameters))
        : QJsonValue(detailedParameters(snapshot.patchParameters)));

    QJsonObject routing;
    QString mode = routingValue(snapshot.routing, "channelMode");
    if (mode.isEmpty())
        mode = normalizeLabel(snapshot.signalChain.channelMode);
    routing.insert("mode", mode);
    const QString channel = routingValue(snapshot.routing, "channelSelect");
    if (!channel.isEmpty())
        routing.insert("channelSelect", channel);
    if (detail == AIContextDetail::Detailed)
        routing.insert("parameters", detailedParameters(snapshot.routing));
    else {
        const QString delay = routingValue(snapshot.routing, "channelDelay");
        const QString sense = routingValue(snapshot.routing, "dynamicSense");
        if (!delay.isEmpty())
            routing.insert("channelDelay", delay);
        if (!sense.isEmpty())
            routing.insert("dynamicSense", sense);
    }
    root.insert("routing", routing);

    QJsonObject chain;
    chain.insert("commonPrefix",
                 modules(snapshot.signalChain.commonPrefix, detail));
    chain.insert("split", module(snapshot.signalChain.split, detail));
    chain.insert("pathA", modules(snapshot.signalChain.pathA, detail));
    chain.insert("pathB", modules(snapshot.signalChain.pathB, detail));
    chain.insert("merge", module(snapshot.signalChain.merge, detail));
    chain.insert("commonSuffix",
                 modules(snapshot.signalChain.commonSuffix, detail));
    root.insert("signalChain", chain);

    QJsonArray effects;
    for (const EffectSnapshot &effect : snapshot.effects) {
        if (detail == AIContextDetail::Compact
            && effect.id == "preampGlobal")
            continue;
        QJsonObject object;
        object.insert("id", effect.id);
        if (effect.hasEnabled)
            object.insert("enabled", effect.enabled);
        const QString type = humanValue(effect.type);
        if (!type.isEmpty())
            object.insert("type", type);
        if (detail == AIContextDetail::Detailed)
            object.insert("parameters", detailedParameters(effect.parameters));
        else if (!effect.hasEnabled || effect.enabled)
            insertDualDelayParameters(&object, effect.parameters);
        effects.append(object);
    }
    root.insert("effects", effects);

    if (detail == AIContextDetail::Detailed) {
        QJsonObject source;
        source.insert("provenance", snapshot.source.provenance);
        source.insert("reliability", snapshot.source.reliability);
        root.insert("source", source);
    }
    return context;
}

QByteArray AIContextBuilder::serialize(const AIContext &context,
                                       bool indented)
{
    return QJsonDocument(context.root).toJson(
        indented ? QJsonDocument::Indented : QJsonDocument::Compact);
}

QString AIContextBuilder::toText(const AIContext &context)
{
    const QJsonObject root = context.root;
    const QJsonObject patch = root.value("patch").toObject();
    QStringList lines;
    lines << "GT Lab AI Context";
    lines << QString("Device: %1").arg(root.value("device").toString());
    lines << QString("Patch: %1 — %2")
                 .arg(patch.value("number").toString(),
                      patch.value("name").toString());
    const QJsonObject routing = root.value("routing").toObject();
    lines << QString("Routing: %1%2")
                 .arg(routing.value("mode").toString(),
                      routing.contains("channelSelect")
                        ? " / " + routing.value("channelSelect").toString()
                        : QString());

    const QJsonObject chain = root.value("signalChain").toObject();
    lines << "Signal Chain:";
    const QStringList prefix = moduleTexts(
        chain.value("commonPrefix").toArray());
    if (!prefix.isEmpty())
        lines << "  " + prefix.join(" -> ");
    lines << "  " + moduleText(chain.value("split").toObject());
    lines << "    A: "
        + moduleTexts(chain.value("pathA").toArray()).join(" -> ");
    lines << "    B: "
        + moduleTexts(chain.value("pathB").toArray()).join(" -> ");
    lines << "  " + moduleText(chain.value("merge").toObject());
    const QStringList suffix = moduleTexts(
        chain.value("commonSuffix").toArray());
    if (!suffix.isEmpty())
        lines << "  " + suffix.join(" -> ");

    lines << "Active Effects:";
    for (const QJsonValue &value : root.value("effects").toArray()) {
        const QJsonObject effect = value.toObject();
        QString summary = "  " + readableId(effect.value("id").toString());
        if (effect.contains("enabled"))
            summary += effect.value("enabled").toBool() ? ": ON" : ": OFF";
        if (effect.contains("type"))
            summary += " — " + effect.value("type").toString();
        lines << summary;
        if (effect.value("parameters").isArray()) {
            for (const QJsonValue &parameterValue
                 : effect.value("parameters").toArray()) {
                const QJsonObject parameter = parameterValue.toObject();
                lines << QString("    %1: %2")
                    .arg(readableId(parameter.value("id").toString()),
                         parameter.value("value").toString());
            }
        } else {
            QJsonObject compactEffect = effect;
            compactEffect.remove("id");
            compactEffect.remove("enabled");
            compactEffect.remove("type");
            appendCompactObjectText(&lines, compactEffect, "    ");
        }
    }
    return lines.join('\n') + '\n';
}

QString AIContextBuilder::normalizeLabel(const QString &label)
{
    QString result = label.simplified();
    result.remove(QRegularExpression(QStringLiteral("^\\([^)]*\\)\\s*")));
    static const QMap<QString, QString> confirmed = {
        {QStringLiteral("DualL/R"), QStringLiteral("Dual L/R")},
        {QStringLiteral("Stereo1"), QStringLiteral("Stereo 1")},
        {QStringLiteral("Stereo2"), QStringLiteral("Stereo 2")},
        {QStringLiteral("Mod"), QStringLiteral("Modulate")}
    };
    return confirmed.value(result, result);
}

} // namespace Ai
