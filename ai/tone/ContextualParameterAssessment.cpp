#include "ai/tone/ContextualParameterAssessment.h"

#include "ai/tone/ToneIntentParser.h"
#include "ai/tone/ToneRules.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>

namespace Ai {
namespace Tone {
namespace {

bool portuguese(const QString &language)
{
    return language.startsWith(QStringLiteral("pt"), Qt::CaseInsensitive);
}

QString displayName(QString id)
{
    id.replace(QRegularExpression(QStringLiteral("([a-z0-9])([A-Z])")),
               QStringLiteral("\\1 \\2"));
    if (!id.isEmpty()) id[0] = id.at(0).toUpper();
    return id;
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

QString parameterValue(const AIContext &context, const QString &block,
                       const QString &parameter)
{
    if (block == QLatin1String("preampGlobal"))
        return context.root.value("routing").toObject()
            .value(parameter).toString();
    const QJsonObject object = effect(context, block);
    QString result = object.value(parameter).toString();
    if (result.isEmpty() && block == QLatin1String("delay")
        && parameter == QLatin1String("feedback")) {
        const QString first = object.value("delay1").toObject()
            .value("feedback").toString();
        const QString second = object.value("delay2").toObject()
            .value("feedback").toString();
        if (!first.isEmpty())
            result = second.isEmpty() || second == first
                ? first : first + QStringLiteral(" / ") + second;
    }
    return result;
}

double number(const QString &text, bool *ok)
{
    const QRegularExpressionMatch match = QRegularExpression(
        QStringLiteral("[+-]?\\d+(?:[.,]\\d+)?")).match(text);
    bool parsed = false;
    const double value = match.hasMatch()
        ? QString(match.captured()).replace(',', '.').toDouble(&parsed) : 0.0;
    if (ok) *ok = parsed;
    return value;
}

QString explicitValue(const QString &text)
{
    const QString normalized = ToneIntentParser::normalize(text);
    const QRegularExpressionMatch match = QRegularExpression(
        QStringLiteral("(?:^|\\s)([+-]?\\d+(?:[.,]\\d+)?\\s*(?:ms|s|sec|segundos?|khz|hz|db|%)?)(?=\\s|$)"),
        QRegularExpression::CaseInsensitiveOption).match(normalized);
    return match.hasMatch() ? match.captured(1).trimmed() : QString();
}

QString contextValue(const AIContext &context, const QString &block,
                     const QString &parameter)
{
    return parameterValue(context, block, parameter);
}

QString assessmentCategory(double value, const ToneParameterRule &rule)
{
    const double strength = ToneRules::normalizedStrength(
        value, rule.minimum, rule.maximum);
    if (strength >= 0.78) return QStringLiteral("high_for_context");
    if (strength <= 0.22) return QStringLiteral("low_for_context");
    return QStringLiteral("coherent");
}

QString patchIdentity(const AIContext &context)
{
    const QJsonObject patch = context.root.value("patch").toObject();
    return patch.value("number").toString() + QLatin1Char('|')
        + patch.value("name").toString();
}

} // namespace

ParameterAssessment ContextualParameterAssessment::assess(
    const AIRequest &request, const Knowledge::QuestionEntity &entity,
    const QString &evaluatedValue) const
{
    ParameterAssessment result;
    result.blockId = entity.family;
    result.parameterId = entity.id;
    result.currentValue = contextValue(request.context, result.blockId,
                                       result.parameterId);
    result.userReferencedValue = evaluatedValue.isEmpty()
        ? entity.userValue : evaluatedValue;
    if (result.currentValue.isEmpty() || result.parameterId.isEmpty())
        return result;

    ToneParameterRule rule;
    if (ToneRules::parameterRule(result.blockId, result.parameterId, &rule)) {
        result.range = QStringLiteral("%1–%2")
            .arg(rule.minimum).arg(rule.maximum);
        result.supportingEvidence.append(QStringLiteral("tone_rule_range"));
        if (rule.sourceKind == ToneSourceKind::OfficialDocumentation)
            result.supportingEvidence.append(
                QStringLiteral("official_documentation_range"));
    }
    result.supportingEvidence.append(QStringLiteral("current_patch_context"));

    bool evaluatedOk = false;
    double evaluated = number(result.userReferencedValue.isEmpty()
        ? result.currentValue : result.userReferencedValue, &evaluatedOk);
    if (evaluatedOk && !result.range.isEmpty())
        result.assessment = assessmentCategory(evaluated, rule);
    else
        result.assessment = QStringLiteral("depends_on_goal");

    const bool pt = portuguese(request.language);
    if (result.parameterId == QLatin1String("preDelay")) {
        result.contextualRole = pt
            ? QStringLiteral("separa o ataque direto do início da ambiência")
            : QStringLiteral("separates the direct attack from the onset of the ambience");
        result.supportingEvidence.append(
            QStringLiteral("official_parameter_semantics"));
        const QJsonObject reverb = effect(request.context, "reverb");
        const QJsonObject delay = effect(request.context, "delay");
        const QString reverbTime = reverb.value("reverbTime").toString();
        if (!reverbTime.isEmpty())
            result.supportingEvidence.append(
                QStringLiteral("reverb.reverbTime=") + reverbTime);
        if (delay.value("enabled").toBool())
            result.supportingEvidence.append(QStringLiteral("delay.active"));
        result.tradeoffs << (pt
            ? QStringLiteral("mais separação e clareza do ataque")
            : QStringLiteral("more separation and attack clarity"))
            << (pt
            ? QStringLiteral("menos sensação de ambiência imediata e colada")
            : QStringLiteral("less immediate, glued ambience"));
        if (evaluatedOk && evaluated >= 80.0)
            result.assessment = QStringLiteral("reasonable_for_goal");
    } else if (result.parameterId == QLatin1String("feedback")) {
        result.contextualRole = pt
            ? QStringLiteral("controla quantas repetições retornam ao delay")
            : QStringLiteral("controls how much repeated signal returns to the delay");
        result.supportingEvidence.append(
            QStringLiteral("official_parameter_semantics"));
        const QString level = contextValue(request.context, "delay",
                                           "effectLevel");
        if (!level.isEmpty())
            result.supportingEvidence.append(
                QStringLiteral("delay.effectLevel=") + level);
        if (effect(request.context, "reverb").value("enabled").toBool())
            result.supportingEvidence.append(QStringLiteral("reverb.active"));
        result.tradeoffs << (pt ? QStringLiteral("mais sustentação e repetições")
                                : QStringLiteral("more sustain and repeats"))
                         << (pt ? QStringLiteral("maior risco de acúmulo")
                                : QStringLiteral("greater buildup risk"));
    } else if (result.parameterId == QLatin1String("reverbTime")) {
        result.contextualRole = pt
            ? QStringLiteral("define a duração da cauda do reverb")
            : QStringLiteral("sets the duration of the reverb tail");
        result.supportingEvidence.append(
            QStringLiteral("official_parameter_semantics"));
        result.tradeoffs << (pt ? QStringLiteral("mais profundidade")
                                : QStringLiteral("more depth"))
                         << (pt ? QStringLiteral("menos definição em passagens densas")
                                : QStringLiteral("less definition in dense passages"));
    } else if (result.parameterId == QLatin1String("gain")
               || result.parameterId == QLatin1String("presence")
               || result.parameterId == QLatin1String("treble")) {
        result.contextualRole = pt
            ? QStringLiteral("molda a resposta e o caráter do preamp")
            : QStringLiteral("shapes the preamp response and character");
        const QJsonObject amp = effect(request.context, result.blockId);
        for (const char *peer : {"gain", "treble", "presence"}) {
            const QString value = amp.value(peer).toString();
            if (!value.isEmpty() && result.parameterId != QLatin1String(peer))
                result.supportingEvidence.append(
                    QStringLiteral("preamp.") + peer + QLatin1Char('=') + value);
        }
        result.tradeoffs << (pt ? QStringLiteral("mais presença ou saturação")
                                : QStringLiteral("more presence or saturation"))
                         << (pt ? QStringLiteral("possível aspereza ou compressão")
                                : QStringLiteral("possible harshness or compression"));
    } else if (result.parameterId == QLatin1String("lowCut")
               || result.parameterId == QLatin1String("highCut")) {
        result.contextualRole = pt
            ? QStringLiteral("controla a faixa de frequência preservada pelo bloco")
            : QStringLiteral("controls the frequency range retained by the block");
        result.tradeoffs << (pt ? QStringLiteral("mais limpeza e foco")
                                : QStringLiteral("more cleanliness and focus"))
                         << (pt ? QStringLiteral("possível perda de corpo ou brilho")
                                : QStringLiteral("possible loss of body or brightness"));
    } else if (result.parameterId == QLatin1String("channelDelay")) {
        result.contextualRole = pt
            ? QStringLiteral("desloca temporalmente os canais do preamp")
            : QStringLiteral("offsets the preamp channels in time");
        result.tradeoffs << (pt ? QStringLiteral("mais largura percebida")
                                : QStringLiteral("more perceived width"))
                         << (pt ? QStringLiteral("possível perda de foco")
                                : QStringLiteral("possible loss of focus"));
    } else {
        result.contextualRole = pt
            ? QStringLiteral("ajusta o comportamento do bloco no patch")
            : QStringLiteral("adjusts the block behavior in the patch");
        result.tradeoffs << (pt ? QStringLiteral("o resultado depende do objetivo tonal")
                                : QStringLiteral("the result depends on the tonal goal"));
    }
    result.confidence = result.supportingEvidence.contains(
        QStringLiteral("official_parameter_semantics"))
        ? Knowledge::Confidence::Derived : Knowledge::Confidence::Guidance;
    result.valid = true;
    return result;
}

QString ContextualParameterAssessment::compose(
    const ParameterAssessment &value, const AIRequest &request,
    bool hypothetical) const
{
    const bool pt = portuguese(request.language);
    const QString block = ToneRules::blockLabel(value.blockId,
                                                 request.language);
    const QString parameter = displayName(value.parameterId);
    const QString evaluated = value.userReferencedValue.isEmpty()
        ? value.currentValue : value.userReferencedValue;
    const bool differs = !value.userReferencedValue.isEmpty()
        && number(value.userReferencedValue, nullptr)
           != number(value.currentValue, nullptr);

    QString direct;
    if (hypothetical) {
        direct = pt
            ? QStringLiteral("Baixar %1 de %2 para %3 é uma alternativa coerente.")
                .arg(parameter, value.currentValue, evaluated)
            : QStringLiteral("Lowering %1 from %2 to %3 is a coherent alternative.")
                .arg(parameter, value.currentValue, evaluated);
    } else if (value.assessment == QLatin1String("high_for_context")) {
        direct = pt
            ? QStringLiteral("%1 está relativamente alto, mas pode fazer sentido no contexto do %2.")
                .arg(evaluated, block)
            : QStringLiteral("%1 is relatively high, but it can make sense in the %2 context.")
                .arg(evaluated, block);
    } else {
        direct = pt
            ? QStringLiteral("Sim, %1 pode fazer sentido para %2 no %3.")
                .arg(evaluated, parameter, block)
            : QStringLiteral("Yes, %1 can make sense for %2 in %3.")
                .arg(evaluated, parameter, block);
    }
    if (differs && !hypothetical) {
        direct += pt
            ? QStringLiteral(" Seu patch atual está em %1; estou avaliando %2 como a alternativa mencionada.")
                .arg(value.currentValue, value.userReferencedValue)
            : QStringLiteral(" Your current patch is at %1; I am assessing %2 as the referenced alternative.")
                .arg(value.currentValue, value.userReferencedValue);
    }
    QString contextDetail;
    if (value.parameterId == QLatin1String("preDelay")) {
        const QJsonObject reverb = effect(request.context, "reverb");
        const QJsonObject delay = effect(request.context, "delay");
        const QString type = reverb.value("type").toString();
        const QString time = reverb.value("reverbTime").toString();
        if (pt) {
            contextDetail = QStringLiteral("O Reverb %1 já tem uma cauda de %2%3.")
                .arg(type, time, delay.value("enabled").toBool()
                    ? QStringLiteral(" e o Delay também está ativo") : QString());
        } else {
            contextDetail = QStringLiteral("The %1 Reverb already has a %2 tail%3.")
                .arg(type, time, delay.value("enabled").toBool()
                    ? QStringLiteral(" and Delay is also active") : QString());
        }
    }
    const QString reason = pt
        ? QStringLiteral("Nesse contexto, %1. %2")
            .arg(value.contextualRole, contextDetail)
        : QStringLiteral("In this context, it %1. %2")
            .arg(value.contextualRole, contextDetail);
    const QString tradeoff = value.tradeoffs.isEmpty() ? QString() : (pt
        ? QStringLiteral("O trade-off é %1 versus %2.")
            .arg(value.tradeoffs.value(0), value.tradeoffs.value(1))
        : QStringLiteral("The trade-off is %1 versus %2.")
            .arg(value.tradeoffs.value(0), value.tradeoffs.value(1)));
    return direct + QStringLiteral("\n\n") + reason
        + (tradeoff.isEmpty() ? QString() : QStringLiteral("\n\n") + tradeoff);
}

ToneConversationReply ContextualParameterAssessment::respond(
    const AIRequest &request,
    const Knowledge::QuestionRoutingResult &routing)
{
    const QString identity = patchIdentity(request.context);
    if (!previousPatchIdentity.isEmpty() && previousPatchIdentity != identity)
        previous = ParameterAssessment();
    previousPatchIdentity = identity;
    ToneConversationReply reply;
    reply.handled = true;
    reply.route = QStringLiteral("parameter_assessment");
    reply.topicId = QStringLiteral("parameter_assessment");

    Knowledge::QuestionEntity parameter;
    for (const Knowledge::QuestionEntity &entity : routing.entities) {
        if (entity.kind == Knowledge::QuestionEntityKind::Parameter) {
            parameter = entity;
            break;
        }
    }
    const bool hypothetical = parameter.id.isEmpty() && previous.valid;
    if (hypothetical) {
        parameter.kind = Knowledge::QuestionEntityKind::Parameter;
        parameter.family = previous.blockId;
        parameter.id = previous.parameterId;
        parameter.canonicalName = displayName(previous.parameterId);
        parameter.userValue = explicitValue(request.userMessage);
    }
    const QString evaluated = hypothetical ? parameter.userValue : QString();
    const ParameterAssessment result = assess(request, parameter, evaluated);
    if (!result.valid) {
        reply.mode = ToneResponseMode::Clarify;
        reply.text = portuguese(request.language)
            ? QStringLiteral("Não encontrei esse parâmetro com um valor disponível no patch atual.")
            : QStringLiteral("I could not find that parameter with an available value in the current patch.");
        return reply;
    }
    previous = result;
    reply.answered = true;
    reply.mode = hypothetical ? ToneResponseMode::HypotheticalCompare
                              : ToneResponseMode::Assess;
    reply.text = compose(result, request, hypothetical);
    return reply;
}

ToneConversationReply ContextualParameterAssessment::justify(
    const AIRequest &request)
{
    ToneConversationReply reply;
    const QString identity = patchIdentity(request.context);
    if (!previousPatchIdentity.isEmpty() && previousPatchIdentity != identity) {
        previous = ParameterAssessment();
        previousPatchIdentity = identity;
    }
    reply.handled = previous.valid;
    if (!reply.handled)
        return reply;
    reply.answered = true;
    reply.mode = ToneResponseMode::Assess;
    reply.route = QStringLiteral("parameter_assessment_justify");
    reply.topicId = QStringLiteral("parameter_assessment_justify");
    const bool pt = portuguese(request.language);
    reply.text = pt
        ? QStringLiteral("Porque %1. A avaliação usa o valor atual %2 e as interações visíveis no patch; não é uma regra absoluta. %3")
            .arg(previous.contextualRole, previous.currentValue,
                 previous.tradeoffs.join(QStringLiteral("; ")))
        : QStringLiteral("Because it %1. The assessment uses the current value %2 and the interactions visible in the patch; it is not an absolute rule. %3")
            .arg(previous.contextualRole, previous.currentValue,
                 previous.tradeoffs.join(QStringLiteral("; ")));
    return reply;
}

bool ContextualParameterAssessment::hasAssessment() const
{
    return previous.valid;
}

const ParameterAssessment &ContextualParameterAssessment::lastAssessment() const
{
    return previous;
}

void ContextualParameterAssessment::reset()
{
    previous = ParameterAssessment();
    previousPatchIdentity.clear();
}

} // namespace Tone
} // namespace Ai
