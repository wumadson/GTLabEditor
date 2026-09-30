#include "ai/LocalGT10Provider.h"

#include <QJsonArray>

namespace {

QString toneSourceName(Ai::Tone::ToneSourceKind source)
{
    using Ai::Tone::ToneSourceKind;
    switch (source) {
    case ToneSourceKind::PatchContext: return QStringLiteral("PatchContext");
    case ToneSourceKind::ToneRule: return QStringLiteral("ToneRule");
    case ToneSourceKind::OfficialDocumentation:
        return QStringLiteral("OfficialDocumentation");
    case ToneSourceKind::Guidance: return QStringLiteral("Guidance");
    }
    return QStringLiteral("Unknown");
}

QJsonObject evidenceObject(const Ai::Tone::ToneEvidence &evidence)
{
    QJsonObject object;
    object.insert(QStringLiteral("blockId"), evidence.blockId);
    object.insert(QStringLiteral("parameterId"), evidence.parameterId);
    object.insert(QStringLiteral("value"), evidence.value);
    object.insert(QStringLiteral("source"), toneSourceName(evidence.sourceKind));
    return object;
}

QString responseModeName(Ai::Tone::ToneResponseMode mode)
{
    using Ai::Tone::ToneResponseMode;
    switch (mode) {
    case ToneResponseMode::Explain: return QStringLiteral("EXPLAIN");
    case ToneResponseMode::Recommend: return QStringLiteral("RECOMMEND");
    case ToneResponseMode::Justify: return QStringLiteral("JUSTIFY");
    case ToneResponseMode::Refine: return QStringLiteral("REFINE");
    case ToneResponseMode::Compare: return QStringLiteral("COMPARE");
    case ToneResponseMode::Assess: return QStringLiteral("ASSESS");
    case ToneResponseMode::HypotheticalCompare:
        return QStringLiteral("HYPOTHETICAL_COMPARE");
    case ToneResponseMode::Clarify: return QStringLiteral("CLARIFY");
    case ToneResponseMode::Build: return QStringLiteral("BUILD_TONE");
    case ToneResponseMode::Unknown: return QStringLiteral("UNKNOWN");
    }
    return QStringLiteral("UNKNOWN");
}

} // namespace

namespace Ai {

LocalGT10Provider::LocalGT10Provider(QObject *parent)
    : AIProvider(parent)
{
    responseTimer.setSingleShot(true);
    responseTimer.setInterval(120);
    connect(&responseTimer, &QTimer::timeout,
            this, &LocalGT10Provider::finishRequest);
}

QString LocalGT10Provider::providerId() const
{
    return QStringLiteral("local-gt10");
}

QString LocalGT10Provider::providerName() const
{
    return tr("Local GT-10");
}

AIProviderCapabilities LocalGT10Provider::capabilities() const
{
    AIProviderCapabilities result;
    result.supportsConversation = true;
    return result;
}

void LocalGT10Provider::sendRequest(const QString &requestId,
                                    const AIRequest &request)
{
    if (!activeRequestId.isEmpty()) {
        emit errorOccurred(requestId,
                           tr("Local GT-10 is already handling a request."));
        return;
    }
    activeRequestId = requestId;
    activeRequest = request;
    responseTimer.start();
}

void LocalGT10Provider::cancelRequest(const QString &requestId)
{
    if (requestId != activeRequestId)
        return;
    responseTimer.stop();
    activeRequestId.clear();
    activeRequest = AIRequest();
    emit requestCancelled(requestId);
}

void LocalGT10Provider::resetConversation()
{
    toneConversationReasoner.reset();
    parameterAssessment.reset();
    lastResponseWasAssessment = false;
    lastComparisonEntities.clear();
}

void LocalGT10Provider::finishRequest()
{
    if (activeRequestId.isEmpty())
        return;
    const QString requestId = activeRequestId;
    const AIRequest request = activeRequest;
    activeRequestId.clear();
    activeRequest = AIRequest();

    AIResponse response;
    response.metadata.insert("provider", providerId());
    Knowledge::QuestionRoutingContext routingContext;
    routingContext.hasRecommendation =
        toneConversationReasoner.state().lastRecommendation.valid;
    routingContext.hasToneAnalysis =
        !toneConversationReasoner.state().lastToneAnalysis.features.isEmpty();
    routingContext.hasAssessment = parameterAssessment.hasAssessment();
    routingContext.patchContext = &request.context;
    routingContext.previousComparisonEntities = lastComparisonEntities;
    const Knowledge::QuestionRoutingResult routing = questionRouter.route(
        request.userMessage, routingContext);
    response.metadata.insert("detectedIntent",
        Knowledge::QuestionIntentRouter::intentName(routing.intent));
    response.metadata.insert("intentConfidence", routing.confidence);
    response.metadata.insert("intentSource", routing.source);
    response.metadata.insert("destination",
        Knowledge::QuestionIntentRouter::destinationName(routing.destination));
    response.metadata.insert("resolvedEntities", routing.entitiesJson());
    for (const Knowledge::QuestionEntity &entity : routing.entities) {
        if (entity.kind == Knowledge::QuestionEntityKind::Parameter
            && !response.metadata.contains("resolvedParameter")) {
            response.metadata.insert("resolvedFamily", entity.family);
            response.metadata.insert("resolvedParameter", entity.id);
            response.metadata.insert("matchedAlias", entity.matchedText);
            response.metadata.insert("matchKind", entity.matchKind);
        } else if (entity.kind == Knowledge::QuestionEntityKind::EffectType
                   && !response.metadata.contains("resolvedEffectType")) {
            response.metadata.insert("resolvedFamily", entity.family);
            response.metadata.insert("resolvedEffectType",
                                     entity.canonicalName);
            response.metadata.insert("matchedAlias", entity.matchedText);
            response.metadata.insert("matchKind", entity.matchKind);
        } else if (entity.kind == Knowledge::QuestionEntityKind::EffectFamily
                   && !response.metadata.contains("resolvedFamily")) {
            response.metadata.insert("resolvedFamily", entity.family);
        }
    }

    if (routing.intent == Knowledge::QuestionIntent::CapabilityDiscovery) {
        const bool hasPatchContext = !request.context.root
            .value(QStringLiteral("patch")).toObject().isEmpty();
        response.text = featureRegistry.capabilityDiscoveryResponse(
            request.language, hasPatchContext);
        response.metadata.insert("answered", true);
        response.metadata.insert("topicId",
                                 QStringLiteral("assistant_capabilities"));
        response.metadata.insert("route",
                                 QStringLiteral("capability_discovery"));
        response.metadata.insert("enabledFeatureCount",
                                 featureRegistry.enabledFeatures().size());
        response.metadata.insert("limitationCount",
                                 featureRegistry.limitations(request.language)
                                     .size());
        emit responseReady(requestId, response);
        return;
    }

    if (routing.intent == Knowledge::QuestionIntent::Clarify) {
        const bool parameterAmbiguity = routing.source
            == QLatin1String("assessment_parameter_ambiguity");
        response.text = request.language.startsWith("pt", Qt::CaseInsensitive)
            ? (parameterAmbiguity
                ? QStringLiteral("Você está falando desse parâmetro em qual bloco ativo? Por exemplo, no Reverb ou no Chorus?")
                : QStringLiteral("Entre quais opções você quer comparar?"))
            : (parameterAmbiguity
                ? QStringLiteral("Which active block do you mean for that parameter—for example, Reverb or Chorus?")
                : QStringLiteral("Which options would you like to compare?"));
        response.metadata.insert("answered", false);
        response.metadata.insert("topicId", QStringLiteral("clarify_comparison"));
        response.metadata.insert("route", QStringLiteral("clarify"));
        emit responseReady(requestId, response);
        return;
    }

    Tone::ToneConversationReply toneReply;
    if (routing.intent == Knowledge::QuestionIntent::AssessCurrentSetting) {
        toneReply = parameterAssessment.respond(request, routing);
        lastResponseWasAssessment = toneReply.handled;
    }
    else if (routing.intent == Knowledge::QuestionIntent::Justify
             && lastResponseWasAssessment
             && parameterAssessment.hasAssessment())
        toneReply = parameterAssessment.justify(request);
    else if (routing.destination == Knowledge::QuestionDestination::Tone) {
        lastResponseWasAssessment = false;
        toneReply = toneConversationReasoner.respond(request);
    } else {
        lastResponseWasAssessment = false;
    }
    if (toneReply.handled) {
        for (auto it = toneReply.metadata.constBegin(); it != toneReply.metadata.constEnd(); ++it)
            response.metadata.insert(it.key(), it.value());
        response.text = toneReply.text;
        response.metadata.insert("answered", toneReply.answered);
        response.metadata.insert("topicId", toneReply.topicId);
        response.metadata.insert("route", toneReply.route);
        response.metadata.insert("responseMode",
                                 responseModeName(toneReply.mode));
        QJsonArray features;
        for (const Tone::ToneFeature &feature : toneReply.analysis.features) {
            QJsonObject item;
            item.insert("id", feature.id);
            item.insert("strength", feature.strength);
            QJsonArray sources;
            for (Tone::ToneSourceKind source : feature.sources)
                sources.append(toneSourceName(source));
            item.insert("sources", sources);
            QJsonArray evidence;
            for (const Tone::ToneEvidence &entry : feature.evidence)
                evidence.append(evidenceObject(entry));
            item.insert("evidence", evidence);
            features.append(item);
        }
        if (!features.isEmpty())
            response.metadata.insert("toneFeatures", features);
        QJsonArray suggestions;
        const QList<Tone::ToneAdjustmentSuggestion> replySuggestions =
            !toneReply.recommendation.suggestions.isEmpty()
                ? toneReply.recommendation.suggestions
                : toneReply.advice.suggestions;
        for (const Tone::ToneAdjustmentSuggestion &suggestion
             : replySuggestions) {
            QJsonObject item;
            item.insert("blockId", suggestion.blockId);
            item.insert("parameterId", suggestion.parameterId);
            item.insert("currentValue", suggestion.currentValue);
            item.insert("direction", suggestion.suggestedDirection);
            item.insert("targetRange", suggestion.suggestedTargetRange);
            item.insert("optionalActivation", suggestion.optionalActivation);
            QJsonArray sources;
            for (Tone::ToneSourceKind source : suggestion.sources)
                sources.append(toneSourceName(source));
            item.insert("sources", sources);
            suggestions.append(item);
        }
        if (!suggestions.isEmpty())
            response.metadata.insert("toneSuggestions", suggestions);
        QJsonArray constraints;
        for (Tone::ToneConstraint constraint
             : toneReply.recommendation.constraints) {
            constraints.append(static_cast<int>(constraint));
        }
        if (!constraints.isEmpty())
            response.metadata.insert("toneConstraints", constraints);
        if (parameterAssessment.hasAssessment()
            && (toneReply.mode == Tone::ToneResponseMode::Assess
                || toneReply.mode
                   == Tone::ToneResponseMode::HypotheticalCompare)) {
            const Tone::ParameterAssessment &assessment =
                parameterAssessment.lastAssessment();
            QJsonObject trace;
            trace.insert("resolvedBlock", assessment.blockId);
            trace.insert("resolvedParameter", assessment.parameterId);
            trace.insert("currentValue", assessment.currentValue);
            trace.insert("userValue", assessment.userReferencedValue);
            trace.insert("range", assessment.range);
            trace.insert("assessment", assessment.assessment);
            trace.insert("confidence",
                         static_cast<int>(assessment.confidence));
            trace.insert("evidence",
                         QJsonArray::fromStringList(
                             assessment.supportingEvidence));
            trace.insert("tradeoffs",
                         QJsonArray::fromStringList(assessment.tradeoffs));
            response.metadata.insert("parameterAssessment", trace);
        }
    } else {
        const Knowledge::LocalKnowledgeAnswer answer = engine.answerRouted(
            request.userMessage, request.language, routing, &request.context);
        response.text = answer.text;
        response.metadata.insert("answered", answer.answered);
        response.metadata.insert("topicId", answer.topicId);
        response.metadata.insert("confidence",
                                 static_cast<int>(answer.confidence));
        response.metadata.insert("sourceKind",
                                 static_cast<int>(answer.sourceKind));
        response.metadata.insert("route", QStringLiteral("knowledge"));
        for (const QString &key : {QStringLiteral("listCount"),
                                   QStringLiteral("destinationService"),
                                   QStringLiteral("sourceKindName")}) {
            if (answer.metadata.contains(key))
                response.metadata.insert(key, answer.metadata.value(key));
        }
        if (answer.metadata.contains("documentationTrace"))
            response.metadata.insert("documentationTrace",
                                     answer.metadata.value("documentationTrace"));
        if (answer.metadata.contains("documentationConflict"))
            response.metadata.insert("documentationConflict",
                                     answer.metadata.value("documentationConflict"));
        if (answer.metadata.contains("documentationMatches"))
            response.metadata.insert("documentationMatches",
                                     answer.metadata.value("documentationMatches"));
    }
    if (routing.isComparison()
        && routing.intent != Knowledge::QuestionIntent::CompareRecommendation
        && routing.entities.size() >= 2)
        lastComparisonEntities = routing.entities;
    emit responseReady(requestId, response);
}

} // namespace Ai
