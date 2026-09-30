#include "ai/tone/ToneConversationReasoner.h"

#include "ai/tone/ToneRules.h"
#include "ai/tone/ToneStyleRecipes.h"

#include <QJsonObject>

namespace Ai {
namespace Tone {
namespace {

bool portuguese(const QString &language)
{
    return language.startsWith(QStringLiteral("pt"), Qt::CaseInsensitive);
}

QString modeRoute(ToneResponseMode mode)
{
    switch (mode) {
    case ToneResponseMode::Explain: return QStringLiteral("tone_explain");
    case ToneResponseMode::Recommend: return QStringLiteral("tone_adjustment");
    case ToneResponseMode::Justify: return QStringLiteral("tone_justify");
    case ToneResponseMode::Refine: return QStringLiteral("tone_refine");
    case ToneResponseMode::Compare: return QStringLiteral("tone_compare");
    case ToneResponseMode::Assess: return QStringLiteral("tone_assess");
    case ToneResponseMode::HypotheticalCompare:
        return QStringLiteral("tone_hypothetical_compare");
    case ToneResponseMode::Clarify: return QStringLiteral("tone_clarify");
    case ToneResponseMode::Build: return QStringLiteral("tone_build");
    case ToneResponseMode::Unknown: break;
    }
    return QStringLiteral("knowledge");
}

} // namespace

QString ToneConversationReasoner::patchIdentity(const AIContext &context)
{
    const QJsonObject patch = context.root.value("patch").toObject();
    const QString number = patch.value("number").toString();
    const QString name = patch.value("name").toString();
    return number.isEmpty() && name.isEmpty()
        ? QStringLiteral("context-without-patch-identity")
        : number + QLatin1Char('|') + name;
}

QList<ToneConstraint> ToneConversationReasoner::constraints(
    const QString &text)
{
    QList<ToneConstraint> result;
    const auto add = [&result](ToneConstraint value) {
        if (!result.contains(value))
            result.append(value);
    };
    if (text.contains("preservar o clean") || text.contains("preserve clean"))
        add(ToneConstraint::PreserveClean);
    if (text.contains("preservar a dinamica") || text.contains("preserve dynamics"))
        add(ToneConstraint::PreserveDynamics);
    if (text.contains("sem perder definicao")
        || text.contains("nao quero perder definicao")
        || text.contains("preservar definicao")
        || text.contains("keep definition") || text.contains("preserve definition"))
        add(ToneConstraint::PreserveDefinition);
    if (text.contains("preservar brilho") || text.contains("preserve brightness"))
        add(ToneConstraint::PreserveBrightness);
    if (text.contains("preservar os graves") || text.contains("preserve low end"))
        add(ToneConstraint::PreserveLowEnd);
    if (text.contains("preservar a ambiencia") || text.contains("preserve ambience"))
        add(ToneConstraint::PreserveAmbience);
    if (text.contains("nao quero usar drive") || text.contains("nao usar od")
        || text.contains("sem drive") || text.contains("avoid drive")
        || text.contains("avoid od"))
        add(ToneConstraint::AvoidOdDs);
    if (text.contains("nao quero mexer no reverb") || text.contains("avoid reverb"))
        add(ToneConstraint::AvoidReverb);
    if (text.contains("manter o delay") || text.contains("nao quero mexer no delay")
        || text.contains("keep delay") || text.contains("avoid delay"))
        add(ToneConstraint::AvoidDelay);
    if (text.contains("nao quero mexer no amp") || text.contains("avoid amp"))
        add(ToneConstraint::AvoidAmp);
    if (text.contains("mas pouco") || text.contains("bem pouco")
        || text.contains("small change") || text.contains("subtle"))
        add(ToneConstraint::SmallChangeOnly);
    return result;
}

bool ToneConversationReasoner::isJustify(const QString &text)
{
    return text == "por que" || text == "porque" || text == "why"
        || text.contains("por que voce sugeriu")
        || text.contains("por que voce esta sugerindo")
        || text.contains("por que esta sugerindo")
        || text.contains("por que mexer") || text.contains("por que primeiro")
        || text.contains("por que nao mexer")
        || text.contains("why did you suggest")
        || text.contains("why that change");
}

bool ToneConversationReasoner::isDetail(const QString &text)
{
    return text.contains("explique melhor") || text.contains("detalhe")
        || text.contains("me explique mais")
        || text.contains("quero entender melhor")
        || text.contains("explain further") || text.contains("more detail");
}

bool ToneConversationReasoner::isCompare(const QString &text)
{
    const bool comparisonCue = text.contains("melhor mexer")
        || text.contains("devo mexer") || text.contains(" ou ")
        || text.contains("better to") || text.contains("which is better")
        || text.contains("versus") || text == QLatin1String("qual a diferenca")
        || text == QLatin1String("what is the difference");
    if (!comparisonCue)
        return false;
    const bool ampDrive = (text.contains("amp") || text.contains("preamp"))
        && (text.contains("drive") || text.contains("od ds")
            || text.contains("blues od"));
    const bool reverbControls = text.contains("reverb time")
        && text.contains("effect level");
    const bool timeEffects = text.contains("delay")
        && text.contains("reverb");
    const bool ampHighs = text.contains("treble")
        && text.contains("presence");
    return ampDrive || reverbControls || timeEffects || ampHighs
        || text == QLatin1String("qual a diferenca")
        || text == QLatin1String("what is the difference");
}

bool ToneConversationReasoner::isFeedback(const QString &text)
{
    return text.contains("mais um pouco") || text.contains("ficou pouco")
        || text.contains("ficou demais") || text.contains("voltar um pouco")
        || text == "gostei" || text == "nao gostei"
        || text.contains("a little more") || text.contains("too much")
        || text.contains("not enough") || text.contains("back off")
        || text == "i like it" || text == "i do not like it";
}

bool ToneConversationReasoner::isAmbiguous(const QString &text)
{
    return text == "quero mais" || text == "mais" || text == "i want more";
}

void ToneConversationReasoner::mergeConstraints(
    QList<ToneConstraint> *target, const QList<ToneConstraint> &incoming)
{
    for (ToneConstraint constraint : incoming) {
        if (!target->contains(constraint))
            target->append(constraint);
    }
}

ToneRecommendationRecord ToneConversationReasoner::recommendationRecord(
    const ToneAdvice &advice, const ToneAnalysis &analysis,
    const QString &language) const
{
    ToneRecommendationRecord record;
    record.intent = advice.intent;
    record.suggestions = advice.suggestions;
    record.constraints = conversationState.activeConstraints;
    record.valid = !record.suggestions.isEmpty();
    if (!record.valid)
        return record;
    for (const ToneAdjustmentSuggestion &suggestion : record.suggestions) {
        if (!record.affectedBlocks.contains(suggestion.blockId))
            record.affectedBlocks.append(suggestion.blockId);
        ToneEvidence evidence;
        evidence.blockId = suggestion.blockId;
        evidence.parameterId = suggestion.parameterId;
        evidence.value = suggestion.currentValue;
        evidence.sourceKind = ToneSourceKind::PatchContext;
        record.evidence.append(evidence);
    }
    for (const ToneEvidence &evidence : analysis.evidence) {
        if (record.evidence.size() >= 6)
            break;
        if (record.affectedBlocks.contains(evidence.blockId))
            record.evidence.append(evidence);
    }
    record.rationale = record.suggestions.first().reason;
    record.tradeOff = portuguese(language)
        ? QStringLiteral("ganhar a característica pedida sem descaracterizar os blocos que já estão ativos.")
        : QStringLiteral("gaining the requested trait without unnecessarily changing the blocks that are already active.");
    if (record.constraints.contains(ToneConstraint::PreserveClean))
        record.tradeOff = portuguese(language)
            ? QStringLiteral("adicionar a característica pedida preservando o máximo possível da resposta clean.")
            : QStringLiteral("adding the requested trait while preserving as much clean response as possible.");
    if (record.constraints.contains(ToneConstraint::PreserveDefinition))
        record.tradeOff = portuguese(language)
            ? QStringLiteral("aumentar o efeito desejado sem encobrir ataque e definição.")
            : QStringLiteral("increasing the desired effect without masking attack and definition.");
    return record;
}

ToneConversationReply ToneConversationReasoner::recommendation(
    const AIRequest &request, const ToneAdjustmentIntent &intent,
    ToneResponseMode mode)
{
    ToneConversationReply reply;
    reply.handled = true;
    reply.mode = mode;
    reply.route = modeRoute(mode);
    reply.topicId = mode == ToneResponseMode::Refine
        ? QStringLiteral("refine_tone") : QStringLiteral("adjust_tone");
    const ToneAnalysis analysis = analyzer.analyze(request.context,
                                                   request.language);
    reply.advice = advisor.advise(request.context, intent, request.language,
                                  conversationState.activeConstraints);
    reply.recommendation = recommendationRecord(reply.advice, analysis,
                                                request.language);
    reply.answered = reply.recommendation.valid;
    if (reply.answered) {
        reply.text = composer.recommend(reply.recommendation, request.context,
                                        request.language,
                                        mode == ToneResponseMode::Refine);
        conversationState.lastToneAnalysis = analysis;
        conversationState.lastToneIntent = intent;
        conversationState.lastRecommendation = reply.recommendation;
        conversationState.lastResponse = reply.text;
        conversationState.lastMode = mode;
    } else {
        reply.text = reply.advice.response;
    }
    return reply;
}

ToneConversationReply ToneConversationReasoner::respond(
    const AIRequest &request)
{
    ToneConversationReply reply;
    const QString identity = patchIdentity(request.context);
    const bool patchChanged = !conversationState.currentPatchIdentity.isEmpty()
        && conversationState.currentPatchIdentity != identity;
    if (patchChanged) {
        lastBuildQuestion.clear();
        conversationState.lastToneAnalysis = ToneAnalysis();
        conversationState.lastToneIntent = ToneAdjustmentIntent();
        conversationState.lastRecommendation = ToneRecommendationRecord();
        conversationState.activeConstraints.clear();
        conversationState.lastResponse.clear();
        conversationState.lastMode = ToneResponseMode::Unknown;
    }
    conversationState.currentPatchIdentity = identity;

    const QString normalized = ToneIntentParser::normalize(request.userMessage);
    const ToneRequest parsed = parser.parse(request.userMessage);
    const QList<ToneConstraint> newConstraints = constraints(normalized);

    if (parsed.kind == ToneRequestKind::ArtistProfileUnavailable) {
        reply.handled = true;
        reply.mode = ToneResponseMode::Unknown;
        reply.route = QStringLiteral("tone_limitation");
        reply.topicId = QStringLiteral("artist_profile");
        reply.text = portuguese(request.language)
            ? QStringLiteral("Os perfis de artistas ainda não fazem parte da base local. Se quiser, posso trabalhar por características como clean mais dinâmico, mais crunch, mais ambiente ou mais definido.")
            : QStringLiteral("Artist profiles are not part of the local knowledge base yet. I can instead work by traits such as a more dynamic clean tone, more crunch, ambience, or definition.");
        return reply;
    }
    if (parsed.kind == ToneRequestKind::UnsupportedCapability) {
        reply.handled = true;
        reply.mode = ToneResponseMode::Unknown;
        reply.route = QStringLiteral("tone_limitation");
        reply.topicId = QStringLiteral("unsupported_capability");
        reply.text = portuguese(request.language)
            ? QStringLiteral("Esse recurso não faz parte das capacidades conhecidas da GT-10. Não vou inventar um parâmetro ou algoritmo; posso orientar o patch usando os recursos reais disponíveis.")
            : QStringLiteral("That feature is not part of the known GT-10 capabilities. I will not invent a parameter or algorithm, but I can guide the patch using its real available features.");
        return reply;
    }
    const bool buildDetail = isDetail(normalized) || normalized.contains("valores") || normalized.contains("values")
        || normalized.contains("passo a passo") || normalized.contains("step by step");
    const bool buildCompact = normalized.contains("configuracao resumida") || normalized == "keep it brief";
    const bool buildFollowup = parsed.kind == ToneRequestKind::None
        && conversationState.lastMode == ToneResponseMode::Build && !lastBuildQuestion.isEmpty()
        && (buildDetail || buildCompact);
    if (parsed.kind == ToneRequestKind::BuildTone || buildFollowup) {
        AIRequest buildRequest = request;
        if (buildFollowup) {
            QString goal = ToneIntentParser::normalize(lastBuildQuestion);
            if (buildDetail) {
                for (const QString &cue : {QString("configuracao resumida"), QString("resumido"), QString("resumida"), QString("keep it brief")})
                    goal.replace(cue, QString());
            }
            buildRequest.userMessage = goal + (buildCompact ? " configuracao resumida" : " parameters")
                + ((normalized.contains("valores") || normalized.contains("values")) ? " values" : "");
        }
        else lastBuildQuestion = request.userMessage;
        reply = ToneStyleRecipes().respond(buildRequest);
        reply.metadata.insert("buildFollowup", buildFollowup);
        // A new style proposal must not leave an old adjustment available
        // for a subsequent justification or feedback turn.
        conversationState.lastRecommendation = ToneRecommendationRecord();
        conversationState.lastToneIntent = ToneAdjustmentIntent();
        conversationState.activeConstraints.clear();
        conversationState.lastToneAnalysis = reply.analysis;
        conversationState.lastResponse = reply.text;
        conversationState.lastMode = reply.mode;
        return reply;
    }
    if (isJustify(normalized) || isDetail(normalized)) {
        reply.handled = true;
        if (isDetail(normalized)
            && conversationState.lastMode == ToneResponseMode::Explain
            && !conversationState.lastToneAnalysis.features.isEmpty()) {
            reply.mode = ToneResponseMode::Explain;
            reply.route = modeRoute(reply.mode);
            reply.topicId = QStringLiteral("explain_tone_detailed");
            reply.answered = true;
            reply.analysis = conversationState.lastToneAnalysis;
            reply.text = composer.explain(conversationState.lastToneAnalysis,
                                          request.context,
                                          request.language, true);
            conversationState.lastResponse = reply.text;
            conversationState.lastMode = reply.mode;
            return reply;
        }
        if (!conversationState.lastRecommendation.valid) {
            reply.mode = ToneResponseMode::Clarify;
            reply.route = modeRoute(reply.mode);
            reply.topicId = QStringLiteral("justify_without_recommendation");
            reply.text = patchChanged
                ? composer.clarify(request.language, true)
                : (portuguese(request.language)
                    ? QStringLiteral("Não tenho uma recomendação anterior ativa para justificar. Se quiser, diga qual ajuste você quer analisar.")
                    : QStringLiteral("I don't have an active previous recommendation to justify. Tell me which adjustment you want to analyze."));
            return reply;
        }
        reply.mode = ToneResponseMode::Justify;
        reply.route = modeRoute(reply.mode);
        reply.topicId = QStringLiteral("justify_tone");
        reply.answered = true;
        reply.recommendation = conversationState.lastRecommendation;
        reply.text = composer.justify(conversationState.lastRecommendation,
                                      request.language, true);
        conversationState.lastResponse = reply.text;
        conversationState.lastMode = reply.mode;
        return reply;
    }
    if (isCompare(normalized)) {
        reply.handled = true;
        reply.answered = true;
        reply.mode = ToneResponseMode::Compare;
        reply.route = modeRoute(reply.mode);
        reply.topicId = QStringLiteral("compare_tone_options");
        reply.recommendation = conversationState.lastRecommendation;
        reply.text = composer.compare(request.context, normalized,
                                      conversationState.lastRecommendation,
                                      request.language);
        conversationState.lastResponse = reply.text;
        conversationState.lastMode = reply.mode;
        return reply;
    }
    if (isFeedback(normalized)) {
        reply.handled = true;
        if (!conversationState.lastRecommendation.valid) {
            reply.mode = ToneResponseMode::Clarify;
            reply.route = modeRoute(reply.mode);
            reply.topicId = QStringLiteral("tone_feedback_without_context");
            reply.text = composer.clarify(request.language, patchChanged);
            return reply;
        }
        reply.answered = true;
        reply.mode = ToneResponseMode::Refine;
        reply.route = modeRoute(reply.mode);
        reply.topicId = QStringLiteral("tone_feedback");
        reply.recommendation = conversationState.lastRecommendation;
        reply.text = composer.feedback(conversationState.lastRecommendation,
                                       normalized, request.language);
        conversationState.lastResponse = reply.text;
        conversationState.lastMode = reply.mode;
        return reply;
    }

    if (!newConstraints.isEmpty()) {
        mergeConstraints(&conversationState.activeConstraints, newConstraints);
        ToneAdjustmentIntent intent = parsed.adjustment;
        if (!intent.isValid())
            intent = conversationState.lastToneIntent;
        if (intent.isValid())
            return recommendation(request, intent, ToneResponseMode::Refine);
        reply.handled = true;
        reply.mode = ToneResponseMode::Clarify;
        reply.route = modeRoute(reply.mode);
        reply.topicId = QStringLiteral("constraint_without_intent");
        reply.text = composer.clarify(request.language);
        return reply;
    }
    if (parsed.kind == ToneRequestKind::ExplainTone) {
        reply.handled = true;
        reply.answered = true;
        reply.mode = ToneResponseMode::Explain;
        reply.route = modeRoute(reply.mode);
        reply.topicId = QStringLiteral("explain_tone");
        reply.analysis = analyzer.analyze(request.context, request.language);
        reply.answered = !reply.analysis.features.isEmpty();
        reply.text = composer.explain(reply.analysis, request.context,
                                      request.language, false);
        conversationState.lastToneAnalysis = reply.analysis;
        conversationState.lastResponse = reply.text;
        conversationState.lastMode = reply.mode;
        return reply;
    }
    if (parsed.kind == ToneRequestKind::AdjustTone)
        return recommendation(request, parsed.adjustment,
                              ToneResponseMode::Recommend);
    if (isAmbiguous(normalized)) {
        reply.handled = true;
        reply.mode = ToneResponseMode::Clarify;
        reply.route = modeRoute(reply.mode);
        reply.topicId = QStringLiteral("clarify_tone_direction");
        reply.text = composer.clarify(request.language);
    }
    return reply;
}

void ToneConversationReasoner::reset()
{
    lastBuildQuestion.clear();
    conversationState = ToneConversationState();
}

const ToneConversationState &ToneConversationReasoner::state() const
{
    return conversationState;
}

} // namespace Tone
} // namespace Ai
