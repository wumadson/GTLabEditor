#include "ai/tone/ToneRules.h"

#include <QMap>

namespace Ai {
namespace Tone {
namespace {

bool portuguese(const QString &language)
{
    return language.startsWith(QStringLiteral("pt"), Qt::CaseInsensitive);
}

QString key(const QString &blockId, const QString &parameterId)
{
    return blockId + QLatin1Char(':') + parameterId;
}

QMap<QString, ToneParameterRule> rules()
{
    QMap<QString, ToneParameterRule> values;
    const auto add = [&values](const char *block, const char *parameter,
                               double minimum, double maximum,
                               double stepMinimum, double stepMaximum,
                               ToneSourceKind source = ToneSourceKind::ToneRule) {
        ToneParameterRule rule;
        rule.blockId = QString::fromLatin1(block);
        rule.parameterId = QString::fromLatin1(parameter);
        rule.minimum = minimum;
        rule.maximum = maximum;
        rule.smallStepMinimum = stepMinimum;
        rule.smallStepMaximum = stepMaximum;
        rule.sourceKind = source;
        values.insert(key(rule.blockId, rule.parameterId), rule);
    };

    for (const char *block : {"preampA", "preampB"}) {
        for (const char *parameter : {"gain", "bass", "middle", "treble",
                                      "presence", "level", "directLevel"})
            add(block, parameter, 0, 100, 5, 10);
    }
    for (const char *parameter : {"drive", "bottom", "tone", "effectLevel",
                                  "directLevel"})
        add("odDs", parameter, 0, 100, 5, 10);
    for (const char *parameter : {"sustain", "attack", "threshold", "release",
                                  "tone", "level"})
        add("comp", parameter, 0, 100, 5, 10);
    for (const char *parameter : {"lowGain", "lowMiddleGain", "highMiddleGain",
                                  "highGain", "outputLevel"})
        add("eq", parameter, -20, 20, 2, 4);
    for (const char *parameter : {"feedback", "directLevel",
                                  "dualDelay1Feedback", "dualDelay1Level",
                                  "dualDelay2Feedback", "dualDelay2Level"})
        add("delay", parameter, 0, 100, 5, 10,
            ToneSourceKind::OfficialDocumentation);
    add("delay", "effectLevel", 0, 120, 5, 10,
        ToneSourceKind::OfficialDocumentation);
    add("delay", "delayTime", 1, 3400, 10, 50,
        ToneSourceKind::OfficialDocumentation);
    add("reverb", "reverbTime", 0.1, 10.0, 0.5, 1.0,
        ToneSourceKind::OfficialDocumentation);
    add("reverb", "preDelay", 0, 500, 10, 25,
        ToneSourceKind::OfficialDocumentation);
    add("reverb", "density", 0, 10, 1, 2,
        ToneSourceKind::OfficialDocumentation);
    for (const char *parameter : {"effectLevel", "directLevel"})
        add("reverb", parameter, 0, 100, 5, 10,
            ToneSourceKind::OfficialDocumentation);
    for (const char *parameter : {"rate", "depth", "effectLevel"})
        add("chorus", parameter, 0, 100, 5, 10,
            ToneSourceKind::OfficialDocumentation);
    add("chorus", "preDelay", 0, 40, 1, 5,
        ToneSourceKind::OfficialDocumentation);
    add("preampGlobal", "channelDelay", 0, 50, 1, 3);
    return values;
}

} // namespace

bool ToneRules::parameterRule(const QString &blockId,
                              const QString &parameterId,
                              ToneParameterRule *rule)
{
    const QMap<QString, ToneParameterRule> values = rules();
    const auto it = values.constFind(key(blockId, parameterId));
    if (it == values.constEnd())
        return false;
    if (rule)
        *rule = it.value();
    return true;
}

double ToneRules::normalizedStrength(double value, double low, double high)
{
    if (high <= low)
        return 0.0;
    const double normalized = (value - low) / (high - low);
    return qBound(0.0, normalized, 1.0);
}

QString ToneRules::featureLabel(const QString &id, const QString &language)
{
    static const QMap<QString, QPair<QString, QString>> labels = {
        {"clean", {"limpa", "clean"}},
        {"crunch", {"com potencial de crunch", "with crunch potential"}},
        {"driven", {"saturada", "driven"}},
        {"bright", {"brilhante", "bright"}},
        {"dark", {"escura", "dark"}},
        {"warm", {"encorpada", "warm"}},
        {"focused", {"focada", "focused"}},
        {"wide", {"ampla", "wide"}},
        {"deep", {"profunda", "deep"}},
        {"ambient", {"ambiente", "ambient"}},
        {"dry", {"seca", "dry"}},
        {"dynamic", {"dinâmica", "dynamic"}},
        {"compressed", {"comprimida", "compressed"}},
        {"aggressive", {"agressiva", "aggressive"}},
        {"smooth", {"suave", "smooth"}},
        {"defined", {"definida", "defined"}},
        {"muddy_risk", {"com possível acúmulo de graves", "with possible low-end buildup"}},
        {"rhythmic", {"com movimento rítmico", "with rhythmic movement"}}
    };
    const auto value = labels.value(id, {id, id});
    return portuguese(language) ? value.first : value.second;
}

QString ToneRules::blockLabel(const QString &id, const QString &language)
{
    Q_UNUSED(language)
    static const QMap<QString, QString> labels = {
        {"preampA", "PREAMP A"}, {"preampB", "PREAMP B"},
        {"preampGlobal", "PREAMP"}, {"odDs", "OD/DS"},
        {"comp", "COMP"}, {"eq", "EQ"}, {"delay", "DELAY"},
        {"reverb", "REVERB"}, {"chorus", "CHORUS"}
    };
    return labels.value(id, id);
}

} // namespace Tone
} // namespace Ai
