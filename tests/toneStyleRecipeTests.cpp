#include "ai/LocalGT10Provider.h"
#include "ai/tone/ToneStyleRecipes.h"
#include "ai/tone/ToneIntentParser.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QTextStream>
#include <QTimer>

using namespace Ai;
using namespace Ai::Tone;
#define TONE_STRINGIFY_DETAIL(value) #value
#define TONE_STRINGIFY(value) TONE_STRINGIFY_DETAIL(value)
namespace {
int assertions = 0, failures = 0;
void expect(bool ok, const QString &label)
{
    ++assertions;
    if (!ok) { ++failures; QTextStream(stderr) << "FAIL " << label << '\n'; }
}
AIResponse ask(LocalGT10Provider &provider, const QString &question,
               const AIContext &context = {}, const QString &language = "pt-BR")
{
    AIResponse result;
    QEventLoop loop;
    bool received = false;
    const auto connection = QObject::connect(&provider, &AIProvider::responseReady,
        &loop, [&](const QString &, const AIResponse &response) {
            received = true; result = response; loop.quit();
        });
    AIRequest request; request.userMessage = question;
    request.context = context; request.language = language;
    provider.sendRequest("style-test", request);
    QTimer::singleShot(2000, &loop, &QEventLoop::quit);
    loop.exec(); QObject::disconnect(connection);
    expect(received, "provider response " + question);
    return result;
}
bool includes(const QJsonArray &array, const QString &value)
{
    for (const QJsonValue &v : array) if (v.toString() == value) return true;
    return false;
}
}
int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    if (application.arguments().contains("--relevance-capture") || application.arguments().contains("--role-capture") || application.arguments().contains("--density-capture")) {
        LocalGT10Provider provider;
        AIContext context;
        context.root.insert("patch", QJsonObject{{"number", "U01-1"}, {"name", "Capture fixture"}});
        context.root.insert("routing", QJsonObject{{"mode", "Single"}});
        context.root.insert("effects", QJsonArray{
            QJsonObject{{"id", "preampA"}, {"enabled", true}, {"type", "MS Modern"}, {"gain", "80"}, {"bass", "85"}},
            QJsonObject{{"id", "odDs"}, {"enabled", true}, {"type", "Blues OD"}, {"drive", "70"}},
            QJsonObject{{"id", "delay"}, {"enabled", false}, {"type", "Single"}},
            QJsonObject{{"id", "reverb"}, {"enabled", true}, {"type", "Hall1"}, {"reverbTime", "9.0 sec"}}
        });
        const QStringList questions = application.arguments().contains("--density-capture") ? QStringList{
            "Como montar um timbre Worship Lead estéreo?", "Como montar um clean Worship?",
            "Quero um high gain estéreo para lead, mas sem ficar embolado.",
            "Quero um crunch para base com mais definição.", "Como fazer um high gain?"}
            : application.arguments().contains("--role-capture") ? QStringList{
            "Quero um high gain estéreo para lead, mas sem ficar embolado.",
            "Como fazer um high gain?", "Quero um clean worship para arpejo, sem ficar estridente.",
            "Quero um crunch para base, mas com mais definição.",
            "Quero um lead com sustain sem aumentar muito o ganho."} : QStringList{QString("Como montar um timbre clean para tocar na igreja?"),
                QString("Como fazer um timbre high gain estéreo?"), QString("Como fazer um high gain?"),
                QString("Como fazer um preset soar estéreo?"),
                QString("Como transformar meu patch atual em high gain estéreo?")};
        for (const QString &q : questions) {
            provider.resetConversation();
            const AIResponse response = ask(provider, q, context);
            QTextStream(stdout) << "CASE " << q << "\nTRACE "
                << QJsonDocument(response.metadata).toJson(QJsonDocument::Compact)
                << "\nRESPONSE\n" << response.text << "\n\n";
        }
        return failures ? 1 : 0;
    }
    expect(ToneTaxonomy::families().size() == 6, "six closed families");
    expect(ToneTaxonomy::profiles().size() == 7, "seven closed profiles");
    expect(ToneTaxonomy::modifiers().size() == 16, "sixteen closed modifiers");
    const QStringList modifierPhrases = {"brighter", "darker", "warmer", "more defined", "less muddy",
        "more dynamic", "more compressed", "more ambient", "less ambient", "wider", "more focused",
        "more aggressive", "more smooth", "more sustain", "less gain", "more gain"};
    for (int i = 0; i < modifierPhrases.size(); ++i) {
        const auto parsed = ToneTaxonomy::parse("Build a clean tone " + modifierPhrases[i]);
        expect(parsed.recognized && parsed.modifiers.contains(static_cast<ToneModifier>(i)), "modifier semantics " + modifierPhrases[i]);
    }
    for (const QString &alias : {QString("limpo"), QString("cristalino"), QString("sem drive"), QString("limpinho"), QString("clin")})
        expect(ToneTaxonomy::parse("Quero um timbre " + alias).family == ToneFamily::Clean, "clean synonym " + alias);
    for (const QString &alias : {QString("xote"), QString("baiao"), QString("piseiro")})
        expect(ToneTaxonomy::parse("Quero um clean para " + alias).profile == ToneProfile::Forro, "forro synonym " + alias);
    for (const QString &alias : {QString("groove"), QString("r&b"), QString("soul moderno"), QString("neosoul")})
        expect(ToneTaxonomy::parse("Quero um clean " + alias).profile == ToneProfile::NeoSoul, "neo soul synonym " + alias);
    const ToneStyleRecipes service;
    for (const ToneRecipe &r : service.recipes()) {
        expect(!r.goals.isEmpty() && !r.blockGuidance.isEmpty(), "recipe has goals and guidance");
        expect(r.rationale.sources.contains("Guidance"), "musical guidance trace");
    }
    const QList<QPair<ToneFamily,ToneProfile>> required = {
        {ToneFamily::Clean,ToneProfile::Neutral}, {ToneFamily::Clean,ToneProfile::Worship},
        {ToneFamily::Clean,ToneProfile::Forro}, {ToneFamily::Clean,ToneProfile::NeoSoul},
        {ToneFamily::Clean,ToneProfile::Pop}, {ToneFamily::EdgeOfBreakup,ToneProfile::Worship},
        {ToneFamily::Crunch,ToneProfile::Rock}, {ToneFamily::HighGain,ToneProfile::Metal},
        {ToneFamily::Lead,ToneProfile::Rock}, {ToneFamily::Lead,ToneProfile::Worship},
        {ToneFamily::Ambient,ToneProfile::Worship}
    };
    for (const auto &pair : required) {
        bool found = false;
        for (const auto &r : service.recipes()) if (r.family == pair.first && r.profile == pair.second) found = true;
        expect(found, "required recipe " + ToneTaxonomy::id(pair.first) + "/" + ToneTaxonomy::id(pair.second));
    }
    Knowledge::GT10CapabilityService capabilities;
    for (const auto &r : service.recipes())
        for (const auto &s : r.blockGuidance)
            for (const QString &type : s.compatibleTypes)
                expect(capabilities.effectTypesForBlock(s.blockId).contains(type, Qt::CaseInsensitive), "recipe exact model " + s.blockId + "/" + type);
    QStringList catalogBlocks;
    for (const auto &entry : service.catalog()) {
        catalogBlocks.append(entry.blockId);
        expect(capabilities.effectTypesForBlock(entry.blockId).contains(entry.type, Qt::CaseInsensitive), "catalog exact type " + entry.type);
        expect(!entry.limitation.isEmpty() && entry.sources.contains("Guidance"), "catalog honest tendency " + entry.type);
    }
    for (const QString &b : {QString("preampA"),QString("odDs"),QString("comp"),QString("delay"),QString("reverb"),QString("chorus"),QString("fx1")})
        expect(catalogBlocks.contains(b), "catalog covers " + b);

    QFile corpus(QString::fromLatin1(TONE_STRINGIFY(TONE_BUILD_CORPUS)));
    expect(corpus.open(QIODevice::ReadOnly), "corpus available");
    const QJsonArray cases = QJsonDocument::fromJson(corpus.readAll()).object().value("questions").toArray();
    expect(cases.size() >= 70, "real corpus includes Phase 6.4 builds and contextual detail turns");
    LocalGT10Provider provider;
    int caseIndex = 0;
    for (const QJsonValue &v : cases) {
        const QJsonObject c = v.toObject();
        const QString question = c.value("question").toString();
        if (c.contains("seed")) {
            provider.resetConversation();
            ask(provider, c.value("seed").toString());
        }
        const AIResponse response = ask(provider, question);
        const QJsonObject m = response.metadata;
        expect(m.value("detectedIntent") == c.value("intent"), "intent " + question);
        if (question == "100 Hz e 8 kHz é obrigatório?") {
            // This previously used the remembered Worship recipe. The explicit
            // promotion now answers as dataset EQ guidance, not a style build.
            expect(m.value("route") == "eq_dataset_guidance" && m.value("readOnly").toBool(), "approved general EQ route replaces style followup");
            expect(response.text.contains("Não é uma regra") && response.text.contains("110 Hz") && response.text.contains("8 kHz"), "non-mandatory actual EQ references");
            expect(m.value("fieldKnowledge").toArray().size() == 2, "both approved traces, no fabricated style metadata");
            ++caseIndex;
            continue;
        }
        expect(m.value("toneFamily") == c.value("family"), "family " + question);
        expect(m.value("toneProfile") == c.value("profile"), "profile " + question);
        expect(m.value("toneModifiers") == c.value("modifiers"), "modifiers " + question);
        for (const QString &dimension : {QString("usageRole"), QString("toneProblem")})
            if (c.contains(dimension)) expect(m.value(dimension) == c.value(dimension), dimension + " / " + question);
        expect(m.value("readOnly").toBool(), "read-only " + question);
        const QString normalized = ToneIntentParser::normalize(response.text);
        for (const auto &term : c.value("mustAddress").toArray())
            expect(normalized.contains(ToneIntentParser::normalize(term.toString())), "addresses " + term.toString() + " / " + question);
        for (const auto &b : c.value("expectedSubsystems").toArray())
            expect(includes(m.value("expectedSubsystems").toArray(), b.toString()), "subsystem " + b.toString() + " / " + question);
        // Check propositions, not entire response strings. Prohibited effects
        // may be mentioned as unsupported, but never appear as recipe options.
        for (const auto &s : m.value("recipeSteps").toArray()) {
            const QJsonObject step = s.toObject();
            for (const auto &type : step.value("types").toArray()) {
                expect(capabilities.effectTypesForBlock(step.value("blockId").toString()).contains(type.toString(),Qt::CaseInsensitive), "all proposed models exist");
                for (const auto &bad : c.value("mustNotInvent").toArray())
                    expect(type.toString().compare(bad.toString(),Qt::CaseInsensitive)!=0, "forbidden model never recommended");
            }
        }
        if (c.value("clarify").toBool()) expect(!m.value("answered").toBool(), "ambiguous heavy requires clarification");
        else expect(m.value("answered").toBool(), "style answered");
        if (caseIndex < 10 && (caseIndex < 5 || caseIndex == 6 || caseIndex == 7 || caseIndex == 8 || caseIndex == 9))
            QTextStream(stdout) << "CASE " << question << "\nTRACE " << QJsonDocument(m).toJson(QJsonDocument::Compact) << "\nRESPONSE\n" << response.text << "\n\n";
        ++caseIndex;
    }
    for (const QString &q : {QString("Quero um clean zorbip."),QString("Qual a diferença entre Ambience e Modulate?"),QString("Quero mais crunch."),QString("Mas quero preservar o clean"),QString("Andy Timmons"),QString("Quero um clean com Shimmer")}) {
        if (q.contains("zorbip")) expect(ToneTaxonomy::parse(q).profile == ToneProfile::Neutral, "unknown word not fuzzy mapped");
        else expect(ToneIntentParser().parse(q).kind != ToneRequestKind::BuildTone, "legacy/unsupported route preserved " + q);
    }
    const auto artist = ask(provider, "Andy Timmons");
    expect(artist.metadata.value("topicId").toString() == "artist_profile", "artist unavailable in provider");
    expect(!artist.metadata.value("answered").toBool(), "no artist profile claim");
    AIContext context;
    context.root.insert("patch", QJsonObject{{"number","U01-1"},{"name","Test"}});
    context.root.insert("effects", QJsonArray{
        QJsonObject{{"id","preampA"},{"enabled",true},{"type","MS Modern"},{"gain","80"},{"bass","85"}},
        QJsonObject{{"id","odDs"},{"enabled",true},{"type","Blues OD"},{"drive","70"}},
        QJsonObject{{"id","reverb"},{"enabled",true},{"type","Hall1"},{"reverbTime","9.0 sec"}}
    });
    const QJsonObject before = context.root;
    const auto adapted = ask(provider,"Quero transformar esse preset em clean para forró; me dê valores.",context);
    expect(adapted.metadata.value("patchAware").toBool(), "patch-aware build");
    expect(adapted.text.contains("80") && adapted.text.contains("OD/DS está ON"), "uses actual current values/states");
    expect(adapted.text.contains("acúmulo de graves"), "reuse tone feature analysis");
    expect(context.root == before, "context unchanged");
    expect(adapted.text.contains("não valor ideal"), "numeric guidance conditional");
    const auto fromScratch = ask(provider,"Como montar um clean para forró?",{},"en");
    expect(!fromScratch.text.contains("Para um") && fromScratch.text.contains("forró clean"), "English style response");
    const auto range = ask(provider,"Quero um clean; me dê valores.");
    expect(range.text.contains("Faixa técnica de Gain"), "catalog range on explicit values request");
    AIContext stereoContext = context;
    stereoContext.root.insert("routing", QJsonObject{{"mode", "Single"}});
    const QJsonObject stereoBefore = stereoContext.root;
    const auto stereo = ask(provider, "Como fazer meu preset soar estéreo?", stereoContext);
    expect(stereo.text.contains("Channel Mode atual: Single"), "uses compact AIContext routing, not excluded preampGlobal");
    AIContext sparse;
    sparse.root.insert("effects", QJsonArray{QJsonObject{{"id", "preampA"}}});
    const auto unknown = ask(provider, "Como transformar meu patch em clean?", sparse);
    expect(unknown.text.contains("desconhecido") && unknown.text.contains("Gain ?"), "missing patch values stay unknown");
    const auto worship = ask(provider, "Como montar um clean Worship?");
    const auto forro = ask(provider, "Como montar um clean para Forró?");
    const auto neoSoul = ask(provider, "Como montar um clean Neo Soul?");
    expect(worship.text != forro.text && worship.text != neoSoul.text && forro.text != neoSoul.text, "three musical identities differ");
    expect(!worship.text.contains("JC-120") && worship.text.contains("VO CLEAN") && worship.text.contains("leve saturação"), "worship amp/edge direction replaces neutral defaults");
    expect(worship.text.contains("referência") && worship.text.contains("não regra") && !worship.text.contains("inclusive usando FLAT"), "compact EQ references not obligations or a repeated EQ lesson");
    const QJsonArray cutInfo = worship.metadata.value("eqCutCapabilities").toArray();
    expect(cutInfo.size() == 2, "two distinct EQ cutoff descriptors despite identical table labels");
    for (const auto &v : cutInfo) {
        const auto c = v.toObject();
        const auto values = c.value("enumValues").toArray();
        if (c.value("parameterId") == "lowCut")
            expect(values.contains("110Hz") && !values.contains("100Hz") && values.contains("800Hz"), "actual EQ low-cut enum, not invented 100Hz");
        else expect(values.contains("8.00kHz") && values.contains("700Hz") && values.contains("FLAT"), "actual EQ high-cut enum");
    }
    const QJsonArray field = worship.metadata.value("fieldKnowledge").toArray();
    expect(field.size() == 3 && includes(worship.metadata.value("knowledgeSources").toArray(), "FieldKnowledge"), "field knowledge traces used");
    for (const auto &v : field)
        expect(v.toObject().value("validationType") == "CuratedMusicalGuidance" && v.toObject().value("confidence") == "Guidance", "no invented physical validation or official authority");
    const auto big = ask(provider, "Como fazer um preset soar grande?");
    const auto wide = ask(provider, "Como fazer um preset soar estéreo?");
    expect(big.text != wide.text && big.metadata.value("toneGoal") == "bigger" && wide.metadata.value("toneGoal") == "wider", "big and stereo semantically distinct");
    expect(big.metadata.value("toneModifiers").toArray().isEmpty() && includes(wide.metadata.value("toneModifiers").toArray(), "wider"), "big does not automatically mean wider");
    expect(big.text.contains("grande em mono") && !big.text.contains("JC-120"), "big in mono without forcing clean amp");
    const auto metal = ask(provider, "Como montar um high gain para metal?");
    expect(metal.text.contains("COMP não é prioridade") && metal.metadata.value("usageRole") == "neutral", "metal does not invent rhythm role or prioritize compressor");
    const auto bigPatch = ask(provider, "Como deixar meu timbre grande sem estéreo?", context);
    expect(!bigPatch.text.contains("experimente desligá-lo") && !bigPatch.text.contains("base limpa"), "spatial request preserves driven family rather than forcing clean");
    const auto worshipEn = ask(provider, "How can I build a clean worship tone?", {}, "en");
    expect(worshipEn.text.contains("light saturation") && worshipEn.text.contains("not a rule"), "refinement remains bilingual");
    const auto conceptual = ask(provider, "Como fazer um timbre high gain estéreo?", stereoContext);
    const auto highGain = ask(provider, "Como fazer um high gain?", stereoContext);
    const auto stereoOnly = ask(provider, "Como fazer um preset soar estéreo?", stereoContext);
    const auto adaptedHighGain = ask(provider, "Como transformar meu patch atual em high gain estéreo?", stereoContext);
    expect(conceptual.metadata.value("toneFamily") == "high_gain" && conceptual.metadata.value("toneProfile") == "neutral"
        && conceptual.metadata.value("toneGoal") == "wider", "compound high gain stereo without forced metal");
    expect(conceptual.text.section('.', 0, 0).contains("high gain") && conceptual.text.section('.', 0, 0).contains("Dual L/R"), "first sentence answers compound intent");
    expect(conceptual.text != highGain.text && conceptual.text != stereoOnly.text, "family plus stereo is not either dimension alone");
    expect(!conceptual.metadata.value("patchAware").toBool() && conceptual.metadata.value("patchContextAvailable").toBool(), "available context secondary for conceptual build");
    expect(!conceptual.metadata.contains("toneFeatures") && !conceptual.metadata.contains("toneSuggestions"), "conceptual builds do not analyze current patch");
    for (const auto &answer : {conceptual, highGain, stereoOnly}) {
        expect(!answer.text.contains("PREAMP A está") && !answer.text.contains("Gain 80") && !answer.text.contains("Reverb ON")
            && !answer.text.contains("Channel Mode atual"), "no unsolicited patch dump");
        expect(answer.text.split("\n\n").size() <= 4, "compact response budget");
    }
    expect(!highGain.text.contains("Dual L/R") && highGain.metadata.value("toneProfile") == "neutral", "plain high gain does not force stereo or metal");
    expect(adaptedHighGain.metadata.value("patchAware").toBool() && adaptedHighGain.text.contains("Gain 80")
        && adaptedHighGain.text.contains("Channel Mode atual: Single") && adaptedHighGain.text.contains("compare esse modo"), "explicit adaptation uses real gain and routing");
    expect(stereoContext.root == stereoBefore && context.root == before, "all responses leave full context unchanged");
    for (const QString &q : {QString("Como montar um timbre clean para tocar na igreja?"), QString("Quero um clean worship")}) {
        const auto answer = ask(provider, q, context);
        for (const QString &leak : {QString("forro"), QString("neo soul"), QString("taxonomia"), QString("recipe"), QString("curated"), QString("curada"), QString("field knowledge"), QString("guidance")})
            expect(!ToneIntentParser::normalize(answer.text).contains(leak), "no internal knowledge leak " + leak);
        expect(answer.text.contains("leve saturação") && answer.text.contains("MATCH") && answer.text.contains("VO CLEAN"), "focused Worship direction retained");
        expect(answer.text.split("\n\n").size() <= 4, "Worship compact with loaded context");
    }
    const auto metalStereo = ask(provider, "Como fazer um high gain metal estéreo?");
    expect(metalStereo.metadata.value("toneProfile") == "metal" && metalStereo.metadata.value("toneGoal") == "wider"
        && metalStereo.text.contains("low-end") && metalStereo.text.contains("OUTPUT L/R"), "explicit metal stereo preserved");
    const auto leadStereo = ask(provider, "Como fazer um lead high gain estéreo?");
    expect(leadStereo.metadata.value("toneFamily") == "high_gain" && leadStereo.metadata.value("usageContext") == "lead"
        && leadStereo.metadata.value("usageRole") == "lead" && !leadStereo.text.contains("palm mute"), "lead changes high gain composition rather than a final aside");
    const auto darkEdge = ask(provider, "Quero edge of breakup mais escuro");
    expect(darkEdge.metadata.value("toneFamily") == "edge_of_breakup" && darkEdge.text.contains("Para escurecer"), "edge and darker composed");
    const auto sustainRock = ask(provider, "Quero um lead rock com mais sustain");
    expect(sustainRock.metadata.value("toneFamily") == "lead" && sustainRock.metadata.value("toneProfile") == "rock"
        && includes(sustainRock.metadata.value("toneModifiers").toArray(), "more_sustain"), "lead rock sustain qualifiers preserved");
    const auto humbuckerWorship = ask(provider, "Quero um clean worship com humbucker", context);
    expect(humbuckerWorship.metadata.value("guitarContext") == "humbucker" && humbuckerWorship.text.contains("humbucker")
        && !humbuckerWorship.metadata.value("patchAware").toBool(), "instrument qualifier without automatic patch use");
    const auto focusedEq = ask(provider, "Que frequências corto no worship?");
    expect(focusedEq.text.contains("110Hz") && !focusedEq.text.contains("Compressão") && !focusedEq.text.contains("Compare VO CLEAN"), "EQ question does not dump full recipe");
    const auto explicitMissing = ask(provider, "Como transformar meu patch em high gain estéreo?");
    expect(!explicitMissing.metadata.value("patchAware").toBool() && explicitMissing.text.contains("Não tenho dados suficientes"), "requested missing context not fabricated");
    const auto englishStereo = ask(provider, "How do I build a high gain stereo tone?", context, "en");
    expect(englishStereo.text.startsWith("To build a high gain truly stereo") && !englishStereo.text.contains("In the loaded patch"), "English composition/context policy");
    const auto weight = ask(provider, "Como abrir o estéreo sem perder peso?");
    expect(weight.metadata.value("preserveBody").toBool() && weight.text.contains("sem perder peso") && weight.text.contains("corpo nos médios"), "weight constraint retained with stereo");
    const auto detail = ask(provider, "Como montar um clean worship? Quero todas as opções.");
    expect(detail.metadata.value("responseDetail") == "detailed" && detail.text.contains("DELUX Crunch") && detail.text.contains("Tweed"), "full options only on explicit detail request");
    for (const QString &q : {QString("Adapte esse patch para high gain estéreo"), QString("Quero um high gain estéreo usando esse patch"), QString("Build a high gain stereo tone using my preset")})
        expect(ToneTaxonomy::parse(q).usePatchContext && ToneTaxonomy::parse(q).recognized, "explicit patch cues " + q);
    expect(!ToneTaxonomy::parse("Quero um high gain sem estéreo").modifiers.contains(ToneModifier::Wider), "negated stereo not added");
    const auto highGainBig = ask(provider, "Quero um high gain para soar grande");
    expect(highGainBig.text.startsWith("Para um timbre high gain") && highGainBig.text.contains("NS")
        && highGainBig.metadata.value("toneGoal") == "bigger", "big goal also preserves explicit family");
    const auto ampChoice = ask(provider, "Qual amp usar para clean worship?");
    expect(ampChoice.text.contains("TWEED") && !ampChoice.text.contains("DELUX") && !ampChoice.text.contains("Compressão"), "focused amp choice has three relevant models, no full recipe dump");
    const auto compound = ask(provider, "Quero um high gain estéreo para lead, mas sem ficar embolado.", context);
    expect(compound.metadata.value("toneFamily") == "high_gain" && compound.metadata.value("usageRole") == "lead"
        && compound.metadata.value("toneProblem") == "muddy" && compound.metadata.value("spatialGoal") == "stereo", "four dimensions composed");
    expect(compound.text.section("\n\n", 0, 0).contains("lead high gain")
        && compound.text.section("\n\n", 0, 0).contains("low-end"), "first paragraph combines role family problem spatial goal");
    expect(!compound.text.contains("palm mute") && !compound.text.contains("base pesada"), "lead has no rhythm-dominant guidance");
    expect(!compound.metadata.value("patchAware").toBool() && compound.metadata.value("toneFeatures").toArray().isEmpty(), "conceptual role problem does not inspect patch");
    expect(compound.text.split("\n\n").size() <= 4, "compound remains compact");
    bool muddyStep = false;
    for (const auto &v : compound.metadata.value("recipeSteps").toArray()) {
        const auto s = v.toObject();
        if (s.value("blockId") == "eq") muddyStep = s.value("guidance").toString().contains("overlap");
    }
    expect(muddyStep, "problem changes traced block guidance, not just final sentence");
    const auto rhythm = ask(provider, "Quero um high gain para base.");
    expect(rhythm.text.contains("palm mute") && rhythm.text.contains("Delay mínimo")
        && !rhythm.text.contains("mais presente para o lead"), "rhythm has distinct gain and space priorities");
    const auto neutral = ask(provider, "Como fazer um high gain?");
    expect(neutral.metadata.value("usageRole") == "neutral" && !neutral.text.contains("palm mute"), "neutral high gain never assumes rhythm");
    const auto harshArpeggio = ask(provider, "Quero um clean worship para arpejo, sem ficar estridente.");
    expect(harshArpeggio.text.split("\n\n").size() == 4 && harshArpeggio.text.contains("High Cut")
        && harshArpeggio.text.contains("Presence") && harshArpeggio.text.contains("leve saturação"), "Worship separates dynamics EQ space and treats harshness");
    const auto sustain = ask(provider, "Quero um lead com sustain sem aumentar muito o ganho.");
    expect(sustain.metadata.value("toneProblem") == "needs_sustain" && includes(sustain.metadata.value("toneModifiers").toArray(), "less_gain")
        && sustain.text.contains("OD/DS") && sustain.text.contains("COMP") && !sustain.text.contains("suba Gain"), "sustain honors gain constraint with alternatives");
    const auto ambient = ask(provider, "Quero um timbre ambient sem perder o ataque.");
    expect(ambient.text.contains("Pre Delay") && ambient.text.contains("Reverb Time") && ambient.text.contains("Feedback")
        && !ambient.text.contains("acrescente Delay/Reverb gradualmente"), "ambient attack changes delay and reverb guidance");
    const auto patchGoal = ask(provider, "Transforme meu patch atual em um lead high gain estéreo sem embolar.", context);
    expect(patchGoal.metadata.value("patchAware").toBool() && patchGoal.text.contains("Gain 80")
        && !patchGoal.text.contains("mais drive") && context.root == before, "explicit problem uses immutable context and does not blindly increase gain");
    const auto englishRole = ask(provider, "Build a high gain stereo lead without muddy lows", context, "en");
    expect(englishRole.metadata.value("toneProblem") == "muddy" && englishRole.text.contains("sustain")
        && !englishRole.text.contains("palm mute"), "English compound composition");
    for (const auto &pair : QList<QPair<QString,QString>>{{"solista","lead"},{"frases","lead"},{"ritmo","rhythm"},{"acompanhamento","rhythm"},{"dedilhado","arpeggio"},{"arpejada","arpeggio"},{"pad","ambient"},{"textura","ambient"}})
        expect(ToneTaxonomy::id(ToneTaxonomy::parse("Quero clean para " + pair.first).usageRole) == pair.second, "role alias " + pair.first);
    for (const auto &pair : QList<QPair<QString,QString>>{{"abafado demais","muddy"},{"sem clareza","muddy"},{"fritando","harsh"},{"áspero","harsh"},{"sem corpo","thin"},{"chiando","noisy"},{"notas se misturam","not_defined"},{"nota morre rápido","needs_sustain"},{"abrir os lados","needs_width"}})
        expect(ToneTaxonomy::id(ToneTaxonomy::parse("Quero um clean " + pair.first).toneProblem) == pair.second, "problem alias " + pair.first);
    const auto noisy = ask(provider, "Quero um high gain ruidoso.");
    expect(noisy.text.contains("NS") && noisy.text.contains("não atribuo causa elétrica"), "noise guidance scoped, no invented diagnosis");
    const auto thin = ask(provider, "Meu timbre está fino.");
    expect(thin.metadata.value("toneFamily") == "" && !thin.metadata.value("patchAware").toBool()
        && thin.text.contains("fase A/B"), "problem-only does not invent family or inspect patch");
    const auto cleanLead = ask(provider, "Quero um clean para lead.");
    expect(cleanLead.text.contains("Gain baixo") && !cleanLead.text.contains("ganho suficiente, não excessivo"), "role does not override explicit clean gain goal");
    for (const QString &q : {QString("Como montar um timbre Worship Lead estéreo?"), QString("Como montar um clean Worship?"),
            QString("Quero um high gain estéreo para lead, mas sem ficar embolado."), QString("Quero um crunch para base com mais definição."), QString("Como fazer um high gain?")}) {
        provider.resetConversation();
        const auto r = ask(provider, q);
        const auto counts = r.metadata.value("optionCountPerBlock").toObject();
        QTextStream(stdout) << "DENSITY " << q << " paragraphs=" << r.text.split("\n\n").size()
            << " characters=" << r.text.size() << " options=" << QJsonDocument(counts).toJson(QJsonDocument::Compact) << '\n';
        expect(r.text.split("\n\n").size() >= 3 && r.text.split("\n\n").size() <= 4, "compact paragraph budget " + q);
        expect(r.text.size() <= 1850 && r.text.size() >= 400, "soft compact size regression " + q);
        expect(r.metadata.value("responseCharacterCount").toInt() == r.text.size(), "metrics reflect final text");
        for (auto it = counts.constBegin(); it != counts.constEnd(); ++it)
            expect(it.value().toInt() <= (it.key() == "preampA" ? 3 : 2), "visible option budget " + it.key());
        expect(!r.text.contains("rationale") && !r.text.contains("taxonomy") && !r.text.contains("subsystem"), "natural response language");
        if (q.contains("crunch")) expect(!r.text.contains("Em high gain") && !r.text.contains("lead"), "crunch rhythm family isolation");
        if (q.contains("clean")) expect(!r.text.contains("metal") && !r.text.contains("5150"), "clean family isolation");
        if (q.contains("lead", Qt::CaseInsensitive)) expect(!r.text.contains("palm mute"), "lead does not inherit rhythm");
        if (q == "Como fazer um high gain?") expect(r.text.split("\n\n").size() == 3 && !r.text.contains("Dual L/R"), "neutral high gain concise no unrequested stereo");
    }
    provider.resetConversation();
    const auto brief = ask(provider, "Como montar um timbre Worship Lead estéreo?");
    const auto expanded = ask(provider, "Agora detalhe os parâmetros.");
    expect(expanded.metadata.value("buildFollowup").toBool() && expanded.metadata.value("responseDetail") == "detailed"
        && expanded.text.size() > brief.text.size() && expanded.metadata.value("toneFamily") == "lead", "detail followup preserves build goal");
    const auto againBrief = ask(provider, "Me dê uma configuração resumida.");
    expect(againBrief.metadata.value("buildFollowup").toBool() && againBrief.metadata.value("responseDetail") == "compact"
        && againBrief.text.size() < expanded.text.size(), "compact followup reuses goal");
    const auto values = ask(provider, "Me passe valores de partida.");
    expect(values.metadata.value("buildFollowup").toBool() && values.metadata.value("responseDetail") == "detailed"
        && values.metadata.value("readOnly").toBool(), "value followup expands without apply");
    expect(ask(provider, "Quero passo a passo.").metadata.value("responseDetail") == "detailed", "step-by-step followup reuses build goal");
    provider.resetConversation();
    expect(!ask(provider, "Agora detalhe os parâmetros.").metadata.value("buildFollowup").toBool(), "reset prevents stale build detail");
    ask(provider, "Como montar um timbre Worship Lead estéreo?", context);
    AIContext changedPatch = context;
    changedPatch.root.insert("patch", QJsonObject{{"number", "U02-1"}, {"name", "Different patch"}});
    expect(!ask(provider, "Agora detalhe os parâmetros.", changedPatch).metadata.value("buildFollowup").toBool(), "patch identity change invalidates remembered build");
    // Approved dataset guidance through the real provider, not only the store.
    provider.resetConversation();
    for (const QString &q : QStringList{"Qual Low Cut usar?", "110 Hz é obrigatório?", "Qual High Cut usar?", "8 kHz é obrigatório?", "Que frequência corto para tirar fizz?", "Como tirar grave embolado?", "100 Hz e 8 kHz é regra?"}) {
        const auto r = ask(provider, q);
        expect(r.metadata.value("route") == "eq_dataset_guidance" && r.metadata.value("readOnly").toBool(), "dataset EQ provider route " + q);
        const QString text = r.text.toLower();
        for (const QString &bad : QStringList{"sempre use 110 hz", "110 hz é o correto", "8 khz é obrigatório", "6 khz é errado", "todos os guitarristas usam"})
            expect(!text.contains(bad), "no absolute prescription " + q);
        expect(text.contains("referência") && text.contains("ajust"), "reference and context " + q);
        const auto trace = r.metadata.value("fieldKnowledge").toArray();
        expect(!trace.isEmpty(), "trace exists " + q);
        for (const auto &v : trace) {
            const auto t = v.toObject(); const bool low = t.value("candidateId") == "KC-15";
            expect(t.value("sourceKind") == "PresetReferenceDataset" && t.value("denominator") == 53
                && t.value("evidenceCount") == (low ? 42 : 29) && t.value("creatorCount") == (low ? 4 : 5)
                && t.value("confidence") == (low ? "StrongWithinDataset" : "ModerateWithinDataset"), "approved trace " + q);
            expect(!r.text.contains("KC-") && !r.text.contains("42/53") && !r.text.contains("29/53"), "trace not normal prose");
        }
        if (q.contains("High") || q.contains("8 kHz") || q.contains("fizz")) {
            expect(r.text.contains("8 kHz") && r.text.contains("6 kHz"), "high cut alternatives");
            expect(r.metadata.value("referenceOnly").toArray().first().toObject().value("promotionStatus") == "NotPromoted", "KC-17 remains reference only");
        } else expect(r.text.contains("110 Hz") && r.text.contains("55 Hz") && r.text.contains("FLAT"), "low cut alternatives");
    }
    for (const QString &q : QStringList{"Qual Low Cut usar?", "Qual High Cut usar?", "Como tirar fizz?"}) {
        const auto r = ask(provider, q, {}, "en");
        expect(r.text.contains("reference") && !r.text.contains("referência"), "English dataset guidance");
    }
    for (const QString &q : QStringList{"O que é Low Cut?", "Quais opções de High Cut existem?", "Qual High Cut usar no Reverb?", "Como montar um clean Worship?"})
        expect(ask(provider,q).metadata.value("route") != "eq_dataset_guidance", "preserve non-general-EQ path " + q);
    const auto entries = ToneStyleRecipes().fieldKnowledge();
    expect(entries.size() == 5, "three curated entries plus exactly two promotions");
    int promoted = 0;
    for (const auto &entry : entries) if (entry.validationType == FieldValidationType::ObservedPresetPattern) {
        ++promoted;
        expect(entry.id == "dataset_eq_low_cut_110" || entry.id == "dataset_eq_high_cut_8k", "only approved entries");
    }
    expect(promoted == 2, "exactly two dataset entries");
    QTextStream(stdout) << "Tone Taxonomy + Style Recipe tests: assertions=" << assertions << " failures=" << failures << '\n';
    return failures ? 1 : 0;
}
