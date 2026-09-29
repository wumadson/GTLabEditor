#include "ai/tone/ToneAnalyzer.h"

#include "ai/tone/ToneFeatureExtractor.h"
#include "ai/tone/ToneRules.h"

#include <algorithm>

namespace Ai {
namespace Tone {
namespace {

bool portuguese(const QString &language)
{
    return language.startsWith(QStringLiteral("pt"), Qt::CaseInsensitive);
}

const ToneFeature *feature(const QList<ToneFeature> &features,
                           const QString &id)
{
    for (const ToneFeature &candidate : features) {
        if (candidate.id == id)
            return &candidate;
    }
    return nullptr;
}

QString evidenceSentence(const ToneFeature &feature, const QString &language)
{
    if (feature.evidence.isEmpty())
        return QString();
    const ToneEvidence &item = feature.evidence.first();
    const QString block = ToneRules::blockLabel(item.blockId, language);
    if (portuguese(language)) {
        if (item.parameterId == QLatin1String("channelMode"))
            return QString("O routing %1 pode contribuir para largura e contraste entre os canais.").arg(item.value);
        if (item.parameterId == QLatin1String("reverbTime"))
            return QString("O %1 em %2 contribui para uma cauda longa e maior profundidade.")
                .arg(block, item.value);
        if (item.parameterId == QLatin1String("time"))
            return QString("Os tempos do %1 (%2) acrescentam movimento rítmico.")
                .arg(block, item.value);
        if (item.parameterId == QLatin1String("gain"))
            return QString("O Gain do %1 em %2 indica o nível de saturação configurado.")
                .arg(block, item.value);
        if (item.parameterId == QLatin1String("middle"))
            return QString("Os médios do %1 em %2 ajudam a preservar corpo e foco.")
                .arg(block, item.value);
    } else {
        if (item.parameterId == QLatin1String("channelMode"))
            return QString("%1 routing can contribute width and contrast between channels.").arg(item.value);
        if (item.parameterId == QLatin1String("reverbTime"))
            return QString("%1 at %2 contributes a longer tail and greater depth.")
                .arg(block, item.value);
        if (item.parameterId == QLatin1String("time"))
            return QString("The %1 times (%2) add rhythmic movement.")
                .arg(block, item.value);
        if (item.parameterId == QLatin1String("gain"))
            return QString("%1 Gain at %2 indicates the configured saturation level.")
                .arg(block, item.value);
        if (item.parameterId == QLatin1String("middle"))
            return QString("%1 mids at %2 help retain body and focus.")
                .arg(block, item.value);
    }
    return QString();
}

} // namespace

ToneAnalysis ToneAnalyzer::analyze(const AIContext &context,
                                   const QString &language) const
{
    ToneAnalysis analysis;
    analysis.features = ToneFeatureExtractor().extract(context);
    for (const ToneFeature &item : analysis.features)
        analysis.evidence.append(item.evidence);
    analysis.caveats.append(portuguese(language)
        ? QStringLiteral("A análise considera os parâmetros configurados; guitarra, captadores, execução, monitoração e cadeia externa também influenciam o resultado.")
        : QStringLiteral("This analysis uses the configured parameters; guitar, pickups, playing, monitoring, and the external chain also influence the result."));

    if (analysis.features.isEmpty()) {
        analysis.summary = portuguese(language)
            ? QStringLiteral("Não há dados suficientes no patch atual para derivar uma tendência tonal com segurança.")
            : QStringLiteral("The current patch does not contain enough data to derive a tone tendency safely.");
        return analysis;
    }

    QList<ToneFeature> ranked = analysis.features;
    std::sort(ranked.begin(), ranked.end(), [](const ToneFeature &a,
                                                const ToneFeature &b) {
        return a.strength > b.strength;
    });
    QStringList labels;
    for (const ToneFeature &item : ranked) {
        if (labels.size() >= 4 || item.id == QLatin1String("muddy_risk"))
            continue;
        labels.append(ToneRules::featureLabel(item.id, language));
    }
    analysis.summary = portuguese(language)
        ? QString("Com base nos parâmetros atuais, este preset tende a uma sonoridade %1.")
              .arg(labels.join(", "))
        : QString("Based on its current parameters, this patch tends toward a %1 tone.")
              .arg(labels.join(", "));

    QStringList details;
    for (const QString &id : {QStringLiteral("wide"), QStringLiteral("rhythmic"),
                              QStringLiteral("deep"), QStringLiteral("driven"),
                              QStringLiteral("crunch"), QStringLiteral("defined")}) {
        const ToneFeature *item = feature(ranked, id);
        if (!item)
            continue;
        const QString sentence = evidenceSentence(*item, language);
        if (!sentence.isEmpty() && !details.contains(sentence))
            details.append(sentence);
        if (details.size() == 3)
            break;
    }
    if (!details.isEmpty())
        analysis.summary += QLatin1Char(' ') + details.join(QLatin1Char(' '));
    return analysis;
}

} // namespace Tone
} // namespace Ai
