#include "ai/knowledge/LocalKnowledgeEngine.h"

#include <QJsonArray>
#include <QJsonObject>

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

QJsonObject effect(const QJsonObject &root, const QString &id)
{
    for (const QJsonValue &value : root.value("effects").toArray()) {
        const QJsonObject candidate = value.toObject();
        if (candidate.value("id").toString() == id)
            return candidate;
    }
    return {};
}

QString moduleNames(const QJsonArray &modules)
{
    QStringList names;
    for (const QJsonValue &value : modules) {
        const QString name = value.toObject().value("name").toString();
        if (!name.isEmpty())
            names.append(name);
    }
    return names.join(QString::fromUtf8(" → "));
}

LocalKnowledgeAnswer authoritative(const QString &topic,
                                   const QString &text,
                                   const QStringList &blocks = {})
{
    LocalKnowledgeAnswer result;
    result.answered = true;
    result.confidence = Confidence::Authoritative;
    result.topicId = topic;
    result.text = text;
    result.sourceKind = SourceKind::MidiTable;
    result.relatedBlocks = blocks;
    return result;
}

} // namespace

LocalKnowledgeEngine::LocalKnowledgeEngine()
    : guides(builtInGuides()),
      officialDocumentation(officialGT10DocumentationIndex()),
      documentationSource(&officialDocumentation)
{
}

LocalKnowledgeEngine::LocalKnowledgeEngine(
    const DocumentationKnowledgeSource *source)
    : guides(builtInGuides()),
      officialDocumentation(officialGT10DocumentationIndex()),
      documentationSource(source ? source : &officialDocumentation)
{
}

LocalKnowledgeEngine::Intent LocalKnowledgeEngine::classify(
    const QString &question) const
{
    const QString text = GT10CapabilityService::normalize(question);
    if (any(text, {"ajuda", "help", "o que voce sabe", "what can you"}))
        return Intent::Help;
    if (any(text, {"patch atual", "current patch", "qual patch", "which patch"})
        || (text.contains("qual") && any(text, {"delay", "reverb", "preamp"})))
        return Intent::CurrentPatch;
    if (any(text, {"cadeia", "signal chain"}))
        return Intent::SignalChain;
    if (any(text, {"roteamento", "routing", "dual l r", "dual lr"}))
        return Intent::Routing;
    if (any(text, {"patch ou system", "patch vs system", "patch or system", "escopo"}))
        return Intent::PatchVsSystem;
    if (any(text, {"pedal de expressao", "expression pedal", "exp sw", "tremolo rate com exp", "feedback com exp"}))
        return Intent::HowToExpression;
    if (any(text, {"como configurar um assign", "how to configure an assign", "target min", "target max", "source mode", "o que e target", "what is target", "o que e source", "what is source"}))
        return Intent::HowToAssign;
    if (any(text, {"ctl1", "ctl2", "foot 1", "foot 2", "foot 3", "foot 4", "bank up", "bank down", "controlador", "controller"}))
        return Intent::HowToController;
    if (any(text, {"quantos assigns", "how many assigns", "possui", "tem ", "does it have", "does the gt 10", "quais efeitos", "what effects", "quais reverbs", "what reverbs"}))
        return Intent::Capability;
    if (any(text, {"reverb", "delay", "preamp", "fx1", "fx2", "harmonist", "tremolo", "compressor", "chorus"}))
        return Intent::EffectInfo;
    return Intent::Unknown;
}

LocalKnowledgeAnswer LocalKnowledgeEngine::answer(
    const QString &question, const QString &language,
    const AIContext *context) const
{
    const QString text = GT10CapabilityService::normalize(question);
    const auto enriched = [&](const LocalKnowledgeAnswer &base) {
        return enrichWithDocumentation(base, question, language);
    };
    switch (classify(question)) {
    case Intent::Help:
        return enriched(authoritative("help", portuguese(language)
            ? "Posso consultar a base local da GT-10 sobre blocos, tipos de efeito, parâmetros, Assign 1–8, controladores, EXP/EXP SW, Signal Chain, routing e o patch atual. Quando a base não comprovar algo, direi que a informação é insuficiente."
            : "I can query the local GT-10 knowledge base about blocks, effect types, parameters, Assigns 1–8, controllers, EXP/EXP SW, Signal Chain, routing, and the current patch. When the local sources do not prove something, I will say the information is insufficient."));
    case Intent::CurrentPatch:
        return currentPatchAnswer(text, language, context);
    case Intent::SignalChain: {
        LocalKnowledgeAnswer current = currentPatchAnswer(text, language, context);
        return enriched(current.answered ? current
                                         : guideAnswer("signal_chain_basics", language));
    }
    case Intent::Routing: {
        LocalKnowledgeAnswer current = currentPatchAnswer(text, language, context);
        return enriched(current.answered ? current
                                         : guideAnswer("routing_basics", language));
    }
    case Intent::PatchVsSystem:
        if (text.contains("assign"))
            return enriched(authoritative("patch_vs_system",
                portuguese(language)
                    ? "Assign 1–8 pertence ao patch: seus estados, Target, Target Min/Max, Source e Source Mode são armazenados no contexto do patch. Configurações identificadas como System são globais."
                    : "Assigns 1–8 are patch-scoped: their enabled state, Target, Target Min/Max, Source, and Source Mode are stored with the patch. Settings identified as System are global."));
        return enriched(guideAnswer("controller_scope", language));
    case Intent::HowToAssign:
        if (any(text, {"target min", "target max"}))
            return enriched(authoritative("assign",
                portuguese(language)
                    ? "Target Min e Target Max definem os dois extremos que o Assign aplica ao parâmetro Target. A faixa válida depende do Target real selecionado no catálogo."
                    : "Target Min and Target Max define the two endpoints that the Assign applies to the Target parameter. The valid range depends on the actual Target selected in the catalog."));
        if (text.contains("source mode"))
            return enriched(authoritative("assign",
                portuguese(language)
                    ? "Source Mode define como o evento de Source é interpretado pelo Assign. Os modos disponíveis vêm do catálogo local da GT-10: " + capabilityService.assignSourceModes().join(", ") + "."
                    : "Source Mode defines how the Assign interprets the Source event. Available modes come from the local GT-10 catalog: " + capabilityService.assignSourceModes().join(", ") + "."));
        if (any(text, {"o que e target", "what is target"}))
            return enriched(authoritative("assign",
                portuguese(language)
                    ? "Target é o parâmetro real da GT-10 que o Assign controla. A disponibilidade deve ser confirmada no catálogo para o bloco e tipo de efeito em uso."
                    : "Target is the actual GT-10 parameter controlled by the Assign. Availability must be confirmed in the catalog for the active block and effect type."));
        if (any(text, {"o que e source", "what is source"}))
            return enriched(authoritative("assign",
                portuguese(language)
                    ? "Source é o controle ou evento que aciona o Assign, como CTL, EXP ou outra fonte presente no catálogo local."
                    : "Source is the control or event that drives the Assign, such as CTL, EXP, or another source present in the local catalog."));
        return enriched(guideAnswer("assign_basics", language));
    case Intent::HowToExpression: {
        if (text.contains("tremolo") && text.contains("rate")) {
            if (!capabilityService.supportsAssignTarget("Tremolo Rate"))
                return unknown(language, "expression");
            return enriched(authoritative("expression",
                portuguese(language)
                    ? "O catálogo local confirma Tremolo Rate como Target. Selecione um Assign 1–8, ative-o, escolha Tremolo Rate no bloco FX correspondente como Target, defina Target Min/Max, escolha EXP como Source e confira Source Mode e a faixa ativa. Nenhuma alteração foi aplicada ao patch."
                    : "The local catalog confirms Tremolo Rate as a Target. Select an Assign from 1–8, enable it, choose Tremolo Rate in the corresponding FX block as Target, set Target Min/Max, choose EXP as Source, and check Source Mode and the active range. No patch change was applied.",
                {"fx1", "fx2"}));
        }
        if (text.contains("delay") && text.contains("feedback")) {
            if (!capabilityService.supportsAssignTarget("Delay Feedback"))
                return unknown(language, "expression");
            return enriched(authoritative("expression",
                portuguese(language)
                    ? "O catálogo local confirma Delay Feedback como Target. Use um Assign ativo com Delay Feedback como Target, ajuste Target Min/Max e selecione EXP como Source. Confira Source Mode e a faixa ativa."
                    : "The local catalog confirms Delay Feedback as a Target. Use an enabled Assign with Delay Feedback as Target, set Target Min/Max, and select EXP as Source. Check Source Mode and the active range.",
                {"delay"}));
        }
        return enriched(guideAnswer("expression_basics", language));
    }
    case Intent::HowToController:
        return enriched(controllerAnswer(text, language));
    case Intent::Capability:
    case Intent::EffectInfo: {
        LocalKnowledgeAnswer result = capabilityAnswer(text, language);
        if (result.answered)
            return enriched(result);
        result = currentPatchAnswer(text, language, context);
        if (result.answered)
            return enriched(result);
        result = enriched(result);
        if (result.answered)
            return result;
        return (documentationSource->isAvailable()
                || any(text, {"manual", "documentacao", "documentation"}))
            ? documentationUnknown(language, "documentation") : result;
    }
    case Intent::Unknown: {
        LocalKnowledgeAnswer result = enriched(unknown(language));
        if (result.answered)
            return result;
        if (any(text, {"manual", "documentacao", "documentation"})
            || documentationSource->isAvailable())
            return documentationUnknown(language);
        return result;
    }
    }
    return unknown(language);
}

const GT10CapabilityService &LocalKnowledgeEngine::capabilities() const
{
    return capabilityService;
}

bool LocalKnowledgeEngine::portuguese(const QString &language) const
{
    return language.startsWith("pt", Qt::CaseInsensitive);
}

LocalKnowledgeAnswer LocalKnowledgeEngine::unknown(
    const QString &language, const QString &topic) const
{
    LocalKnowledgeAnswer result;
    result.topicId = topic;
    result.text = portuguese(language)
        ? "Não encontrei informação suficiente na base local da GT-10 para afirmar isso com segurança."
        : "I couldn't find enough information in the local GT-10 knowledge base to state that reliably.";
    return result;
}

LocalKnowledgeAnswer LocalKnowledgeEngine::documentationUnknown(
    const QString &language, const QString &topic) const
{
    LocalKnowledgeAnswer result;
    result.topicId = topic;
    result.text = portuguese(language)
        ? "Não encontrei informação suficiente na documentação local da GT-10 para responder isso com segurança."
        : "I couldn't find enough information in the local GT-10 documentation to answer that reliably.";
    return result;
}

LocalKnowledgeAnswer LocalKnowledgeEngine::enrichWithDocumentation(
    const LocalKnowledgeAnswer &base, const QString &question,
    const QString &language) const
{
    if (!documentationSource || !documentationSource->isAvailable())
        return base;
    const LocalKnowledgeAnswer documented = documentationSource->answer(
        question, language);
    if (!documented.answered)
        return base;

    LocalKnowledgeAnswer result = base;
    if (!base.answered)
        return documented;

    const QJsonObject trace = documented.metadata.value(
        "documentationTrace").toObject();
    const QJsonObject claims = trace.value("claims").toObject();
    const QString factKey = base.metadata.value("factKey").toString();
    const QJsonValue structuralValue = base.metadata.value("factValue");
    if (!factKey.isEmpty() && claims.contains(factKey)
        && claims.value(factKey) != structuralValue) {
        result.metadata.insert("documentationConflict", true);
        result.metadata.insert("documentationTrace", trace);
        result.text += portuguese(language)
            ? "\n\nA base técnica do Editor e a documentação local divergem neste ponto; a base estrutural foi preservada."
            : "\n\nThe Editor's technical base and the local documentation differ on this point; structural truth was preserved.";
        return result;
    }

    result.metadata.insert("documentationTrace", trace);
    if (base.confidence == Confidence::Guidance) {
        result.text = documented.text + "\n\n" + base.text;
        result.confidence = documented.confidence;
        result.sourceKind = documented.sourceKind;
    } else {
        result.text += "\n\n" + documented.text;
    }
    return result;
}

LocalKnowledgeAnswer LocalKnowledgeEngine::guideAnswer(
    const QString &id, const QString &language) const
{
    const KnowledgeGuide *guide = findGuide(guides, id);
    if (!guide)
        return unknown(language, id);
    LocalKnowledgeAnswer result;
    result.answered = true;
    result.confidence = Confidence::Guidance;
    result.topicId = id;
    result.text = portuguese(language) ? guide->bodyPtBr : guide->bodyEn;
    result.sourceKind = SourceKind::Guide;
    result.relatedBlocks = guide->relatedTopics;
    return result;
}

LocalKnowledgeAnswer LocalKnowledgeEngine::currentPatchAnswer(
    const QString &text, const QString &language,
    const AIContext *context) const
{
    if (!context || context->root.isEmpty())
        return unknown(language, "current_patch");
    const QJsonObject root = context->root;
    if (any(text, {"qual patch", "patch atual", "current patch", "which patch"})) {
        const QJsonObject patch = root.value("patch").toObject();
        if (patch.isEmpty())
            return unknown(language, "current_patch");
        return authoritative("current_patch",
            portuguese(language)
                ? QString("Patch atual: %1 — %2.").arg(
                    patch.value("number").toString(), patch.value("name").toString())
                : QString("Current patch: %1 — %2.").arg(
                    patch.value("number").toString(), patch.value("name").toString()));
    }
    if (text.contains("delay")) {
        const QString type = effect(root, "delay").value("type").toString();
        if (!type.isEmpty())
            return authoritative("current_delay",
                portuguese(language) ? QString("O patch atual usa Delay %1.").arg(type)
                                     : QString("The current patch uses %1 Delay.").arg(type),
                {"delay"});
    }
    if (text.contains("reverb")) {
        const QString type = effect(root, "reverb").value("type").toString();
        if (!type.isEmpty())
            return authoritative("current_reverb",
                portuguese(language) ? QString("O patch atual usa Reverb %1.").arg(type)
                                     : QString("The current patch uses %1 Reverb.").arg(type),
                {"reverb"});
    }
    if (any(text, {"cadeia", "signal chain"})) {
        const QJsonObject chain = root.value("signalChain").toObject();
        QStringList parts;
        const QString prefix = moduleNames(chain.value("commonPrefix").toArray());
        const QString pathA = moduleNames(chain.value("pathA").toArray());
        const QString pathB = moduleNames(chain.value("pathB").toArray());
        const QString suffix = moduleNames(chain.value("commonSuffix").toArray());
        if (!prefix.isEmpty()) parts << prefix;
        if (!pathA.isEmpty()) parts << QString("A: %1").arg(pathA);
        if (!pathB.isEmpty()) parts << QString("B: %1").arg(pathB);
        if (!suffix.isEmpty()) parts << suffix;
        if (!parts.isEmpty())
            return authoritative("signal_chain",
                (portuguese(language) ? "Signal Chain atual: " : "Current Signal Chain: ")
                    + parts.join(" | ") + ".",
                {"signal_chain"});
    }
    if (any(text, {"routing", "roteamento", "dual l r", "dual lr"})) {
        const QString mode = root.value("routing").toObject().value("mode").toString();
        if (!mode.isEmpty())
            return authoritative("routing",
                portuguese(language) ? QString("O modo de routing atual é %1.").arg(mode)
                                     : QString("The current routing mode is %1.").arg(mode),
                {"routing"});
    }
    return unknown(language, "current_patch");
}

LocalKnowledgeAnswer LocalKnowledgeEngine::capabilityAnswer(
    const QString &text, const QString &language) const
{
    const bool pt = portuguese(language);
    if (any(text, {"quantos assigns", "how many assigns"})) {
        LocalKnowledgeAnswer result = authoritative(
            "assign", pt ? "A GT-10 possui 8 Assigns por patch: Assign 1–8."
                          : "The GT-10 provides 8 Assigns per patch: Assigns 1–8.");
        result.metadata.insert("factKey", "assign.count");
        result.metadata.insert("factValue", capabilityService.assignCount());
        return result;
    }
    if (any(text, {"quais efeitos", "what effects"}))
        return authoritative("effects",
            (pt ? "Blocos de efeito conhecidos pela base local: "
                : "Effect blocks known by the local catalog: ")
                + capabilityService.blockIds().join(", ") + ".");
    if (any(text, {"quais reverbs", "what reverbs"})) {
        const QStringList types = capabilityService.effectTypesForBlock("reverb");
        if (!types.isEmpty())
            return authoritative("reverb", (pt ? "Tipos de Reverb: " : "Reverb types: ")
                + types.join(", ") + ".", {"reverb"});
    }
    if (any(text, {"quais parametros", "what parameters"})) {
        QString blockId;
        if (text.contains("reverb")) blockId = "reverb";
        else if (text.contains("delay")) blockId = "delay";
        else if (text.contains("fx1")) blockId = "fx1";
        else if (text.contains("fx2")) blockId = "fx2";
        else if (text.contains("preamp a")) blockId = "preampA";
        else if (text.contains("preamp b")) blockId = "preampB";

        if (!blockId.isEmpty()) {
            QString effectType;
            for (const QString &candidate : capabilityService.effectTypesForBlock(blockId)) {
                if (text.contains(GT10CapabilityService::normalize(candidate))) {
                    effectType = candidate;
                    break;
                }
            }
            const QStringList parameters = capabilityService.parametersForEffect(
                blockId, effectType);
            if (!parameters.isEmpty())
                return authoritative("parameters",
                    (pt ? QString("Parâmetros confirmados para %1 em %2: ")
                              .arg(effectType.isEmpty() ? blockId : effectType, blockId)
                        : QString("Confirmed parameters for %1 in %2: ")
                              .arg(effectType.isEmpty() ? blockId : effectType, blockId))
                        + parameters.join(", ") + ".", {blockId});
        }
        return unknown(language, "parameters");
    }
    if (text.contains("dual delay")) {
        bool found = false;
        for (const QString &type : capabilityService.effectTypesForBlock("delay"))
            found = found || GT10CapabilityService::normalize(type).contains("dual");
        if (found)
            return authoritative("delay", pt ? "Sim. O catálogo local do Delay contém tipos Dual."
                                              : "Yes. The local Delay catalog contains Dual types.", {"delay"});
    }
    if (text.contains("spring") && text.contains("reverb")) {
        if (capabilityService.hasEffectType("reverb", "spring"))
            return authoritative("reverb", pt ? "Sim. Spring é um tipo de Reverb confirmado pelo catálogo local."
                                               : "Yes. Spring is a Reverb type confirmed by the local catalog.", {"reverb"});
    }
    if (text.contains("tremolo") && text.contains("fx1")) {
        if (capabilityService.hasEffectType("fx1", "tremolo"))
            return authoritative("fx1", pt ? "Sim. FX1 possui Tremolo no catálogo local."
                                            : "Yes. FX1 includes Tremolo in the local catalog.", {"fx1"});
    }
    if (text.contains("harmonist") && text.contains("fx2")) {
        if (capabilityService.hasEffectType("fx2", "harmonist"))
            return authoritative("fx2", pt ? "Sim. FX2 possui Harmonist no catálogo local."
                                            : "Yes. FX2 includes Harmonist in the local catalog.", {"fx2"});
    }
    for (const QString &effectType : {QStringLiteral("Harmonist"),
                                      QStringLiteral("Tremolo")}) {
        if (!text.contains(GT10CapabilityService::normalize(effectType)))
            continue;
        const QStringList blocks = capabilityService.blocksForEffectType(effectType);
        if (!blocks.isEmpty())
            return authoritative("effects",
                pt ? QString("Sim. %1 aparece no catálogo local em: %2.")
                         .arg(effectType, blocks.join(", "))
                   : QString("Yes. %1 appears in the local catalog in: %2.")
                         .arg(effectType, blocks.join(", ")),
                blocks);
    }
    if (text.contains("preamp") && (text.contains(" a ") || text.contains(" b ") || text.contains("a e b")))
        return authoritative("preamp", pt ? "Sim. A estrutura do patch contém PREAMP A e PREAMP B."
                                           : "Yes. The patch structure contains PREAMP A and PREAMP B.",
                             {"preampA", "preampB"});
    if (text.contains("neural") || text.contains("xyz123")
        || text.contains("17 reverb") || text.contains("tres reverb")
        || text.contains("three reverb"))
        return unknown(language, "capability");
    return unknown(language, "capability");
}

LocalKnowledgeAnswer LocalKnowledgeEngine::controllerAnswer(
    const QString &text, const QString &language) const
{
    QString query;
    if (text.contains("ctl1")) query = "ctl1";
    else if (text.contains("ctl2")) query = "ctl2";
    else if (text.contains("exp sw")) query = "exp_sw";
    else if (text.contains("foot 1")) query = "foot1";
    else if (text.contains("foot 2")) query = "foot2";
    else if (text.contains("foot 3")) query = "foot3";
    else if (text.contains("foot 4")) query = "foot4";
    else if (text.contains("bank up")) query = "bank_up";
    else if (text.contains("bank down")) query = "bank_down";
    const ControllerCapability item = capabilityService.controller(query);
    if (item.id.isEmpty())
        return guideAnswer("controller_scope", language);
    const QString scope = GT10CapabilityService::scopeName(item.scope);
    return authoritative("controllers",
        portuguese(language)
            ? QString("%1 usa escopo %2 no modelo atual do Editor. Configuração: %3. Como Source de Assign: %4.")
                  .arg(item.name, scope, item.configuredIn,
                       item.assignSource ? "sim" : "não confirmado")
            : QString("%1 uses %2 scope in the current Editor model. Configuration: %3. Assign Source: %4.")
                  .arg(item.name, scope, item.configuredIn,
                       item.assignSource ? "yes" : "not confirmed"));
}

} // namespace Knowledge
} // namespace Ai
