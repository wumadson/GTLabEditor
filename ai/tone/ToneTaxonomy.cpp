#include "ai/tone/ToneTaxonomy.h"
#include "ai/tone/ToneIntentParser.h"

#include <QMap>

namespace Ai { namespace Tone {
namespace {
bool any(const QString &text, const QStringList &aliases)
{
    const QString padded = " " + text + " ";
    for (const QString &alias : aliases) {
        if (padded.contains(" " + alias + " "))
            return true;
    }
    return false;
}
QString canonical(const QString &text)
{
    QStringList words = ToneIntentParser::normalize(text).split(' ');
    // Only approved near-spellings; no broad edit-distance guessing.
    const QMap<QString, QString> typos = {
        {"worhsip", "worship"}, {"worshp", "worship"},
        {"clin", "clean"}, {"cranch", "crunch"}, {"neosoul", "neo soul"}
    };
    for (QString &word : words)
        word = typos.value(word, word);
    return words.join(' ');
}
}

QString ToneTaxonomy::id(ToneFamily v)
{
    return families().value(static_cast<int>(v));
}
QString ToneTaxonomy::id(ToneProfile v)
{
    return profiles().value(static_cast<int>(v));
}
QString ToneTaxonomy::id(ToneModifier v)
{
    return modifiers().value(static_cast<int>(v));
}
QString ToneTaxonomy::id(ToneUsageRole v)
{
    return QStringList{"neutral", "rhythm", "lead", "arpeggio", "ambient"}.value(static_cast<int>(v));
}
QString ToneTaxonomy::id(ToneProblem v)
{
    return QStringList{"none", "muddy", "harsh", "thin", "noisy", "not_defined", "needs_sustain", "needs_width"}.value(static_cast<int>(v));
}
QStringList ToneTaxonomy::families()
{
    return {"clean", "edge_of_breakup", "crunch", "high_gain", "ambient", "lead"};
}
QStringList ToneTaxonomy::profiles()
{
    return {"neutral", "worship", "forro", "rock", "metal", "neo_soul", "pop"};
}
QStringList ToneTaxonomy::modifiers()
{
    return {"brighter", "darker", "warmer", "more_defined", "less_muddy",
        "more_dynamic", "more_compressed", "more_ambient", "less_ambient",
        "wider", "more_focused", "more_aggressive", "more_smooth",
        "more_sustain", "less_gain", "more_gain"};
}

ToneStyleRequest ToneTaxonomy::parse(const QString &text)
{
    ToneStyleRequest result;
    const QString n = canonical(text);
    const auto family = [&](ToneFamily v, const QStringList &aliases) {
        if (!result.explicitFamily && any(n, aliases)) {
            result.family = v;
            result.explicitFamily = true;
        }
    };
    family(ToneFamily::EdgeOfBreakup, {"edge of breakup", "edge of break up", "comecando a quebrar"});
    family(ToneFamily::HighGain, {"high gain", "muito ganho"});
    family(ToneFamily::Clean, {"clean", "limpo", "cristalino", "sem drive", "limpinho"});
    family(ToneFamily::Crunch, {"crunch"});
    family(ToneFamily::Ambient, {"ambient", "ambiente", "ambiental"});
    family(ToneFamily::Lead, {"lead", "solo"});
    // Solo is a lead synonym, not a separate composition policy.
    if (any(n, {"lead", "solo", "solista", "frase", "frases"})) result.usageRole = ToneUsageRole::Lead;
    else if (any(n, {"arpejo", "arpejada", "dedilhado", "arpeggio", "fingerpicked"})) result.usageRole = ToneUsageRole::Arpeggio;
    else if (any(n, {"base", "ritmo", "ritmica", "acompanhamento", "rhythm"})) result.usageRole = ToneUsageRole::Rhythm;
    else if (any(n, {"ambient", "ambiencia", "pad", "textura", "texture"})) result.usageRole = ToneUsageRole::Ambient;
    if (result.usageRole != ToneUsageRole::Neutral) result.usageContext = id(result.usageRole);
    const auto problem = [&](ToneProblem v, const QStringList &aliases) {
        if (result.toneProblem == ToneProblem::None && any(n, aliases)) result.toneProblem = v;
    };
    problem(ToneProblem::Muddy, {"embolado", "embolando", "embolar", "abafado demais", "grave embolado", "sem clareza", "muddy"});
    problem(ToneProblem::Harsh, {"estridente", "aspero", "agudo demais", "fizz", "fritando", "harsh"});
    problem(ToneProblem::Thin, {"magro", "sem corpo", "fino", "thin"});
    problem(ToneProblem::Noisy, {"ruidoso", "chiando", "muito ruido", "noisy"});
    problem(ToneProblem::NotDefined, {"sem definicao", "perde definicao", "notas se misturam", "mais definicao", "sem perder o ataque", "sem perder ataque", "more definition", "without losing attack"});
    problem(ToneProblem::NeedsSustain, {"sem sustain", "mais sustain", "com sustain", "nota morre rapido", "more sustain", "with sustain"});
    problem(ToneProblem::NeedsWidth, {"mais aberto", "mais largo", "mais estereo", "abrir os lados", "wider"});
    const auto profile = [&](ToneProfile v, const QStringList &aliases) {
        if (!result.explicitProfile && any(n, aliases)) {
            result.profile = v;
            result.explicitProfile = true;
        }
    };
    profile(ToneProfile::NeoSoul, {"neo soul", "new soul", "groove", "r b", "soul moderno"});
    profile(ToneProfile::Forro, {"forro", "xote", "baiao", "piseiro"});
    profile(ToneProfile::Worship, {"worship", "louvor", "adoracao", "igreja"});
    profile(ToneProfile::Metal, {"metal"});
    profile(ToneProfile::Rock, {"rock", "rock classico", "hard rock"});
    profile(ToneProfile::Pop, {"pop"});
    if (!result.explicitFamily && result.profile == ToneProfile::Metal)
        result.family = ToneFamily::HighGain;
    if (!result.explicitFamily && any(n, {"pesado", "heavy"})) {
        result.needsClarification = !result.explicitProfile;
        if (result.profile == ToneProfile::Rock) result.family = ToneFamily::Crunch;
    }

    const QStringList modifierAliases[] = {
        {"mais brilhante", "mais brilho", "brighter"},
        {"mais escuro", "darker"}, {"mais quente", "mais corpo", "warmer"},
        {"mais definido", "mais definicao", "more defined"},
        {"menos embolado", "less muddy"}, {"mais dinamico", "more dynamic"},
        {"mais comprimido", "more compressed"}, {"mais ambiente", "more ambient"},
        {"menos ambiente", "less ambient"}, {"mais amplo", "mais largo", "wider"},
        {"mais focado", "more focused"}, {"mais agressivo", "more aggressive"},
        {"mais suave", "more smooth"}, {"mais sustain", "more sustain"},
        {"menos ganho", "less gain"}, {"mais ganho", "more gain"}
    };
    for (int i = 0; i < 16; ++i) {
        if (any(n, modifierAliases[i]))
            result.modifiers.append(static_cast<ToneModifier>(i));
    }
    if (any(n, {"sem aumentar muito o ganho", "sem aumentar ganho", "without adding gain"})
        && !result.modifiers.contains(ToneModifier::LessGain)) result.modifiers.append(ToneModifier::LessGain);
    if (any(n, {"soar grande", "som grande", "sound big", "timbre grande"}))
        result.spatialGoal = ToneSpatialGoal::Big;
    if (any(n, {"stereo", "estereo", "nos dois lados", "both sides"})
        && !any(n, {"sem estereo", "without stereo"}))
        result.spatialGoal = ToneSpatialGoal::Stereo;
    if (any(n, {"soar moderno", "som moderno", "sound modern"}))
        result.spatialGoal = ToneSpatialGoal::Modern;
    if (result.spatialGoal == ToneSpatialGoal::Stereo) {
        if (!result.modifiers.contains(ToneModifier::Wider))
            result.modifiers.append(ToneModifier::Wider);
    }
    if (any(n, {"strato", "strat", "single coil", "single coils"}))
        result.guitarContext = "single_coil";
    if (any(n, {"humbucker", "humbuckers"}))
        result.guitarContext = "humbucker";
    result.wantsValues = any(n, {"valores", "values", "ranges"});
    result.wantsDetail = result.wantsValues || any(n, {"passo a passo", "step by step", "parametros", "parameters", "detalhamento", "details", "todas as opcoes", "quais opcoes", "liste todos", "all options", "list all"});
    if (any(n, {"configuracao resumida", "resumido", "resumida", "keep it brief"})) {
        result.wantsDetail = false;
        result.wantsValues = false;
    }
    result.usePatchContext = any(n, {"nesse patch", "neste patch", "esse patch", "meu patch", "patch atual",
        "nesse preset", "neste preset", "esse preset", "meu preset", "preset atual", "o que estou usando",
        "transforme esse", "adapte esse", "melhore esse", "my patch", "my preset", "this patch", "this preset", "current patch", "current preset"})
        && !any(n, {"sem usar o patch", "sem usar meu patch", "without using my patch"});
    result.preserveBody = any(n, {"sem perder peso", "sem perder corpo", "without losing weight", "without losing body"});
    const bool modelChoice = any(n, {"qual drive", "qual amplificador", "qual amp", "qual preamp",
        "qual reverb", "qual delay", "which drive", "which amp", "which reverb", "which delay"})
        && any(n, {"usar", "use", "melhor", "best"});
    if (modelChoice) {
        if (any(n, {"drive"})) result.focusBlock = "odDs";
        else if (any(n, {"reverb"})) result.focusBlock = "reverb";
        else if (any(n, {"delay"})) result.focusBlock = "delay";
        else result.focusBlock = "preampA";
    }
    const bool buildCue = any(n, {"montar", "fazer", "criar", "transformar", "transforme", "adapte", "melhore", "deixar", "quero", "build", "create", "make", "want", "adapt", "transform"});
    const bool problemStatement = result.toneProblem != ToneProblem::None
        && any(n, {"meu timbre esta", "my tone is"});
    const bool styleCue = result.explicitFamily || result.explicitProfile || problemStatement
        || result.needsClarification || result.spatialGoal != ToneSpatialGoal::None;
    // Queries about facts, comparisons and existing fine adjustments keep
    // their established routes. A style alone is also a useful direct request.
    const bool adjustment = any(n, {"mais crunch", "mais clean", "mais limpo", "mais ambiente", "more crunch", "more clean"});
    const bool technical = any(n, {"diferenca", "compare", "difference", "explicar", "explique", "explain", "tem", "possui", "available"});
    const bool followup = any(n, {"preservar", "preserve", "manter", "keep", "prefiro", "prefer"})
        && !any(n, {"montar", "criar", "transformar", "build", "create"});
    // These questions concern the curated musical recipe, not whether the
    // hardware contains an effect. Keep this exception deliberately narrow.
    const bool worshipRecipeQuestion = result.profile == ToneProfile::Worship
        && any(n, {"leve saturacao", "totalmente limpo", "posso usar", "frequencias corto", "worship eq"});
    if (worshipRecipeQuestion && any(n, {"posso usar"}) && any(n, {"pouco ganho"})) {
        result.family = ToneFamily::Clean;
        result.explicitFamily = true;
    }
    const bool eqReferenceQuestion = any(n, {"100 hz e 8 khz"}) && any(n, {"obrigatorio", "mandatory"});
    if (eqReferenceQuestion) {
        result.profile = ToneProfile::Worship;
        result.explicitProfile = true;
    }
    const bool sizeComparison = any(n, {"diferenca entre soar grande e soar estereo", "difference between big and stereo"});
    if (worshipRecipeQuestion || eqReferenceQuestion || sizeComparison) {
        if (worshipRecipeQuestion && any(n, {"posso usar"})) result.focusBlock = "preampA";
        if (any(n, {"frequencias corto", "worship eq"}) || eqReferenceQuestion) result.focusBlock = "eq";
        if (sizeComparison) result.focusBlock = "sizeComparison";
        result.recognized = true;
        return result;
    }
    result.recognized = styleCue && !technical && !followup
        && (problemStatement || modelChoice || result.spatialGoal != ToneSpatialGoal::None
            || (buildCue && (!adjustment || any(n, {"montar", "criar", "transformar", "build", "create"})))
            || n == "clean" || n == "crunch" || n == "edge of breakup"
            || n == "clean worship" || n == "clean forro" || n == "clean neo soul");
    return result;
}

} }
