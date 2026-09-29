#include "ai/tone/ToneAdjustmentAdvisor.h"

#include "ai/tone/ToneRules.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QRegularExpression>
#include <QtMath>

namespace Ai {
namespace Tone {
namespace {

bool portuguese(const QString &language)
{
    return language.startsWith(QStringLiteral("pt"), Qt::CaseInsensitive);
}

QJsonObject effect(const AIContext &context, const QString &id)
{
    for (const QJsonValue &value
         : context.root.value(QStringLiteral("effects")).toArray()) {
        const QJsonObject object = value.toObject();
        if (object.value(QStringLiteral("id")).toString() == id)
            return object;
    }
    return {};
}

bool active(const QJsonObject &object)
{
    return !object.isEmpty()
        && (!object.contains(QStringLiteral("enabled"))
            || object.value(QStringLiteral("enabled")).toBool());
}

bool numericText(const QString &text, double *number)
{
    QString normalized = text;
    normalized.replace(',', '.');
    static const QRegularExpression pattern(
        QStringLiteral("[-+]?[0-9]+(?:\\.[0-9]+)?"));
    const QRegularExpressionMatch match = pattern.match(normalized);
    if (!match.hasMatch())
        return false;
    bool ok = false;
    const double result = match.captured().toDouble(&ok);
    if (ok && number)
        *number = result;
    return ok;
}

QString value(const QJsonObject &object, const QString &id)
{
    const QJsonValue item = object.value(id);
    if (item.isString())
        return item.toString();
    if (item.isDouble())
        return QString::number(item.toDouble());
    return QString();
}

QString capabilityLabel(const QString &parameterId)
{
    static const QMap<QString, QString> labels = {
        {"gain", "Gain"}, {"bass", "Bass"}, {"middle", "Middle"},
        {"treble", "Treble"}, {"presence", "Presence"},
        {"drive", "Drive"}, {"tone", "Tone"}, {"bottom", "Bottom"},
        {"sustain", "Sustain"}, {"threshold", "Threshold"},
        {"feedback", "Feedback"}, {"effectLevel", "Effect Level"},
        {"reverbTime", "Reverb Time"}, {"preDelay", "Pre Delay"},
        {"density", "Density"}
    };
    return labels.value(parameterId, parameterId);
}

QString formatted(double value, double step)
{
    return step < 1.0 ? QString::number(value, 'f', 1)
                      : QString::number(qRound(value));
}

bool numericSuggestion(const Knowledge::GT10CapabilityService &service,
                       const QJsonObject &object, const QString &blockId,
                       const QString &parameterId, bool increase,
                       const QString &reason, ToneAdjustmentSuggestion *suggestion)
{
    const QString currentText = value(object, parameterId);
    double current = 0.0;
    if (currentText.isEmpty() || !numericText(currentText, &current))
        return false;
    ToneParameterRule rule;
    if (!ToneRules::parameterRule(blockId, parameterId, &rule))
        return false;

    const QString type = object.value(QStringLiteral("type")).toString();
    const Knowledge::ParameterCapability capability =
        service.parameterCapability(blockId, type,
                                    capabilityLabel(parameterId));
    double minimum = rule.minimum;
    double maximum = rule.maximum;
    double catalogMinimum = 0.0;
    double catalogMaximum = 0.0;
    if (capability.available
        && numericText(capability.minimum, &catalogMinimum)
        && numericText(capability.maximum, &catalogMaximum)
        && catalogMaximum > catalogMinimum) {
        minimum = catalogMinimum;
        maximum = catalogMaximum;
    }

    if ((increase && current >= maximum) || (!increase && current <= minimum))
        return false;
    const double sign = increase ? 1.0 : -1.0;
    double first = qBound(minimum,
                          current + sign * rule.smallStepMinimum, maximum);
    double second = qBound(minimum,
                           current + sign * rule.smallStepMaximum, maximum);
    if (!increase)
        qSwap(first, second);

    suggestion->blockId = blockId;
    suggestion->parameterId = parameterId;
    suggestion->currentValue = currentText;
    suggestion->suggestedDirection = increase
        ? QStringLiteral("increase") : QStringLiteral("decrease");
    suggestion->suggestedTargetRange = QStringLiteral("%1–%2")
        .arg(formatted(first, rule.smallStepMinimum),
             formatted(second, rule.smallStepMinimum));
    suggestion->reason = reason;
    suggestion->confidence = capability.available
        ? Knowledge::Confidence::Authoritative
        : Knowledge::Confidence::Guidance;
    suggestion->sources = {ToneSourceKind::PatchContext, rule.sourceKind};
    return true;
}

void appendNumeric(QList<ToneAdjustmentSuggestion> *suggestions,
                   const Knowledge::GT10CapabilityService &service,
                   const QJsonObject &object, const QString &blockId,
                   const QString &parameterId, bool increase,
                   const QString &reason)
{
    ToneAdjustmentSuggestion suggestion;
    if (numericSuggestion(service, object, blockId, parameterId, increase,
                          reason, &suggestion))
        suggestions->append(suggestion);
}

void appendOptional(QList<ToneAdjustmentSuggestion> *suggestions,
                    const QString &blockId, const QString &parameterId,
                    const QString &current, const QString &target,
                    const QString &reason)
{
    ToneAdjustmentSuggestion suggestion;
    suggestion.blockId = blockId;
    suggestion.parameterId = parameterId;
    suggestion.currentValue = current;
    suggestion.suggestedDirection = QStringLiteral("optional");
    suggestion.suggestedTargetRange = target;
    suggestion.reason = reason;
    suggestion.confidence = Knowledge::Confidence::Guidance;
    suggestion.sources = {ToneSourceKind::PatchContext,
                          ToneSourceKind::ToneRule,
                          ToneSourceKind::Guidance};
    suggestion.optionalActivation = true;
    suggestions->append(suggestion);
}

QString suggestionText(const ToneAdjustmentSuggestion &suggestion,
                       const QString &language, bool first)
{
    const QString block = ToneRules::blockLabel(suggestion.blockId, language);
    const QString prefix = portuguese(language)
        ? (first ? QStringLiteral("Eu começaria")
                 : QStringLiteral("Como segunda opção"))
        : (first ? QStringLiteral("I would start")
                 : QStringLiteral("As a second option"));
    if (suggestion.optionalActivation) {
        return portuguese(language)
            ? QString("%1 considerando %2 de forma opcional (%3). %4")
                  .arg(prefix, block, suggestion.suggestedTargetRange,
                       suggestion.reason)
            : QString("%1 by considering %2 as an optional step (%3). %4")
                  .arg(prefix, block, suggestion.suggestedTargetRange,
                       suggestion.reason);
    }
    const QString action = suggestion.suggestedDirection == QLatin1String("increase")
        ? (portuguese(language) ? QStringLiteral("subindo")
                                : QStringLiteral("raising"))
        : (portuguese(language) ? QStringLiteral("reduzindo")
                                : QStringLiteral("lowering"));
    return portuguese(language)
        ? QString("%1 %2 %3 do valor atual %4 para algo em torno de %5. %6")
              .arg(prefix, action, capabilityLabel(suggestion.parameterId),
                   suggestion.currentValue, suggestion.suggestedTargetRange,
                   suggestion.reason)
        : QString("%1 by %2 %3 from its current %4 to around %5. %6")
              .arg(prefix, action, capabilityLabel(suggestion.parameterId),
                   suggestion.currentValue, suggestion.suggestedTargetRange,
                   suggestion.reason);
}

} // namespace

ToneAdvice ToneAdjustmentAdvisor::advise(
    const AIContext &context, const ToneAdjustmentIntent &intent,
    const QString &language, const QList<ToneConstraint> &constraints) const
{
    ToneAdvice advice;
    advice.intent = intent;
    if (!intent.isValid() || context.root.isEmpty()) {
        advice.response = portuguese(language)
            ? QStringLiteral("Não há contexto suficiente do patch atual para orientar esse ajuste com segurança.")
            : QStringLiteral("There is not enough current-patch context to guide that adjustment safely.");
        return advice;
    }

    const QJsonObject preampA = effect(context, "preampA");
    const QJsonObject preampB = effect(context, "preampB");
    const QJsonObject od = effect(context, "odDs");
    const QJsonObject comp = effect(context, "comp");
    const QJsonObject delay = effect(context, "delay");
    const QJsonObject reverb = effect(context, "reverb");
    const QJsonObject eq = effect(context, "eq");
    const QJsonObject routing = context.root.value("routing").toObject();
    const bool avoidAmp = constraints.contains(ToneConstraint::AvoidAmp);
    const bool avoidOd = constraints.contains(ToneConstraint::AvoidOdDs);
    const bool preserveAmbience = constraints.contains(
        ToneConstraint::PreserveAmbience);
    const bool avoidReverb = constraints.contains(ToneConstraint::AvoidReverb)
        || preserveAmbience;
    const bool avoidDelay = constraints.contains(ToneConstraint::AvoidDelay)
        || preserveAmbience;
    const bool preserveLowEnd = constraints.contains(
        ToneConstraint::PreserveLowEnd);
    const QString ptReason = portuguese(language) ? QStringLiteral("Uma mudança pequena preserva a identidade atual do preset.")
                                                  : QStringLiteral("A small change preserves the current patch identity.");

    switch (intent.direction) {
    case ToneDirection::MoreCrunch:
        if (!avoidAmp && active(preampA))
            appendNumeric(&advice.suggestions, capabilityService, preampA,
                          "preampA", "gain", true, ptReason);
        else if (!avoidAmp && active(preampB))
            appendNumeric(&advice.suggestions, capabilityService, preampB,
                          "preampB", "gain", true, ptReason);
        if (!avoidOd && advice.suggestions.size() < 2
            && !od.isEmpty() && !active(od))
            appendOptional(&advice.suggestions, "odDs", "enabled", "OFF",
                           od.value("type").toString() + " / low Drive",
                           portuguese(language)
                               ? QStringLiteral("O OD/DS está OFF e não influencia o timbre atual; ativá-lo é apenas uma alternativa posterior.")
                               : QStringLiteral("OD/DS is OFF and does not affect the current tone; enabling it is only a later alternative."));
        break;
    case ToneDirection::MoreClean:
        if (!avoidOd && active(od))
            appendNumeric(&advice.suggestions, capabilityService, od,
                          "odDs", "drive", false, ptReason);
        if (!avoidAmp && advice.suggestions.isEmpty() && active(preampA))
            appendNumeric(&advice.suggestions, capabilityService, preampA,
                          "preampA", "gain", false, ptReason);
        break;
    case ToneDirection::MoreAmbient:
    case ToneDirection::MoreDeep:
        if (!avoidReverb && active(reverb)) {
            double reverbTime = 0.0;
            const bool alreadyLong = numericText(value(reverb, "reverbTime"),
                                                  &reverbTime)
                && reverbTime >= 8.0;
            if (!alreadyLong) {
                appendNumeric(&advice.suggestions, capabilityService, reverb,
                              "reverb", "reverbTime", true,
                              portuguese(language)
                                  ? QStringLiteral("Isso alonga a cauda sem aumentar todos os efeitos de ambiência ao mesmo tempo.")
                                  : QStringLiteral("This lengthens the tail without raising every ambience effect at once."));
            }
            if (advice.suggestions.isEmpty())
                appendNumeric(&advice.suggestions, capabilityService, reverb,
                              "reverb", "effectLevel", true,
                              alreadyLong
                                  ? (portuguese(language)
                                      ? QStringLiteral("O Reverb Time já está longo; ajustar o balanço da cauda é mais controlado do que estendê-la ainda mais.")
                                      : QStringLiteral("Reverb Time is already long; adjusting the tail balance is more controlled than extending it further."))
                                  : ptReason);
        } else if (!avoidReverb && !reverb.isEmpty()) {
            appendOptional(&advice.suggestions, "reverb", "enabled", "OFF",
                           reverb.value("type").toString(),
                           portuguese(language)
                               ? QStringLiteral("O Reverb está OFF, portanto não contribui atualmente.")
                               : QStringLiteral("Reverb is OFF, so it does not currently contribute."));
        }
        if (!avoidDelay && advice.suggestions.size() < 2 && active(delay))
            appendNumeric(&advice.suggestions, capabilityService, delay,
                          "delay", "feedback", true,
                          portuguese(language)
                              ? QStringLiteral("Use isso apenas se também quiser mais repetições e movimento.")
                              : QStringLiteral("Use this only if you also want more repeats and movement."));
        break;
    case ToneDirection::MoreDefined:
    case ToneDirection::LessMuddy:
        if (!avoidReverb && active(reverb))
            appendNumeric(&advice.suggestions, capabilityService, reverb,
                          "reverb", "effectLevel", false,
                          portuguese(language)
                              ? QStringLiteral("Reduzir primeiro a ambiência pode liberar definição sem afinar o corpo do amp.")
                              : QStringLiteral("Reducing ambience first can improve definition without thinning the amp body."));
        if (!avoidDelay && advice.suggestions.isEmpty() && active(delay))
            appendNumeric(&advice.suggestions, capabilityService, delay,
                          "delay", "feedback", false, ptReason);
        if (!avoidAmp && !preserveLowEnd
            && advice.suggestions.isEmpty() && active(preampA))
            appendNumeric(&advice.suggestions, capabilityService, preampA,
                          "preampA", "bass", false, ptReason);
        if (active(eq) && value(eq, "lowCut").compare("Flat", Qt::CaseInsensitive) != 0)
            advice.caveats.append(portuguese(language)
                ? QStringLiteral("O EQ já possui Low Cut ativo; evite empilhar outro corte sem ouvir o resultado.")
                : QStringLiteral("EQ already has an active Low Cut; avoid stacking another cut without listening."));
        break;
    case ToneDirection::MoreBright:
        if (!avoidAmp && active(preampA))
            appendNumeric(&advice.suggestions, capabilityService, preampA,
                          "preampA", "treble", true, ptReason);
        if (!avoidAmp && advice.suggestions.isEmpty() && active(preampB))
            appendNumeric(&advice.suggestions, capabilityService, preampB,
                          "preampB", "presence", true, ptReason);
        break;
    case ToneDirection::MoreDark:
        if (!avoidAmp && active(preampA))
            appendNumeric(&advice.suggestions, capabilityService, preampA,
                          "preampA", "presence", false, ptReason);
        if (!avoidAmp && advice.suggestions.isEmpty() && active(preampB))
            appendNumeric(&advice.suggestions, capabilityService, preampB,
                          "preampB", "treble", false, ptReason);
        break;
    case ToneDirection::MoreWide: {
        const QString mode = routing.value("mode").toString();
        if (!mode.contains("Dual L/R", Qt::CaseInsensitive))
            appendOptional(&advice.suggestions, "preampGlobal", "channelMode",
                           mode, "Dual L/R",
                           portuguese(language)
                               ? QStringLiteral("Dual L/R pode contribuir para largura quando os canais A/B têm conteúdo coerente.")
                               : QStringLiteral("Dual L/R can contribute width when channels A/B contain suitable material."));
        else
            appendNumeric(&advice.suggestions, capabilityService, routing,
                          "preampGlobal", "channelDelay", true, ptReason);
        break;
    }
    case ToneDirection::MoreFocused:
        appendNumeric(&advice.suggestions, capabilityService, routing,
                      "preampGlobal", "channelDelay", false, ptReason);
        if (!avoidReverb && advice.suggestions.isEmpty() && active(reverb))
            appendNumeric(&advice.suggestions, capabilityService, reverb,
                          "reverb", "effectLevel", false, ptReason);
        break;
    case ToneDirection::MoreAggressive:
        if (!avoidAmp && active(preampA))
            appendNumeric(&advice.suggestions, capabilityService, preampA,
                          "preampA", "gain", true, ptReason);
        break;
    case ToneDirection::MoreSmooth:
        if (!avoidAmp && active(preampA))
            appendNumeric(&advice.suggestions, capabilityService, preampA,
                          "preampA", "presence", false, ptReason);
        break;
    case ToneDirection::MoreDynamic:
        if (active(comp))
            appendNumeric(&advice.suggestions, capabilityService, comp,
                          "comp", "sustain", false, ptReason);
        if (!avoidAmp && advice.suggestions.isEmpty() && active(preampA))
            appendNumeric(&advice.suggestions, capabilityService, preampA,
                          "preampA", "gain", false, ptReason);
        break;
    case ToneDirection::MoreCompressed:
        if (active(comp))
            appendNumeric(&advice.suggestions, capabilityService, comp,
                          "comp", "sustain", true, ptReason);
        else if (!comp.isEmpty())
            appendOptional(&advice.suggestions, "comp", "enabled", "OFF",
                           comp.value("type").toString(),
                           portuguese(language)
                               ? QStringLiteral("O COMP está OFF e só passaria a influenciar após ativação explícita.")
                               : QStringLiteral("COMP is OFF and would only influence the tone after explicit activation."));
        break;
    case ToneDirection::None:
        break;
    }

    while (advice.suggestions.size() > 2)
        advice.suggestions.removeLast();
    if (advice.suggestions.isEmpty()) {
        advice.response = portuguese(language)
            ? QStringLiteral("O patch atual não oferece uma mudança pequena e comprovada para essa direção sem atingir um limite ou inventar um parâmetro.")
            : QStringLiteral("The current patch does not offer a proven small change in that direction without hitting a limit or inventing a parameter.");
        return advice;
    }
    QStringList rendered;
    for (int index = 0; index < advice.suggestions.size(); ++index)
        rendered.append(suggestionText(advice.suggestions.at(index), language,
                                       index == 0));
    advice.response = rendered.join(QLatin1Char(' '));
    if (!advice.caveats.isEmpty())
        advice.response += QLatin1Char(' ') + advice.caveats.first();
    return advice;
}

} // namespace Tone
} // namespace Ai
