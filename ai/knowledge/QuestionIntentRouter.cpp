#include "ai/knowledge/QuestionIntentRouter.h"

#include "ai/AIProvider.h"
#include "ai/PatchParameterCatalog.h"
#include "ai/tone/ToneIntentParser.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QRegularExpression>
#include <algorithm>

namespace Ai {
namespace Knowledge {
namespace {

bool any(const QString &text, std::initializer_list<const char *> terms)
{
    for (const char *term : terms) {
        if (text.contains(QString::fromLatin1(term)))
            return true;
    }
    return false;
}

void appendUnique(QList<QuestionEntity> *entities,
                  const QuestionEntity &candidate)
{
    for (const QuestionEntity &entity : *entities) {
        if (entity.kind == candidate.kind && entity.family == candidate.family
            && entity.canonicalName.compare(candidate.canonicalName,
                                             Qt::CaseInsensitive) == 0)
            return;
    }
    entities->append(candidate);
}

QString familyId(const QString &text)
{
    const auto word = [&text](const QString &pattern) {
        return QRegularExpression(QStringLiteral("(^| )%1($| )").arg(pattern))
            .match(text).hasMatch();
    };
    if (word(QStringLiteral("reverbs?"))
        || word(QStringLiteral("reverberac(?:ao|oes)")))
        return QStringLiteral("reverb");
    if (word(QStringLiteral("chorus(?:es)?")))
        return QStringLiteral("chorus");
    const bool preAlias = word(QStringLiteral("pre"))
        && !text.contains(QStringLiteral("pre delay"))
        && !text.contains(QStringLiteral("predelay"));
    if (word(QStringLiteral("pre ?amps?"))
        || word(QStringLiteral("amplificadores?"))
        || word(QStringLiteral("amps?")) || preAlias)
        return QStringLiteral("preamp");
    if (text.contains("od ds") || word(QStringLiteral("overdrives?"))
        || word(QStringLiteral("distortions?"))
        || word(QStringLiteral("distorc(?:ao|oes)"))
        || word(QStringLiteral("drives?")))
        return QStringLiteral("odDs");
    if (word(QStringLiteral("delays?")) || word(QStringLiteral("atrasos?")))
        return QStringLiteral("delay");
    if (word(QStringLiteral("compressors?"))
        || word(QStringLiteral("compressores?"))
        || word(QStringLiteral("comp")))
        return QStringLiteral("comp");
    if (word(QStringLiteral("fx ?1"))) return QStringLiteral("fx1");
    if (word(QStringLiteral("fx ?2"))) return QStringLiteral("fx2");
    if (word(QStringLiteral("p ?fx")) || text.contains("pedal fx")
        || text.contains("foot volume"))
        return QStringLiteral("pedalFxFv");
    return {};
}

QString readableId(QString id)
{
    id.replace(QRegularExpression(QStringLiteral("([a-z0-9])([A-Z])")),
               QStringLiteral("\\1 \\2"));
    if (!id.isEmpty())
        id[0] = id.at(0).toUpper();
    return id;
}

QString semanticFamily(const QString &block)
{
    if (block == QLatin1String("preampA")
        || block == QLatin1String("preampB"))
        return QStringLiteral("preamp");
    return block;
}

struct ParameterEntityCandidate
{
    QString block;
    QString family;
    QString id;
    QString label;
    QStringList aliases;
    QString matched;
};

QString aliasNormalized(QString text);

QStringList semanticAliases(const QString &id)
{
    static const QMap<QString, QStringList> aliases = {
        {"preDelay", {"pre delay", "predelay", "pre atraso"}},
        {"channelDelay", {"channel delay", "atraso de canal"}},
        {"effectLevel", {"effect level", "nivel de efeito"}},
        {"directLevel", {"direct level", "nivel direto"}},
        {"gainSwitch", {"gain switch", "chave de ganho"}},
        {"reverbTime", {"reverb time", "tempo de reverb",
                         "segundos de reverb", "seconds of reverb"}},
        {"delayTime", {"delay time", "tempo de delay"}},
        {"highCut", {"high cut", "hicut", "corte de agudos"}},
        {"lowCut", {"low cut", "locut", "corte de graves"}},
        {"lowMiddleGain", {"low middle gain", "ganho medio grave"}},
        {"highMiddleGain", {"high middle gain", "ganho medio agudo"}},
        {"lowMiddleQ", {"low middle q", "q medio grave"}},
        {"highMiddleQ", {"high middle q", "q medio agudo"}},
        {"feedback", {"feedback"}}, {"gain", {"gain", "ganho"}},
        {"bass", {"bass", "graves"}},
        {"middle", {"middle", "medios"}},
        {"treble", {"treble", "agudos"}},
        {"presence", {"presence", "presenca"}},
        {"density", {"density", "densidade"}},
        {"sustain", {"sustain", "sustentacao"}},
        {"attack", {"attack", "ataque"}},
        {"tone", {"tone", "tonalidade"}},
        {"level", {"level", "nivel"}}
    };
    QStringList result = aliases.value(id);
    const QString readable = GT10CapabilityService::normalize(readableId(id));
    if (!readable.isEmpty() && !result.contains(readable))
        result.append(readable);
    return result;
}

QList<ParameterEntityCandidate> parameterIndex()
{
    QList<ParameterEntityCandidate> result;
    const QList<EffectDescriptor> effects = PatchParameterCatalog::effects({});
    for (const EffectDescriptor &effect : effects) {
        for (const ParameterDescriptor &parameter : effect.parameters) {
            const QStringList aliases = semanticAliases(parameter.id);
            if (aliases.isEmpty())
                continue;
            ParameterEntityCandidate candidate;
            candidate.block = effect.id;
            candidate.family = semanticFamily(effect.id);
            candidate.id = parameter.id;
            candidate.label = readableId(parameter.id);
            candidate.aliases = aliases;
            result.append(candidate);
        }
    }
    // Assign fields are not effect parameters in PatchParameterCatalog. They
    // are nevertheless real catalog/documentation entities exposed by
    // GT10CapabilityService (Target, Source and Source Mode). Keep their
    // semantic aliases here and point them at those real field IDs.
    const auto addAssign = [&result](const char *id, const char *label,
                                     std::initializer_list<const char *> aliases) {
        ParameterEntityCandidate candidate;
        candidate.block = QStringLiteral("assign");
        candidate.family = QStringLiteral("assign");
        candidate.id = QString::fromLatin1(id);
        candidate.label = QString::fromLatin1(label);
        for (const char *alias : aliases)
            candidate.aliases.append(QString::fromLatin1(alias));
        result.append(candidate);
    };
    addAssign("sourceMode", "Source Mode",
              {"source mode", "modo da fonte"});
    addAssign("source", "Source", {"source", "fonte"});
    addAssign("targetMin", "Target Min",
              {"target min", "target minimum", "minimo do target"});
    addAssign("targetMax", "Target Max",
              {"target max", "target maximum", "maximo do target"});
    addAssign("target", "Target", {"target", "alvo"});
    return result;
}

bool phraseMatch(const QString &text, const QString &phrase)
{
    return QRegularExpression(QStringLiteral("(^| )%1($| )")
        .arg(QRegularExpression::escape(aliasNormalized(phrase))))
        .match(text).hasMatch();
}

QString explicitValue(const QString &text)
{
    static const QRegularExpression valueExpression(
        QStringLiteral("(?:^|\\s)([+-]?\\d+(?:[.,]\\d+)?\\s*(?:ms|s|sec|segundos?|khz|hz|db|%)?)(?=\\s|$)"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = valueExpression.match(text);
    if (!match.hasMatch())
        return {};
    QString value = match.captured(1).trimmed();
    value.replace(QRegularExpression(
        QStringLiteral("^(\\d+(?:[.,]\\d+)?)(ms|s|sec|segundos?|khz|hz|db|%)$"),
        QRegularExpression::CaseInsensitiveOption),
        QStringLiteral("\\1 \\2"));
    return value;
}

double numericValue(const QString &text, bool *ok = nullptr)
{
    const QRegularExpressionMatch match = QRegularExpression(
        QStringLiteral("[+-]?\\d+(?:[.,]\\d+)?")).match(text);
    bool parsed = false;
    const double value = match.hasMatch()
        ? QString(match.captured()).replace(',', '.').toDouble(&parsed) : 0.0;
    if (ok) *ok = parsed;
    return value;
}

QJsonObject contextEffect(const AIContext *context, const QString &block)
{
    if (!context)
        return {};
    const QJsonArray effects = context->root.value("effects").toArray();
    for (const QJsonValue &value : effects) {
        const QJsonObject effect = value.toObject();
        if (effect.value("id").toString() == block)
            return effect;
    }
    return {};
}

QString contextParameterValue(const AIContext *context,
                              const ParameterEntityCandidate &candidate)
{
    if (!context)
        return {};
    if (candidate.block == QLatin1String("preampGlobal"))
        return context->root.value("routing").toObject()
            .value(candidate.id).toString();
    const QJsonObject effect = contextEffect(context, candidate.block);
    QString value = effect.value(candidate.id).toString();
    if (value.isEmpty() && candidate.block == QLatin1String("delay")
        && candidate.id == QLatin1String("feedback")) {
        const QString first = effect.value("delay1").toObject()
            .value("feedback").toString();
        const QString second = effect.value("delay2").toObject()
            .value("feedback").toString();
        if (!first.isEmpty() && (second.isEmpty() || first == second))
            value = first;
    }
    return value;
}

bool activeCandidate(const AIContext *context,
                     const ParameterEntityCandidate &candidate)
{
    if (!context)
        return false;
    if (candidate.block == QLatin1String("preampGlobal"))
        return !contextParameterValue(context, candidate).isEmpty();
    if (candidate.block == QLatin1String("preampA")
        || candidate.block == QLatin1String("preampB")) {
        const QString selected = context->root.value("routing").toObject()
            .value("channelSelect").toString();
        if (selected.trimmed().endsWith("A", Qt::CaseInsensitive)
            && candidate.block == QLatin1String("preampB"))
            return false;
        if (selected.trimmed().endsWith("B", Qt::CaseInsensitive)
            && candidate.block == QLatin1String("preampA"))
            return false;
    }
    const QJsonObject effect = contextEffect(context, candidate.block);
    return !effect.isEmpty() && (!effect.contains("enabled")
        || effect.value("enabled").toBool())
        && !contextParameterValue(context, candidate).isEmpty();
}

QString catalogBlock(const QString &family)
{
    if (family == QLatin1String("preamp")) return QStringLiteral("preampA");
    return family;
}

QString aliasNormalized(QString text)
{
    text = GT10CapabilityService::normalize(text);
    text.replace(QRegularExpression(QStringLiteral("\\bmodulado\\b")),
                 QStringLiteral("modulate"));
    text.replace(QRegularExpression(QStringLiteral("\\bambiencia\\b")),
                 QStringLiteral("ambience"));
    return text;
}

bool comparisonCue(const QString &text)
{
    return any(text, {"diferenca", "compare", "comparar", " versus ",
                      " vs ", "difference between", "what changes between",
                      "qual e melhor", "qual eh melhor", "which is better"})
        || text.contains(" ou ") || text.contains(" or ");
}

bool explainCue(const QString &text)
{
    return any(text, {"o que e", "oque e", "o que faz", "oque faz",
                      "faz o que", "faz oque", "para que serve",
                      "pra que serve",
                      "como funciona", "explique", "what is", "what does",
                      "how does", "explain"})
        || QRegularExpression(QStringLiteral("^(?:o que|oque) .+ faz$"))
               .match(text).hasMatch();
}

bool currentPatchCue(const QString &text)
{
    return any(text, {"estou usando", "esta usando", "está usando",
                      "meu patch", "patch atual", "current patch",
                      "am i using", "which reverb am i", "which delay am i",
                      "what reverb am i", "what delay am i", "valor do",
                      "value of", "esta ligado", "is on", "is enabled",
                      "esta selecionado", "tipo selecionado",
                      "is selected", "selected type"});
}

bool listCue(const QString &text)
{
    if (QRegularExpression(QStringLiteral(
            "(^| )(?:parametros?|parameters?)(?:$| )"))
            .match(text).hasMatch())
        return false;
    const bool portuguesePlural =
        (text.startsWith("quais ") || text.startsWith("que "))
        && any(text, {" tem ", " existem", " possui", " ha "});
    const bool englishPlural =
        (text.startsWith("what ") || text.startsWith("which "))
        && (any(text, {" are available", " exist", " has "})
            || QRegularExpression(QStringLiteral(
                   "^what .+ does (?:the )?gt 10 have$"))
                   .match(text).hasMatch());
    return portuguesePlural || englishPlural
        || any(text, {"que tipos de", "quais tipos de", "quais modelos de",
                      "que efeitos", "quais efeitos", "me mostra os", "lista os",
                      "what types of", "what models", "list the",
                      "what effects are in", "what effects are available in"})
        || QRegularExpression(QStringLiteral("^(?:me mostra|lista) (?:os|as) "))
               .match(text).hasMatch();
}

bool justifyCue(const QString &text)
{
    return text == QLatin1String("por que") || text == QLatin1String("porque")
        || text == QLatin1String("why") || any(text, {
            "por que isso", "por que voce sugeriu", "por que voce esta sugerindo",
            "por que esta sugerindo", "por que essa mudanca", "por que esse ajuste",
            "por que mexer", "por que nao", "why did you suggest",
            "why are you suggesting", "why that change", "why not"});
}

bool detailCue(const QString &text)
{
    return any(text, {"explique melhor", "detalhe", "me explique mais",
                      "quero entender melhor", "explain further", "more detail",
                      "me passe valores", "me passe os valores", "valores de partida",
                      "configuracao resumida", "keep it brief", "passo a passo", "step by step"});
}

bool feedbackCue(const QString &text)
{
    return any(text, {"mais um pouco", "ficou pouco", "ficou demais",
                      "voltar um pouco", "gostei", "nao gostei",
                      "a little more", "too much", "not enough", "back off",
                      "i like it", "i do not like it"});
}

bool refineCue(const QString &text)
{
    return any(text, {"preservar", "nao quero", "sem perder", "prefiro nao",
                      "manter o", "preserve", "avoid", "keep ", "without losing"});
}

QList<QuestionEntity> ofKind(const QList<QuestionEntity> &entities,
                             QuestionEntityKind kind)
{
    QList<QuestionEntity> result;
    for (const QuestionEntity &entity : entities) {
        if (entity.kind == kind)
            result.append(entity);
    }
    return result;
}

} // namespace

bool QuestionRoutingResult::isComparison() const
{
    return intent == QuestionIntent::CompareEffectTypes
        || intent == QuestionIntent::CompareParameters
        || intent == QuestionIntent::CompareRecommendation;
}

QJsonArray QuestionRoutingResult::entitiesJson() const
{
    QJsonArray result;
    for (const QuestionEntity &entity : entities) {
        QJsonObject value;
        value.insert("kind", QuestionIntentRouter::entityKindName(entity.kind));
        if (!entity.family.isEmpty()) value.insert("family", entity.family);
        value.insert("name", entity.canonicalName);
        if (!entity.matchedText.isEmpty())
            value.insert("matchedText", entity.matchedText);
        if (!entity.id.isEmpty()) value.insert("id", entity.id);
        if (!entity.userValue.isEmpty())
            value.insert("userValue", entity.userValue);
        if (!entity.matchKind.isEmpty())
            value.insert("matchKind", entity.matchKind);
        result.append(value);
    }
    return result;
}

QList<QuestionEntity> QuestionIntentRouter::resolveEntities(
    const QString &normalized, const QuestionRoutingContext &context) const
{
    QList<QuestionEntity> result;
    const QString text = aliasNormalized(normalized);
    QString explicitFamily = familyId(text);

    QList<ParameterEntityCandidate> matchedParameters;
    for (ParameterEntityCandidate candidate : parameterIndex()) {
        for (const QString &alias : candidate.aliases) {
            const QString normalizedAlias = aliasNormalized(alias);
            if (!phraseMatch(text, normalizedAlias))
                continue;
            if (normalizedAlias.size() > candidate.matched.size())
                candidate.matched = normalizedAlias;
        }
        if (!candidate.matched.isEmpty()) {
            matchedParameters.append(candidate);
        }
    }

    // A compound parameter owns its words. "Pre Delay" and "Channel Delay"
    // are not evidence for the generic DELAY effect family.
    if (explicitFamily == QLatin1String("delay")) {
        bool compoundDelay = false;
        for (const ParameterEntityCandidate &candidate : matchedParameters) {
            if (candidate.matched.contains("delay")
                && candidate.id != QLatin1String("delayTime")) {
                compoundDelay = true;
                break;
            }
        }
        if (compoundDelay)
            explicitFamily.clear();
    }

    if (!explicitFamily.isEmpty()) {
        appendUnique(&result, {QuestionEntityKind::EffectFamily,
                              explicitFamily, explicitFamily, explicitFamily,
                              explicitFamily, {}, {}});
    }

    QStringList blocks;
    if (!explicitFamily.isEmpty()) {
        blocks << catalogBlock(explicitFamily);
    } else {
        blocks = capabilities.blockIds();
    }
    for (const QString &block : blocks) {
        for (const QString &type : capabilities.effectTypesForBlock(block)) {
            QString candidate = aliasNormalized(type);
            QString canonicalType = type;
            if (block == QLatin1String("reverb")
                && candidate == QLatin1String("mod")) {
                candidate = QStringLiteral("modulate");
                canonicalType = QStringLiteral("Modulate");
            }
            if (candidate == QLatin1String("hall1"))
                candidate = QStringLiteral("hall 1");
            if (candidate == QLatin1String("hall2"))
                candidate = QStringLiteral("hall 2");
            if (candidate.isEmpty()
                || !QRegularExpression(QStringLiteral("(^| )%1($| )")
                       .arg(QRegularExpression::escape(candidate)))
                        .match(text).hasMatch())
                continue;
            QString family = explicitFamily;
            if (family.isEmpty())
                family = block.startsWith("preamp") ? QStringLiteral("preamp")
                                                     : block;
            QuestionEntity entity{QuestionEntityKind::EffectType, family,
                                  canonicalType, candidate, {}, {}, {}};
            entity.matchKind = QStringLiteral("catalog_type");
            appendUnique(&result, entity);
        }
    }

    QList<ParameterEntityCandidate> compatible;
    for (const ParameterEntityCandidate &candidate : matchedParameters) {
        bool shadowedByCompound = false;
        for (const ParameterEntityCandidate &other : matchedParameters) {
            if (other.id != candidate.id
                && other.matched.size() > candidate.matched.size()
                && phraseMatch(other.matched, candidate.matched)) {
                shadowedByCompound = true;
                break;
            }
        }
        if (shadowedByCompound)
            continue;
        if (!explicitFamily.isEmpty()
            && candidate.family != explicitFamily
            && candidate.block != explicitFamily)
            continue;
        compatible.append(candidate);
    }
    if (explicitFamily.isEmpty() && compatible.size() > 1) {
        QList<ParameterEntityCandidate> active;
        for (const ParameterEntityCandidate &candidate : compatible) {
            if (activeCandidate(context.patchContext, candidate))
                active.append(candidate);
        }
        if (!active.isEmpty())
            compatible = active;
        const QString userValue = explicitValue(text);
        bool userOk = false;
        const double userNumber = numericValue(userValue, &userOk);
        if (userOk && compatible.size() > 1) {
            QList<ParameterEntityCandidate> matchingValue;
            for (const ParameterEntityCandidate &candidate : compatible) {
                bool currentOk = false;
                const double currentNumber = numericValue(
                    contextParameterValue(context.patchContext, candidate),
                    &currentOk);
                if (currentOk && qAbs(currentNumber - userNumber) < 0.001)
                    matchingValue.append(candidate);
            }
            if (matchingValue.size() == 1)
                compatible = matchingValue;
        }
    }
    const QString userValue = explicitValue(text);
    for (const ParameterEntityCandidate &candidate : compatible) {
        QuestionEntity entity{QuestionEntityKind::Parameter,
                              candidate.block, candidate.label,
                              candidate.matched, candidate.id, userValue, {}};
        entity.matchKind = candidate.matched.contains(QLatin1Char(' '))
            ? QStringLiteral("compound_alias")
            : QStringLiteral("alias");
        appendUnique(&result, entity);
    }

    for (const ControllerCapability &controller : capabilities.controllers()) {
        const QString id = aliasNormalized(controller.id);
        const QString name = aliasNormalized(controller.name);
        if (text.contains(id) || text.contains(name))
            appendUnique(&result, {QuestionEntityKind::Controller, {},
                                  controller.name, id, controller.id, {}, {}});
    }
    if (currentPatchCue(text))
        appendUnique(&result, {QuestionEntityKind::PatchReference, {},
                              QStringLiteral("current_patch"),
                              QStringLiteral("current"), {}, {}, {}});
    return result;
}

QuestionRoutingResult QuestionIntentRouter::route(
    const QString &question, const QuestionRoutingContext &context) const
{
    QuestionRoutingResult result;
    const QString text = aliasNormalized(question);
    result.entities = resolveEntities(text, context);
    const QList<QuestionEntity> types = ofKind(result.entities,
                                                QuestionEntityKind::EffectType);
    const QList<QuestionEntity> parameters = ofKind(
        result.entities, QuestionEntityKind::Parameter);

    const auto set = [&result](QuestionIntent intent,
                               QuestionDestination destination,
                               const char *source,
                               const char *confidence = "high") {
        result.intent = intent;
        result.destination = destination;
        result.source = QString::fromLatin1(source);
        result.confidence = QString::fromLatin1(confidence);
    };

    if (Tone::ToneIntentParser().parse(question).kind == Tone::ToneRequestKind::BuildTone) {
        set(QuestionIntent::BuildTone, QuestionDestination::Tone,
            "tone_taxonomy+style_recipe");
        return result;
    }

    const bool assistantDiscovery = !any(text, {"gt 10", "gt10"})
        && any(text, {
            "o que voce pode fazer", "o que voce sabe fazer",
            "como voce pode me ajudar", "que tipo de pergunta posso fazer",
            "quais perguntas posso fazer", "o que eu posso perguntar",
            "quais coisas voce consegue responder",
            "me explique o que voce faz",
            "me ajuda a entender o que posso perguntar",
            "what can you do", "how can you help me", "what can i ask",
            "what kinds of questions can i ask",
            "what do you know how to do", "what can you help with"
        });
    if (assistantDiscovery) {
        set(QuestionIntent::CapabilityDiscovery,
            QuestionDestination::Knowledge,
            "assistant_capability_discovery");
        return result;
    }

    const bool assessmentCue = any(text, {
        "faz sentido", "esta bom", "esta legal", "e muito", "e pouco",
        "esta alto", "esta baixo", "esta exagerado", "esta fraco",
        "esta forte", "vale a pena deixar assim", "devo deixar assim",
        "does this make sense", "make sense", "too much", "too high",
        "too low", "setting okay", "should i leave it"
    });
    const bool hypotheticalCue = context.hasAssessment
        && any(text, {"e se eu", "se eu baixar", "se eu colocar",
                      "what if i", "if i lower", "if i set"})
        && !explicitValue(text).isEmpty();
    if (hypotheticalCue) {
        set(QuestionIntent::AssessCurrentSetting,
            QuestionDestination::Tone, "assessment_follow_up+hypothetical");
        return result;
    }
    if (assessmentCue && !parameters.isEmpty()) {
        if (parameters.size() > 1) {
            set(QuestionIntent::Clarify, QuestionDestination::Clarify,
                "assessment_parameter_ambiguity", "medium");
        } else {
            set(QuestionIntent::AssessCurrentSetting,
                QuestionDestination::Tone,
                "assessment_grammar+compound_parameter+patch_context");
        }
        return result;
    }

    // Equipment-wide list questions are structural capability queries. They
    // must be resolved before current-patch and individual type explanations.
    if (listCue(text)) {
        set(QuestionIntent::ListEffectTypes,
            QuestionDestination::Knowledge,
            "capability_list_grammar+catalog");
        return result;
    }

    // Explicit technical grammar outranks generic follow-up and keyword rules.
    if (comparisonCue(text)) {
        if (types.size() >= 2) {
            set(QuestionIntent::CompareEffectTypes,
                QuestionDestination::Knowledge, "comparison_grammar+catalog");
            return result;
        }
        if (parameters.size() >= 2) {
            const bool conversationalChoice = !any(text, {
                "diferenca", "compare", "comparar", "versus", " vs ",
                "difference between", "what changes between"});
            set(QuestionIntent::CompareParameters,
                conversationalChoice ? QuestionDestination::Tone
                                     : QuestionDestination::Knowledge,
                conversationalChoice
                    ? "comparison_choice+tone_context"
                    : "comparison_grammar+catalog");
            return result;
        }
        const bool ampDrive = any(text, {"amp", "preamp"})
            && (any(text, {"drive", "od ds", "overdrive", "blues od"})
                || (!types.isEmpty()
                    && types.first().family == QLatin1String("odDs")));
        if (ampDrive) {
            set(QuestionIntent::CompareRecommendation,
                QuestionDestination::Tone, "comparison_grammar+tone_context");
            return result;
        }
        if (!types.isEmpty()) {
            // Keep the recognized side and let the knowledge layer report the
            // unknown peer instead of inventing an algorithm.
            set(QuestionIntent::CompareEffectTypes,
                QuestionDestination::Knowledge, "comparison_grammar+partial_catalog",
                "medium");
            return result;
        }
        if (text == QLatin1String("qual a diferenca")
            || text == QLatin1String("what is the difference")) {
            if (!context.previousComparisonEntities.isEmpty()) {
                result.entities = context.previousComparisonEntities;
                const bool previousParameters = ofKind(
                    result.entities, QuestionEntityKind::Parameter).size() >= 2;
                set(previousParameters ? QuestionIntent::CompareParameters
                                       : QuestionIntent::CompareEffectTypes,
                    QuestionDestination::Knowledge,
                    "conversation_previous_comparison", "medium");
            } else if (context.hasRecommendation) {
                set(QuestionIntent::CompareRecommendation,
                    QuestionDestination::Tone,
                    "conversation_last_recommendation", "medium");
            } else {
                set(QuestionIntent::Clarify, QuestionDestination::Clarify,
                    "ambiguous_comparison", "low");
            }
            return result;
        }
    }
    if (explainCue(text) && !types.isEmpty()) {
        set(QuestionIntent::ExplainEffectType,
            QuestionDestination::Knowledge, "explanation_grammar+catalog");
        return result;
    }
    if (explainCue(text) && !parameters.isEmpty()) {
        set(QuestionIntent::ExplainParameter,
            QuestionDestination::Knowledge, "explanation_grammar+catalog");
        return result;
    }
    if (!parameters.isEmpty()
        && any(text, {"quero reduzir", "quero baixar", "quero aumentar",
                      "reduza", "aumente", "lower ", "reduce ",
                      "increase "})) {
        set(QuestionIntent::AdjustTone, QuestionDestination::Tone,
            "parameter_adjustment+catalog");
        return result;
    }
    if (any(text, {"como configurar", "como usar", "como controlar",
                   "how to", "how do i", "how can i"})) {
        set(QuestionIntent::HowTo, QuestionDestination::Knowledge,
            "how_to_grammar");
        return result;
    }

    const Tone::ToneRequest tone = Tone::ToneIntentParser().parse(question);
    if (tone.kind == Tone::ToneRequestKind::ExplainTone) {
        set(QuestionIntent::ExplainTone, QuestionDestination::Tone,
            "tone_intent_parser");
        return result;
    }
    if (currentPatchCue(text)) {
        set(QuestionIntent::CurrentPatchFact,
            QuestionDestination::PatchContext, "current_patch_grammar");
        return result;
    }
    if (justifyCue(text)) {
        set(QuestionIntent::Justify, QuestionDestination::Tone,
            context.hasRecommendation ? "follow_up+recommendation"
                                      : "follow_up_without_recommendation",
            context.hasRecommendation ? "high" : "medium");
        return result;
    }
    if (detailCue(text)) {
        set(QuestionIntent::DetailFollowUp, QuestionDestination::Tone,
            "follow_up_detail", "medium");
        return result;
    }
    if (feedbackCue(text)) {
        set(QuestionIntent::Feedback, QuestionDestination::Tone,
            "follow_up_feedback", "medium");
        return result;
    }
    if (refineCue(text)) {
        set(QuestionIntent::Refine, QuestionDestination::Tone,
            "follow_up_constraint", "medium");
        return result;
    }
    if (tone.kind == Tone::ToneRequestKind::AdjustTone
        || tone.kind == Tone::ToneRequestKind::ArtistProfileUnavailable
        || tone.kind == Tone::ToneRequestKind::UnsupportedCapability) {
        set(QuestionIntent::AdjustTone, QuestionDestination::Tone,
            "tone_intent_parser");
        return result;
    }
    if (any(text, {"ajuda", "help", "o que voce sabe fazer",
                   "como voce pode me ajudar", "what can you do",
                   "how can you help"})) {
        set(QuestionIntent::Help, QuestionDestination::Knowledge,
            "help_grammar");
        return result;
    }
    if (any(text, {"possui", "tem ", "does it have", "does the gt 10",
                   "o que a gt 10 pode fazer", "what can the gt 10 do",
                   "quais efeitos", "quais reverbs", "what effects",
                   "what reverbs", "quantos assigns"})) {
        set(QuestionIntent::Capability, QuestionDestination::Knowledge,
            "capability_grammar");
        return result;
    }
    set(QuestionIntent::Unknown, QuestionDestination::Unknown,
        "no_deterministic_match", "low");
    return result;
}

QString QuestionIntentRouter::intentName(QuestionIntent intent)
{
    switch (intent) {
    case QuestionIntent::CurrentPatchFact: return "CURRENT_PATCH_FACT";
    case QuestionIntent::AssessCurrentSetting: return "ASSESS_CURRENT_SETTING";
    case QuestionIntent::CompareEffectTypes: return "COMPARE_EFFECT_TYPES";
    case QuestionIntent::CompareParameters: return "COMPARE_PARAMETERS";
    case QuestionIntent::ExplainEffectType: return "EXPLAIN_EFFECT_TYPE";
    case QuestionIntent::ExplainParameter: return "EXPLAIN_PARAMETER";
    case QuestionIntent::ListEffectTypes: return "LIST_EFFECT_TYPES";
    case QuestionIntent::HowTo: return "HOW_TO";
    case QuestionIntent::CapabilityDiscovery: return "CAPABILITY_DISCOVERY";
    case QuestionIntent::Capability: return "CAPABILITY";
    case QuestionIntent::ExplainTone: return "EXPLAIN_TONE";
    case QuestionIntent::AdjustTone: return "ADJUST_TONE";
    case QuestionIntent::BuildTone: return "BUILD_TONE";
    case QuestionIntent::Justify: return "JUSTIFY";
    case QuestionIntent::Refine: return "REFINE";
    case QuestionIntent::CompareRecommendation: return "COMPARE_RECOMMENDATION";
    case QuestionIntent::DetailFollowUp: return "DETAIL_FOLLOWUP";
    case QuestionIntent::Feedback: return "FEEDBACK";
    case QuestionIntent::Help: return "HELP";
    case QuestionIntent::Clarify: return "CLARIFY";
    case QuestionIntent::Unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

QString QuestionIntentRouter::destinationName(QuestionDestination destination)
{
    switch (destination) {
    case QuestionDestination::Knowledge: return "KNOWLEDGE";
    case QuestionDestination::PatchContext: return "PATCH_CONTEXT";
    case QuestionDestination::Tone: return "TONE_CONVERSATION";
    case QuestionDestination::Clarify: return "CLARIFY";
    case QuestionDestination::Unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

QString QuestionIntentRouter::entityKindName(QuestionEntityKind kind)
{
    switch (kind) {
    case QuestionEntityKind::EffectFamily: return "effect_family";
    case QuestionEntityKind::EffectType: return "effect_type";
    case QuestionEntityKind::Parameter: return "parameter";
    case QuestionEntityKind::Controller: return "controller";
    case QuestionEntityKind::PatchReference: return "patch_reference";
    case QuestionEntityKind::Unresolved: return "unresolved";
    }
    return "unresolved";
}

} // namespace Knowledge
} // namespace Ai
