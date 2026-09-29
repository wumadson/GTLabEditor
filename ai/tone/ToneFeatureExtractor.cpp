#include "ai/tone/ToneFeatureExtractor.h"

#include "ai/tone/ToneRules.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QtMath>

namespace Ai {
namespace Tone {
namespace {

QJsonObject effect(const QJsonArray &effects, const QString &id)
{
    for (const QJsonValue &value : effects) {
        const QJsonObject object = value.toObject();
        if (object.value(QStringLiteral("id")).toString() == id)
            return object;
    }
    return {};
}

bool active(const QJsonObject &effect)
{
    return !effect.isEmpty()
        && (!effect.contains(QStringLiteral("enabled"))
            || effect.value(QStringLiteral("enabled")).toBool());
}

QString parameterText(const QJsonObject &object, const QString &id)
{
    const QJsonValue value = object.value(id);
    if (value.isString())
        return value.toString();
    if (value.isDouble())
        return QString::number(value.toDouble());
    return QString();
}

bool number(const QJsonObject &object, const QString &id, double *result)
{
    QString text = parameterText(object, id).trimmed();
    text.replace(',', '.');
    static const QRegularExpression pattern(
        QStringLiteral("[-+]?[0-9]+(?:\\.[0-9]+)?"));
    const QRegularExpressionMatch match = pattern.match(text);
    if (!match.hasMatch())
        return false;
    bool ok = false;
    const double value = match.captured().toDouble(&ok);
    if (ok && result)
        *result = value;
    return ok;
}

void appendBlock(QStringList *blocks, const QString &block)
{
    if (!blocks->contains(block))
        blocks->append(block);
}

void addFeature(QList<ToneFeature> *features, const QString &id,
                double strength, const QString &block,
                const QString &parameter, const QString &value,
                ToneSourceKind source = ToneSourceKind::ToneRule,
                Knowledge::Confidence confidence = Knowledge::Confidence::Derived)
{
    ToneFeature *feature = nullptr;
    for (ToneFeature &candidate : *features) {
        if (candidate.id == id) {
            feature = &candidate;
            break;
        }
    }
    if (!feature) {
        ToneFeature created;
        created.id = id;
        created.confidence = confidence;
        features->append(created);
        feature = &features->last();
    }
    feature->strength = qMax(feature->strength, qBound(0.0, strength, 1.0));
    appendBlock(&feature->relatedBlocks, block);
    feature->evidence.append({block, parameter, value,
                              ToneSourceKind::PatchContext});
    if (!feature->sources.contains(ToneSourceKind::PatchContext))
        feature->sources.append(ToneSourceKind::PatchContext);
    if (!feature->sources.contains(source))
        feature->sources.append(source);
}

void analyzePreamp(QList<ToneFeature> *features,
                   const QJsonObject &preamp, const QString &id)
{
    if (!active(preamp))
        return;
    double value = 0.0;
    if (number(preamp, QStringLiteral("gain"), &value)) {
        if (value <= 30.0) {
            addFeature(features, "clean", 1.0 - value / 45.0,
                       id, "gain", parameterText(preamp, "gain"));
            addFeature(features, "dynamic", 0.55, id, "gain",
                       parameterText(preamp, "gain"));
        } else if (value <= 60.0) {
            addFeature(features, "crunch", ToneRules::normalizedStrength(
                           value, 30.0, 60.0), id, "gain",
                       parameterText(preamp, "gain"));
        } else {
            addFeature(features, "driven", ToneRules::normalizedStrength(
                           value, 55.0, 90.0), id, "gain",
                       parameterText(preamp, "gain"));
            addFeature(features, "aggressive", ToneRules::normalizedStrength(
                           value, 65.0, 100.0), id, "gain",
                       parameterText(preamp, "gain"));
        }
    }
    if (number(preamp, QStringLiteral("bass"), &value)) {
        if (value >= 58.0)
            addFeature(features, "warm", ToneRules::normalizedStrength(
                           value, 50.0, 80.0), id, "bass",
                       parameterText(preamp, "bass"));
        if (value >= 72.0)
            addFeature(features, "muddy_risk", ToneRules::normalizedStrength(
                           value, 68.0, 100.0), id, "bass",
                       parameterText(preamp, "bass"));
    }
    if (number(preamp, QStringLiteral("middle"), &value) && value >= 55.0) {
        addFeature(features, "focused", ToneRules::normalizedStrength(
                       value, 45.0, 80.0), id, "middle",
                   parameterText(preamp, "middle"));
        addFeature(features, "defined", 0.55, id, "middle",
                   parameterText(preamp, "middle"));
    }
    const QString gainSwitch = parameterText(preamp, "gainSwitch");
    if (gainSwitch.contains("High", Qt::CaseInsensitive))
        addFeature(features, "driven", 0.55, id, "gainSwitch", gainSwitch);
    double treble = -1.0;
    double presence = -1.0;
    number(preamp, QStringLiteral("treble"), &treble);
    number(preamp, QStringLiteral("presence"), &presence);
    const double upper = qMax(treble, presence);
    if (upper >= 58.0)
        addFeature(features, "bright", ToneRules::normalizedStrength(
                       upper, 50.0, 85.0), id,
                   upper == treble ? "treble" : "presence",
                   QString::number(upper));
    else if (treble >= 0.0 && presence >= 0.0
             && qMax(treble, presence) <= 35.0) {
        addFeature(features, "dark", 0.65, id, "treble/presence",
                   QString("%1/%2").arg(treble).arg(presence));
        addFeature(features, "smooth", 0.5, id, "treble/presence",
                   QString("%1/%2").arg(treble).arg(presence));
    }
}

} // namespace

QList<ToneFeature> ToneFeatureExtractor::extract(const AIContext &context) const
{
    QList<ToneFeature> features;
    const QJsonObject root = context.root;
    const QJsonArray effects = root.value(QStringLiteral("effects")).toArray();

    const QJsonObject preampA = effect(effects, QStringLiteral("preampA"));
    const QJsonObject preampB = effect(effects, QStringLiteral("preampB"));
    analyzePreamp(&features, preampA, QStringLiteral("preampA"));
    analyzePreamp(&features, preampB, QStringLiteral("preampB"));

    const QJsonObject routing = root.value(QStringLiteral("routing")).toObject();
    const QString mode = routing.value(QStringLiteral("mode")).toString();
    if (mode.contains(QStringLiteral("Dual L/R"), Qt::CaseInsensitive))
        addFeature(&features, "wide", 0.85, "preampGlobal", "channelMode",
                   mode, ToneSourceKind::OfficialDocumentation);

    const QJsonObject comp = effect(effects, QStringLiteral("comp"));
    double value = 0.0;
    if (active(comp)) {
        const QString type = comp.value(QStringLiteral("type")).toString();
        if (!type.contains("Limit", Qt::CaseInsensitive)
            && number(comp, QStringLiteral("sustain"), &value)
            && value >= 50.0)
            addFeature(&features, "compressed", ToneRules::normalizedStrength(
                           value, 40.0, 90.0), "comp", "sustain",
                       parameterText(comp, "sustain"));
        if (number(comp, QStringLiteral("threshold"), &value) && value >= 50.0)
            addFeature(&features, "compressed", 0.65, "comp", "threshold",
                       parameterText(comp, "threshold"));
    }

    const QJsonObject od = effect(effects, QStringLiteral("odDs"));
    if (active(od)) {
        if (number(od, QStringLiteral("drive"), &value)) {
            addFeature(&features, value >= 60.0 ? "aggressive" : "driven",
                       ToneRules::normalizedStrength(value, 20.0, 90.0),
                       "odDs", "drive", parameterText(od, "drive"));
        }
        if (number(od, QStringLiteral("bottom"), &value) && value >= 70.0)
            addFeature(&features, "muddy_risk", 0.65, "odDs", "bottom",
                       parameterText(od, "bottom"));
        if (number(od, QStringLiteral("tone"), &value)) {
            if (value >= 62.0)
                addFeature(&features, "bright", 0.55, "odDs", "tone",
                           parameterText(od, "tone"));
            else if (value <= 35.0)
                addFeature(&features, "dark", 0.5, "odDs", "tone",
                           parameterText(od, "tone"));
        }
    }

    const QJsonObject eq = effect(effects, QStringLiteral("eq"));
    if (active(eq)) {
        const QString lowCut = parameterText(eq, QStringLiteral("lowCut"));
        if (!lowCut.isEmpty() && lowCut.compare("Flat", Qt::CaseInsensitive) != 0)
            addFeature(&features, "defined", 0.65, "eq", "lowCut", lowCut,
                       ToneSourceKind::OfficialDocumentation);
        if (number(eq, QStringLiteral("lowGain"), &value) && value >= 8.0)
            addFeature(&features, "muddy_risk", 0.6, "eq", "lowGain",
                       parameterText(eq, "lowGain"));
        if (number(eq, QStringLiteral("highGain"), &value)) {
            if (value >= 4.0)
                addFeature(&features, "bright", 0.6, "eq", "highGain",
                           parameterText(eq, "highGain"));
            else if (value <= -4.0)
                addFeature(&features, "dark", 0.6, "eq", "highGain",
                           parameterText(eq, "highGain"));
        }
        const QString highCut = parameterText(eq, QStringLiteral("highCut"));
        if (!highCut.isEmpty() && highCut.compare("Flat", Qt::CaseInsensitive) != 0)
            addFeature(&features, "smooth", 0.5, "eq", "highCut", highCut,
                       ToneSourceKind::OfficialDocumentation);
    }

    const QJsonObject delay = effect(effects, QStringLiteral("delay"));
    const bool delayActive = active(delay);
    if (delayActive) {
        const QString type = delay.value(QStringLiteral("type")).toString();
        if (type.contains("Dual", Qt::CaseInsensitive)) {
            addFeature(&features, "ambient", 0.65, "delay", "type", type,
                       ToneSourceKind::OfficialDocumentation);
            addFeature(&features, "wide", 0.65, "delay", "type", type,
                       ToneSourceKind::OfficialDocumentation);
        }
        const QJsonObject delay1 = delay.value(QStringLiteral("delay1")).toObject();
        const QJsonObject delay2 = delay.value(QStringLiteral("delay2")).toObject();
        const QString time1 = parameterText(delay1, QStringLiteral("time"));
        const QString time2 = parameterText(delay2, QStringLiteral("time"));
        if (time1.contains('/') || time2.contains('/'))
            addFeature(&features, "rhythmic", 0.75, "delay", "time",
                       (time1 + " / " + time2).trimmed(),
                       ToneSourceKind::OfficialDocumentation);
        double feedback1 = 0.0;
        double feedback2 = 0.0;
        const bool hasFeedback1 = number(delay1, "feedback", &feedback1);
        const bool hasFeedback2 = number(delay2, "feedback", &feedback2);
        if ((hasFeedback1 || hasFeedback2)
            && qMax(feedback1, feedback2) >= 45.0) {
            addFeature(&features, "deep", ToneRules::normalizedStrength(
                           qMax(feedback1, feedback2), 35.0, 80.0),
                       "delay", "feedback",
                       QString::number(qMax(feedback1, feedback2)),
                       ToneSourceKind::OfficialDocumentation);
        }
        const QString highCut1 = parameterText(delay1, "highCut");
        const QString highCut2 = parameterText(delay2, "highCut");
        if ((!highCut1.isEmpty() && highCut1.compare("Flat", Qt::CaseInsensitive) != 0)
            || (!highCut2.isEmpty() && highCut2.compare("Flat", Qt::CaseInsensitive) != 0)) {
            addFeature(&features, "smooth", 0.5, "delay", "highCut",
                       (highCut1 + " / " + highCut2).trimmed(),
                       ToneSourceKind::OfficialDocumentation);
        }
    }

    const QJsonObject reverb = effect(effects, QStringLiteral("reverb"));
    const bool reverbActive = active(reverb);
    if (reverbActive) {
        const QString type = reverb.value(QStringLiteral("type")).toString();
        if (type.contains("Hall", Qt::CaseInsensitive)
            || type.contains("Ambience", Qt::CaseInsensitive)
            || type.contains("Modulate", Qt::CaseInsensitive))
            addFeature(&features, "ambient", 0.7, "reverb", "type", type,
                       ToneSourceKind::OfficialDocumentation);
        if (number(reverb, QStringLiteral("reverbTime"), &value)) {
            if (value >= 4.0)
                addFeature(&features, "ambient", ToneRules::normalizedStrength(
                               value, 3.0, 9.0), "reverb", "reverbTime",
                           parameterText(reverb, "reverbTime"),
                           ToneSourceKind::OfficialDocumentation);
            if (value >= 6.0)
                addFeature(&features, "deep", ToneRules::normalizedStrength(
                               value, 5.0, 10.0), "reverb", "reverbTime",
                           parameterText(reverb, "reverbTime"),
                           ToneSourceKind::OfficialDocumentation);
        }
        if (number(reverb, QStringLiteral("preDelay"), &value) && value >= 40.0)
            addFeature(&features, "defined", 0.55, "reverb", "preDelay",
                       parameterText(reverb, "preDelay"),
                       ToneSourceKind::OfficialDocumentation);
        if (number(reverb, QStringLiteral("effectLevel"), &value) && value >= 30.0)
            addFeature(&features, "ambient", ToneRules::normalizedStrength(
                           value, 20.0, 65.0), "reverb", "effectLevel",
                       parameterText(reverb, "effectLevel"),
                       ToneSourceKind::OfficialDocumentation);
        if (number(reverb, QStringLiteral("density"), &value) && value >= 7.0)
            addFeature(&features, "smooth", 0.6, "reverb", "density",
                       parameterText(reverb, "density"),
                       ToneSourceKind::OfficialDocumentation);
        const QString lowCut = parameterText(reverb, QStringLiteral("lowCut"));
        if (!lowCut.isEmpty() && lowCut.compare("Flat", Qt::CaseInsensitive) != 0)
            addFeature(&features, "defined", 0.55, "reverb", "lowCut", lowCut,
                       ToneSourceKind::OfficialDocumentation);
        const QString highCut = parameterText(reverb, QStringLiteral("highCut"));
        if (!highCut.isEmpty() && highCut.compare("Flat", Qt::CaseInsensitive) != 0)
            addFeature(&features, "smooth", 0.55, "reverb", "highCut", highCut,
                       ToneSourceKind::OfficialDocumentation);
    }

    const QJsonObject chorus = effect(effects, QStringLiteral("chorus"));
    const bool chorusActive = active(chorus);
    if (chorusActive) {
        const QString type = chorus.value(QStringLiteral("type")).toString();
        if (type.contains("Stereo", Qt::CaseInsensitive))
            addFeature(&features, "wide", 0.65, "chorus", "type", type,
                       ToneSourceKind::OfficialDocumentation);
        if (number(chorus, QStringLiteral("depth"), &value) && value >= 45.0)
            addFeature(&features, "smooth", 0.55, "chorus", "depth",
                       parameterText(chorus, "depth"));
    }

    if (!delayActive && !reverbActive && !chorusActive)
        addFeature(&features, "dry", 0.8, "effects", "timeEffects", "off");

    return features;
}

} // namespace Tone
} // namespace Ai
