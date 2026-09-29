#include "ai/tone/ToneResponseComposer.h"

#include "ai/tone/ToneRules.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QMap>

namespace Ai {
namespace Tone {
namespace {

bool portuguese(const QString &language)
{
    return language.startsWith(QStringLiteral("pt"), Qt::CaseInsensitive);
}

QJsonObject effect(const AIContext &context, const QString &id)
{
    for (const QJsonValue &value : context.root.value("effects").toArray()) {
        const QJsonObject object = value.toObject();
        if (object.value("id").toString() == id)
            return object;
    }
    return {};
}

bool active(const QJsonObject &object)
{
    return !object.isEmpty()
        && (!object.contains("enabled") || object.value("enabled").toBool());
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

QString parameterLabel(const QString &id)
{
    static const QMap<QString, QString> labels = {
        {"gain", "Gain"}, {"bass", "Bass"}, {"middle", "Middle"},
        {"treble", "Treble"}, {"presence", "Presence"},
        {"drive", "Drive"}, {"effectLevel", "Effect Level"},
        {"feedback", "Feedback"}, {"reverbTime", "Reverb Time"},
        {"channelDelay", "Channel Delay"}, {"sustain", "Sustain"}
    };
    return labels.value(id, id);
}

QString suggestionSentence(const ToneAdjustmentSuggestion &suggestion,
                           const QString &language, bool alternative)
{
    const bool pt = portuguese(language);
    const QString block = ToneRules::blockLabel(suggestion.blockId, language);
    if (suggestion.optionalActivation) {
        return pt
            ? QString("Como alternativa, %1 pode ser considerado de forma opcional (%2). %3")
                  .arg(block, suggestion.suggestedTargetRange,
                       suggestion.reason)
            : QString("As an alternative, %1 can be considered as an optional step (%2). %3")
                  .arg(block, suggestion.suggestedTargetRange,
                       suggestion.reason);
    }
    const QString action = suggestion.suggestedDirection == "increase"
        ? (pt ? QStringLiteral("subir") : QStringLiteral("raise"))
        : (pt ? QStringLiteral("reduzir") : QStringLiteral("lower"));
    if (pt) {
        return QString("%1%2 %3 do %4, atualmente em %5, para aproximadamente %6. %7")
            .arg(alternative ? QStringLiteral("Como segunda rota, ")
                             : QStringLiteral(""),
                 action, parameterLabel(suggestion.parameterId), block,
                 suggestion.currentValue, suggestion.suggestedTargetRange,
                 suggestion.reason);
    }
    return QString("%1%2 %3 on %4, currently %5, to approximately %6. %7")
        .arg(alternative ? QStringLiteral("As a second route, ")
                         : QStringLiteral(""),
             action, parameterLabel(suggestion.parameterId), block,
             suggestion.currentValue, suggestion.suggestedTargetRange,
             suggestion.reason);
}

} // namespace

QString ToneResponseComposer::explain(const ToneAnalysis &analysis,
                                      const AIContext &context,
                                      const QString &language,
                                      bool detailed) const
{
    if (analysis.features.isEmpty())
        return analysis.summary;
    const bool pt = portuguese(language);
    QStringList paragraphs;
    paragraphs.append(analysis.summary);

    const QJsonObject routing = context.root.value("routing").toObject();
    const QJsonObject ampA = effect(context, "preampA");
    const QJsonObject ampB = effect(context, "preampB");
    QStringList architecture;
    if (!routing.value("mode").toString().isEmpty()) {
        architecture.append(pt
            ? QString("O routing está em %1").arg(routing.value("mode").toString())
            : QString("Routing is set to %1").arg(routing.value("mode").toString()));
    }
    if (active(ampA)) {
        architecture.append(pt
            ? QString("o PREAMP A usa %1 com Gain %2 e Middle %3")
                  .arg(value(ampA, "type"), value(ampA, "gain"),
                       value(ampA, "middle"))
            : QString("PREAMP A uses %1 with Gain %2 and Middle %3")
                  .arg(value(ampA, "type"), value(ampA, "gain"),
                       value(ampA, "middle")));
    }
    if (active(ampB)) {
        architecture.append(pt
            ? QString("o PREAMP B usa %1 com Gain %2 e Middle %3")
                  .arg(value(ampB, "type"), value(ampB, "gain"),
                       value(ampB, "middle"))
            : QString("PREAMP B uses %1 with Gain %2 and Middle %3")
                  .arg(value(ampB, "type"), value(ampB, "gain"),
                       value(ampB, "middle")));
    }
    if (!architecture.isEmpty()) {
        paragraphs.append(architecture.join(pt ? QStringLiteral("; ")
                                                : QStringLiteral("; "))
                          + (pt
                              ? QStringLiteral(". Essa combinação define a base de ganho, corpo e contraste do preset.")
                              : QStringLiteral(". That combination defines the patch's gain structure, body, and contrast.")));
    }

    const QJsonObject delay = effect(context, "delay");
    const QJsonObject reverb = effect(context, "reverb");
    const QJsonObject eq = effect(context, "eq");
    QStringList ambience;
    if (active(delay)) {
        const QJsonObject delay1 = delay.value("delay1").toObject();
        const QJsonObject delay2 = delay.value("delay2").toObject();
        QString times;
        if (!delay1.isEmpty() || !delay2.isEmpty())
            times = QString(" (%1 / %2)").arg(value(delay1, "time"),
                                               value(delay2, "time"));
        ambience.append(pt
            ? QString("o Delay %1%2 cria repetição e movimento")
                  .arg(value(delay, "type"), times)
            : QString("the %1 Delay%2 adds repeats and movement")
                  .arg(value(delay, "type"), times));
    }
    if (active(reverb)) {
        ambience.append(pt
            ? QString("o Reverb %1 em %2, com Effect Level %3, sustenta a profundidade e a cauda")
                  .arg(value(reverb, "type"), value(reverb, "reverbTime"),
                       value(reverb, "effectLevel"))
            : QString("the %1 Reverb at %2, with Effect Level %3, supports depth and tail length")
                  .arg(value(reverb, "type"), value(reverb, "reverbTime"),
                       value(reverb, "effectLevel")));
    }
    if (active(eq) && !value(eq, "lowCut").isEmpty()) {
        ambience.append(pt
            ? QString("o EQ com Low Cut em %1 ajuda a controlar o acúmulo de graves")
                  .arg(value(eq, "lowCut"))
            : QString("EQ with Low Cut at %1 helps control low-frequency buildup")
                  .arg(value(eq, "lowCut")));
    }
    if (!ambience.isEmpty())
        paragraphs.append((pt ? QStringLiteral("Na parte de espaço, ")
                              : QStringLiteral("For space and ambience, "))
                          + ambience.join(pt ? QStringLiteral("; ")
                                             : QStringLiteral("; ")) + ".");

    if (detailed && !analysis.caveats.isEmpty())
        paragraphs.append(analysis.caveats.first());
    return paragraphs.join(QStringLiteral("\n\n"));
}

QString ToneResponseComposer::recommend(const ToneRecommendationRecord &record,
                                        const AIContext &context,
                                        const QString &language,
                                        bool refined) const
{
    Q_UNUSED(context)
    if (!record.valid || record.suggestions.isEmpty())
        return clarify(language);
    const bool pt = portuguese(language);
    QStringList paragraphs;
    const ToneAdjustmentSuggestion &primary = record.suggestions.first();
    const QString opening = refined
        ? (pt ? QStringLiteral("Com essa restrição, eu refinaria o plano sem mudar a direção tonal.")
              : QStringLiteral("With that constraint, I would refine the plan without changing its tonal direction."))
        : (primary.optionalActivation
            ? (pt ? QStringLiteral("Neste patch, a opção mais segura é tratar essa mudança como um passo opcional.")
                  : QStringLiteral("In this patch, the safest route is to treat that change as an optional step."))
            : (pt ? QStringLiteral("Neste patch, o primeiro ponto em que eu mexeria é o bloco que já está contribuindo para essa característica.")
                  : QStringLiteral("In this patch, I would first adjust the block that is already contributing to that trait.")));
    paragraphs.append(opening + QLatin1Char(' ')
                      + suggestionSentence(primary, language, false));
    if (record.suggestions.size() > 1)
        paragraphs.append(suggestionSentence(record.suggestions.at(1),
                                             language, true));
    paragraphs.append(pt
        ? QString("O trade-off é %1 Por isso a faixa sugerida continua pequena e serve como primeiro teste, não como valor absoluto.")
              .arg(record.tradeOff)
        : QString("The trade-off is %1 That is why the suggested range remains small and is a first test, not an absolute setting.")
              .arg(record.tradeOff));
    return paragraphs.join(QStringLiteral("\n\n"));
}

QString ToneResponseComposer::justify(const ToneRecommendationRecord &record,
                                      const QString &language,
                                      bool detailed) const
{
    if (!record.valid || record.suggestions.isEmpty())
        return clarify(language);
    const bool pt = portuguese(language);
    const ToneAdjustmentSuggestion &primary = record.suggestions.first();
    QString answer = pt
        ? QString("Sugeri %1 no %2 porque %3")
              .arg(parameterLabel(primary.parameterId),
                   ToneRules::blockLabel(primary.blockId, language),
                   record.rationale)
        : QString("I suggested %1 on %2 because %3")
              .arg(parameterLabel(primary.parameterId),
                   ToneRules::blockLabel(primary.blockId, language),
                   record.rationale);
    answer += pt
        ? QString(" O valor atual é %1 e a faixa %2 mantém a mudança controlada.")
              .arg(primary.currentValue, primary.suggestedTargetRange)
        : QString(" Its current value is %1, and the %2 range keeps the change controlled.")
              .arg(primary.currentValue, primary.suggestedTargetRange);
    if (record.suggestions.size() > 1) {
        const ToneAdjustmentSuggestion &alternative = record.suggestions.at(1);
        answer += pt
            ? QString(" Não comecei pelo %1 porque ele representa uma alternativa com trade-off diferente: %2")
                  .arg(ToneRules::blockLabel(alternative.blockId, language),
                       alternative.reason)
            : QString(" I did not start with %1 because it is an alternative with a different trade-off: %2")
                  .arg(ToneRules::blockLabel(alternative.blockId, language),
                       alternative.reason);
    }
    if (detailed)
        answer += pt ? QString(" O principal cuidado é %1").arg(record.tradeOff)
                     : QString(" The main caution is %1").arg(record.tradeOff);
    return answer;
}

QString ToneResponseComposer::compare(const AIContext &context,
                                      const QString &prompt,
                                      const ToneRecommendationRecord &record,
                                      const QString &language) const
{
    const bool pt = portuguese(language);
    const QJsonObject amp = effect(context, "preampA");
    const QJsonObject od = effect(context, "odDs");
    const QJsonObject delay = effect(context, "delay");
    const QJsonObject reverb = effect(context, "reverb");
    if ((prompt.contains("amp") || prompt.contains("preamp"))
        && (prompt.contains("drive") || prompt.contains("od ds")
            || prompt.contains("blues od"))) {
        const QString odState = active(od) ? QStringLiteral("ON")
                                           : QStringLiteral("OFF");
        return pt
            ? QString("Entre amp e drive, eu começaria pelo amp neste patch. O PREAMP A já está ativo com Gain %1, enquanto o %2 está %3. Ajustar o amp preserva melhor a estrutura atual; ativar ou aumentar o drive tende a produzir uma mudança maior de compressão e saturação.")
                  .arg(value(amp, "gain"), value(od, "type"), odState)
            : QString("Between amp and drive, I would start with the amp in this patch. PREAMP A is already active at Gain %1, while %2 is %3. Adjusting the amp better preserves the current structure; enabling or raising drive tends to make a larger compression and saturation change.")
                  .arg(value(amp, "gain"), value(od, "type"), odState);
    }
    if (prompt.contains("reverb time") && prompt.contains("effect level")) {
        const QString time = value(reverb, "reverbTime");
        return pt
            ? QString("Reverb Time controla principalmente a duração da cauda; Effect Level controla quanto dessa cauda entra no balanço. Como o tempo atual já está em %1, eu priorizaria Effect Level para ganhar presença sem alongar ainda mais a resposta.").arg(time)
            : QString("Reverb Time mainly controls tail length; Effect Level controls how much of that tail enters the balance. Since the current time is already %1, I would prioritize Effect Level for more presence without extending the response further.").arg(time);
    }
    if (prompt.contains("treble") && prompt.contains("presence")) {
        return pt
            ? QString("Treble altera o conteúdo agudo do pré de forma mais ampla; Presence tende a atuar na sensação de ataque e abertura do estágio do amp. Com Treble %1 e Presence %2, eu escolheria apenas um: Treble para brilho geral, Presence para mais recorte e ataque.")
                  .arg(value(amp, "treble"), value(amp, "presence"))
            : QString("Treble changes the preamp's high-frequency content more broadly; Presence tends to affect attack and openness at the amp stage. With Treble %1 and Presence %2, I would choose only one: Treble for overall brightness, Presence for more edge and attack.")
                  .arg(value(amp, "treble"), value(amp, "presence"));
    }
    if (prompt.contains("delay") && prompt.contains("reverb")) {
        return pt
            ? QString("Delay e Reverb cumprem papéis diferentes aqui: o %1 cria repetições/movimento, enquanto o %2 define cauda e profundidade. Para mais movimento, ajuste Delay; para mais sensação de espaço contínuo, ajuste Reverb.")
                  .arg(value(delay, "type"), value(reverb, "type"))
            : QString("Delay and Reverb serve different roles here: %1 creates repeats and movement, while %2 defines tail and depth. Adjust Delay for more movement, or Reverb for a more continuous sense of space.")
                  .arg(value(delay, "type"), value(reverb, "type"));
    }
    if (record.valid && record.suggestions.size() > 1) {
        return pt
            ? QString("As duas rotas válidas são %1 e %2. Eu manteria a primeira porque interfere menos na estrutura atual; a segunda continua disponível como alternativa.")
                  .arg(ToneRules::blockLabel(record.suggestions.at(0).blockId, language),
                       ToneRules::blockLabel(record.suggestions.at(1).blockId, language))
            : QString("The two valid routes are %1 and %2. I would keep the first because it changes the current structure less; the second remains available as an alternative.")
                  .arg(ToneRules::blockLabel(record.suggestions.at(0).blockId, language),
                       ToneRules::blockLabel(record.suggestions.at(1).blockId, language));
    }
    return clarify(language);
}

QString ToneResponseComposer::feedback(const ToneRecommendationRecord &record,
                                       const QString &prompt,
                                       const QString &language) const
{
    if (!record.valid || record.suggestions.isEmpty())
        return clarify(language);
    const bool pt = portuguese(language);
    const ToneAdjustmentSuggestion &primary = record.suggestions.first();
    if (prompt == "gostei" || prompt == "i like it") {
        return pt
            ? QString("Ótimo. Eu manteria esse ajuste de %1 no %2 dentro da faixa %3; não há motivo para ampliar a mudança sem uma nova direção tonal.")
                  .arg(parameterLabel(primary.parameterId),
                       ToneRules::blockLabel(primary.blockId, language),
                       primary.suggestedTargetRange)
            : QString("Great. I would keep that %1 adjustment on %2 within the %3 range; there is no reason to expand it without a new tonal direction.")
                  .arg(parameterLabel(primary.parameterId),
                       ToneRules::blockLabel(primary.blockId, language),
                       primary.suggestedTargetRange);
    }
    if (prompt.contains("demais") || prompt.contains("too much")
        || prompt.contains("voltar") || prompt.contains("back off")
        || prompt == "nao gostei" || prompt == "i do not like it") {
        return pt
            ? QString("Então eu recuaria um pouco no mesmo ajuste de %1 do %2. Em vez de avançar por toda a faixa %3, teste o limite mais próximo do valor atual %4; isso preserva a direção sem manter o excesso percebido.")
                  .arg(parameterLabel(primary.parameterId),
                       ToneRules::blockLabel(primary.blockId, language),
                       primary.suggestedTargetRange, primary.currentValue)
            : QString("Then I would back off the same %1 adjustment on %2. Instead of using the whole %3 range, test the end closest to the current %4; that keeps the direction without retaining the perceived excess.")
                  .arg(parameterLabel(primary.parameterId),
                       ToneRules::blockLabel(primary.blockId, language),
                       primary.suggestedTargetRange, primary.currentValue);
    }
    return pt
        ? QString("Para avançar mais um pouco, eu manteria o mesmo %1 do %2 e passaria gradualmente além da primeira faixa %3, sem pular direto para uma mudança grande. O trade-off continua sendo %4")
              .arg(parameterLabel(primary.parameterId),
                   ToneRules::blockLabel(primary.blockId, language),
                   primary.suggestedTargetRange, record.tradeOff)
        : QString("To move a little further, I would keep the same %1 adjustment on %2 and move gradually beyond the first %3 range, without jumping straight to a large change. The trade-off remains %4")
              .arg(parameterLabel(primary.parameterId),
                   ToneRules::blockLabel(primary.blockId, language),
                   primary.suggestedTargetRange, record.tradeOff);
}

QString ToneResponseComposer::clarify(const QString &language,
                                      bool patchChanged) const
{
    if (portuguese(language)) {
        return patchChanged
            ? QStringLiteral("O patch mudou desde a recomendação anterior, então descartei aquele ajuste para não misturar contextos. Diga qual direção você quer neste novo preset — por exemplo, mais crunch, mais ambiente ou mais definido.")
            : QStringLiteral("Qual direção você quer priorizar? Posso trabalhar, por exemplo, com mais crunch, mais clean, mais ambiente, mais definição, brilho ou foco.");
    }
    return patchChanged
        ? QStringLiteral("The patch changed after the previous recommendation, so I discarded that adjustment rather than mixing contexts. Tell me which direction you want for this new patch—for example, more crunch, ambience, or definition.")
        : QStringLiteral("Which direction do you want to prioritize? For example, I can work toward more crunch, clean, ambience, definition, brightness, or focus.");
}

} // namespace Tone
} // namespace Ai
