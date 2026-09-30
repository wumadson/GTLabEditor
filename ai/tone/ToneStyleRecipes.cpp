#include "ai/tone/ToneStyleRecipes.h"

#include "ai/tone/ToneAnalyzer.h"
#include "ai/tone/ToneAdjustmentAdvisor.h"
#include "ai/tone/ToneIntentParser.h"
#include "ai/tone/ToneRules.h"
#include "ai/knowledge/DocumentationKnowledgeSource.h"
#include "ai/PatchParameterCatalog.h"
#include "MidiTable.h"

#include <QJsonArray>

namespace Ai { namespace Tone {
namespace {
QString choose(bool pt, const char *ptBr, const char *en)
{
    return QString::fromUtf8(pt ? ptBr : en);
}
QJsonObject effect(const AIContext &context, const QString &id)
{
    for (const QJsonValue &value : context.root.value("effects").toArray()) {
        const QJsonObject object = value.toObject();
        if (object.value("id").toString() == id) return object;
    }
    return {};
}
QString valueText(const QJsonValue &value)
{
    return value.isDouble() ? QString::number(value.toDouble()) : value.toString();
}
bool exactType(const Knowledge::GT10CapabilityService &service,
               const QString &block, const QString &type)
{
    return service.effectTypesForBlock(block).contains(type, Qt::CaseInsensitive);
}
QJsonArray eqCutCapabilities()
{
    QJsonArray result;
    // Both enum nodes are labelled "Cut Off" in MidiTable. Resolve the
    // distinct low/high identities through the existing parameter descriptors,
    // not a fuzzy label lookup or a second address table.
    for (const EffectDescriptor &block : PatchParameterCatalog::effects({})) {
        if (block.id != "eq") continue;
        for (const ParameterDescriptor &p : block.parameters) {
            if (p.id != "lowCut" && p.id != "highCut") continue;
            Midi node;
            if (!MidiTable::Instance()->lookup("Structure",
                {p.address.block.left(2), p.address.block.mid(2), p.address.offset}, &node)) continue;
            QStringList values;
            for (const Midi &v : node.level) if (!v.name.isEmpty()) values.append(v.name);
            result.append(QJsonObject{{"blockId", block.id}, {"parameterId", p.id},
                {"tableLabel", node.customdesc}, {"unit", "Hz/kHz"},
                {"enumValues", QJsonArray::fromStringList(values)}, {"source", "CapabilityService"}});
        }
    }
    return result;
}
ToneRecipeStep step(const char *block, const char *parameter,
                    const QStringList &types, const char *pt, const char *en,
                    bool optional = false)
{
    ToneRecipeStep result;
    result.blockId = block;
    result.parameterId = parameter;
    result.compatibleTypes = types;
    result.ptBr = QString::fromUtf8(pt);
    result.en = QString::fromUtf8(en);
    result.optional = optional;
    result.rationale = {result.ptBr, result.en, {"ToneRule", "Guidance"}};
    return result;
}
ToneRecipe base(ToneFamily family)
{
    ToneRecipe r;
    r.family = family;
    r.preferredBlocks = QStringList{"preampA", "eq"};
    r.optionalBlocks = QStringList{"comp", "odDs", "delay", "reverb", "chorus"};
    r.avoid = QStringList{"excess_gain", "masked_attack"};
    r.rationale.sources = QStringList{"ToneProfile", "ToneRule", "Guidance"};
    switch (family) {
    case ToneFamily::Clean:
        r.goals = QStringList{"definition", "low_gain", "dynamics", "controlled_ambience"};
        r.rationale.ptBr = "Para um clean neutro, mantenha a guitarra e a execução em primeiro plano, com Gain baixo e dinâmica preservada.";
        r.rationale.en = "For a neutral clean, keep the guitar and playing in front with low Gain and retained dynamics.";
        r.blockGuidance.append(step("preampA", "gain", {"(JC Clean) JC-120", "(TW Clean) Clean Twin"},
            "Comece por uma base limpa, com Gain baixo; compare os modelos mantendo o volume percebido semelhante. O ataque forte deve continuar limpo.",
            "Start with a clean base and low Gain; compare models at similar perceived volume. A strong attack should stay clean."));
        break;
    case ToneFamily::EdgeOfBreakup:
        r.goals = QStringList{"touch_sensitive", "mid_presence", "controlled_low_end"};
        r.rationale.ptBr = "Edge of breakup fica na fronteira do clean: toque leve limpo e ataque forte começando a saturar, sem virar um crunch constante.";
        r.rationale.en = "Edge of breakup sits at the clean boundary: light playing stays clean and stronger attacks begin to saturate, without constant crunch.";
        r.blockGuidance.append(step("preampA", "gain", {"(TW Clean) Tweed", "(Combo) VO Drive", "(Match) MATCH Drive"},
            "Suba o Gain aos poucos até essa fronteira, mantenha médios presentes e compare com o volume da guitarra reduzido. OD/DS é um segundo estágio opcional, não a base obrigatória.",
            "Raise Gain gradually to that boundary, retain mids, and compare with the guitar volume backed off. OD/DS is an optional second stage, not a required base."));
        break;
    case ToneFamily::Crunch:
        r.goals = QStringList{"chord_definition", "mid_presence", "moderate_gain"};
        r.rationale.ptBr = "Para crunch, procure a textura do ataque e médios firmes, não a quantidade máxima de distorção.";
        r.rationale.en = "For crunch, aim for attack texture and firm mids rather than maximum distortion.";
        r.blockGuidance.append(step("preampA", "gain", {"(Crunch) Boss Crunch", "(MS Classic)MS1959 I", "(Combo) VO Drive"},
            "Use Gain moderado e compare acordes abertos com palm mute. Reduza ganho se as notas se fundirem; OD/DS pode acrescentar um estágio de lead, mas não precisa ficar sempre ligado.",
            "Use moderate Gain and compare open chords with palm mutes. Back off if notes merge; OD/DS may add a lead stage but need not stay enabled."));
        break;
    case ToneFamily::HighGain:
        r.goals = QStringList{"note_definition", "controlled_low_end", "noise_control"};
        r.rationale.ptBr = "Para high gain, comece com saturação suficiente, definição da nota e médios audíveis. O Gain necessário depende da execução; base e lead pedem ajustes diferentes, não Gain no máximo.";
        r.rationale.en = "For high gain, start with sufficient saturation, note definition, and audible mids. The Gain needed depends on playing; rhythm and lead need different adjustments, not maximum Gain.";
        r.blockGuidance.append(step("preampA", "gain", {"(R-Fier) R-Fier Modern", "(MS Modern) MS HiGain", "(Metal) 5150 Drive"},
            "Compare bases de alto ganho com volume semelhante; use apenas o Gain necessário. OD/DS de baixo Drive antes do PREAMP pode apertar a entrada. Ajuste NS ao ruído real, sem cortar finais de notas.",
            "Compare high-gain bases at similar volume and use only the Gain needed. Low-Drive OD/DS before PREAMP may tighten the input. Set NS for actual noise without cutting note tails."));
        break;
    case ToneFamily::Ambient:
        r.goals = QStringList{"depth", "rhythmic_space", "intelligible_attack"};
        r.rationale.ptBr = "Ambient prioriza profundidade e movimento, mas o ataque ainda precisa localizar a frase dentro da cauda.";
        r.rationale.en = "Ambient prioritizes depth and movement, while the attack still needs to locate the phrase within the tail.";
        r.blockGuidance.append(step("preampA", "gain", {"(JC Clean) JC-120", "(TW Clean) Clean Twin"},
            "Uma base limpa ou quase limpa deixa as repetições legíveis; evite empilhar saturação em cada eco.",
            "A clean or nearly clean base keeps repeats legible; avoid stacking saturation into every echo."));
        break;
    case ToneFamily::Lead:
        r.goals = QStringList{"sustain", "mid_presence", "articulation"};
        r.rationale.ptBr = "Para um lead cantado, sustain e médios ajudam a frase a aparecer; mais ganho sozinho não garante presença no conjunto.";
        r.rationale.en = "For a singing lead, sustain and mids help the phrase stand out; extra gain alone does not guarantee presence in a mix.";
        r.blockGuidance.append(step("preampA", "gain", {"(BG Lead) BG Lead", "(Match) MATCH Lead", "(MS Classic)MS1959 I"},
            "Use Gain suficiente para sustentar a nota, com médios presentes. OD/DS opcional pode acrescentar sustain; equilibre Effect Level para não confundir volume com saturação.",
            "Use enough Gain to sustain notes while retaining mids. Optional OD/DS may add sustain; balance Effect Level so volume is not mistaken for saturation."));
        break;
    }
    r.blockGuidance.append(step("comp", "sustain", {"Comp"},
        "COMP é opcional: ajuste Sustain ouvindo a diferença entre notas leves e fortes; preserve dinâmica e ataque e evite aumentar ruído ou comprimir uma base já saturada.",
        "COMP is optional: set Sustain while comparing soft and strong notes; retain attack and avoid raising noise or compressing an already saturated base.", true));
    r.blockGuidance.append(step("eq", "lowGain", {},
        "Separe excesso de corpo de grave abaixo da região útil: Low Gain atua no balanço, Low Cut pode atenuar o grave que só ocupa espaço. Para aspereza/fizz, confira High Cut e High Gain sem apagar a palhetada; os cortes dependem da guitarra, captador e monitoração.",
        "Separate excess body from lows below the useful region: Low Gain shapes balance; Low Cut can attenuate lows that only occupy space. For harshness/fizz, check High Cut and High Gain without erasing picking; cuts depend on guitar, pickup, and monitoring."));
    r.blockGuidance.append(step("delay", "effectLevel", {"Single", "Analog", "DualL/R"},
        "Delay, Reverb e modulação são opções: comece com espaço discreto e verifique se a próxima nota continua clara. A base também funciona sem esses blocos.",
        "Delay, Reverb, and modulation are options: start with modest space and check that the next note stays clear. The base also works without these blocks.", true));
    r.blockGuidance.append(step("reverb", "effectLevel", {"Room", "Plate"},
        "Use uma cauda curta ou moderada; mais Effect Level traz profundidade, mas pode afastar a guitarra e esconder o ataque.",
        "Use a short or moderate tail; more Effect Level adds depth but may push the guitar back and hide attack.", true));
    if (family == ToneFamily::HighGain) {
        r.blockGuidance[2] = step("eq", "lowCut", {},
            "Controle o low-end com Low Cut/Bass sem tirar peso, mantenha médios audíveis e Presence/agudos sem fizz. EQ antes do PREAMP pode moldar a entrada; depois, corrigir o balanço final. Ajuste à guitarra, captador e monitoração.",
            "Control low-end with Low Cut/Bass without losing weight, retain audible mids and Presence/highs without fizz. EQ before PREAMP can shape the input; after, correct final balance. Fit guitar, pickup, and monitoring.");
        r.blockGuidance[1] = step("comp", "sustain", {},
            "A saturação já comprime: COMP não é prioridade no high gain; comece sem ele e cuide do ruído e dos finais das notas.",
            "Saturation already compresses: COMP is not a priority for high gain; start without it and manage noise and note tails.", true);
        r.blockGuidance[3] = step("delay", "effectLevel", {"Single"},
            "Delay é opcional: ajuste Effect Level e Feedback à frase. Se for base, use mínimo ou desligado; se for lead, repetições moderadas podem apoiar o sustain.",
            "Delay is optional: fit Effect Level and Feedback to the phrase. For rhythm, use minimal or off; for lead, moderate repeats can support sustain.", true);
        r.blockGuidance[4] = step("reverb", "effectLevel", {"Room"},
            "Escolha a profundidade do Reverb depois de definir o uso: curta para pausas claras, mais presente para frase sustentada, sem esconder o ataque.",
            "Choose Reverb depth after defining the role: short for clear rests, more present for sustained phrases without masking attack.", true);
    }
    return r;
}
void replace(ToneRecipe *recipe, const ToneRecipeStep &replacement)
{
    for (ToneRecipeStep &s : recipe->blockGuidance) {
        if (s.blockId == replacement.blockId) { s = replacement; return; }
    }
    recipe->blockGuidance.append(replacement);
}
// Refine the selected recipe before rendering and tracing it. Role and problem
// are composition dimensions, not a second router or an appended disclaimer.
void composeUsage(ToneRecipe *r, const ToneStyleRequest &s)
{
    const bool lead = s.usageRole == ToneUsageRole::Lead;
    const bool rhythm = s.usageRole == ToneUsageRole::Rhythm;
    if (lead) {
        r->goals = QStringList{"sustain", "mid_presence", "note_definition"};
        if (s.family != ToneFamily::Clean && s.family != ToneFamily::EdgeOfBreakup)
            replace(r, step("preampA", "gain", r->blockGuidance.first().compatibleTypes,
            "Para o lead, use ganho suficiente, não excessivo, e médios presentes para sustain com definição da nota. Compare amps com level balance; a compressão natural da saturação já sustenta a frase. OD/DS como boost de Drive baixo é opcional, não uma obrigação de aumentar Gain.",
            "For lead, use sufficient rather than excessive gain and present mids for sustain with note definition. Compare amps at balanced levels; saturation's natural compression already supports phrases. Low-Drive OD/DS boost is optional, not a requirement to raise Gain."));
        replace(r, step("delay", "effectLevel", {"Single", "DualL/R"},
            "Delay com nível e Feedback moderados apoia o sustain perceptivo sem misturar notas; ajuste as repetições à frase, não apenas ao tamanho da cauda.",
            "Delay with moderate level and Feedback supports perceived sustain without merging notes; fit repeats to the phrase, not just tail length.", true));
        replace(r, step("reverb", "effectLevel", {"Plate", "Hall1"},
            "Reverb pode ser mais presente para o lead: equilibre Reverb Time e Effect Level para criar profundidade sem lavar o ataque.",
            "Reverb can be more present for lead: balance Reverb Time and Effect Level for depth without washing out attack.", true));
    } else if (rhythm) {
        r->goals = QStringList{"chord_definition", "controlled_low_end", "clear_rests"};
        if (s.family != ToneFamily::Clean && s.family != ToneFamily::EdgeOfBreakup)
            replace(r, step("preampA", "gain", r->blockGuidance.first().compatibleTypes,
            "Para base, use Gain moderado/apenas suficiente para preservar acordes, palm mute e pausas definidas. Mantenha médios úteis e low-end apertado; OD/DS com Drive baixo pode apertar a entrada, mas é opcional.",
            "For rhythm, use moderate/only sufficient Gain to retain chords, palm mutes, and clear rests. Keep useful mids and tight low-end; low-Drive OD/DS can tighten input but is optional."));
        if (s.family == ToneFamily::HighGain) {
            r->blockGuidance.first().ptBr += " Ajuste NS ao ruído real sem cortar finais de notas.";
            r->blockGuidance.first().en += " Fit NS to actual noise without cutting note tails.";
        }
        replace(r, step("delay", "effectLevel", {"Single"},
            "Na base, deixe Delay mínimo ou desligado; Feedback baixo evita acumular repetições nas pausas.",
            "For rhythm, leave Delay minimal or off; low Feedback avoids repeats building up in rests.", true));
        replace(r, step("reverb", "effectLevel", {"Room"},
            "Reverb curto e discreto mantém os acordes e pausas legíveis, sem cauda de lead como padrão.",
            "Short, modest Reverb keeps chords and rests legible, without a lead tail by default.", true));
    } else if (s.usageRole == ToneUsageRole::Arpeggio) {
        r->goals = QStringList{"clear_attack", "note_separation", "rhythmic_space"};
        replace(r, step("comp", "attack", {"Comp"},
            "No arpejo, preserve o ataque claro e a separação das notas. COMP leve é opcional: ajuste Attack e Sustain sem apagar a dinâmica do dedilhado.",
            "For arpeggios, retain clear attack and note separation. Light COMP is optional: set Attack and Sustain without erasing fingerpicking dynamics.", true));
        replace(r, step("delay", "feedback", {"Single", "DualL/R"},
            "Use Delay rítmico musical com Feedback e Effect Level contidos para deixar cada nota do arpejo aparecer.",
            "Use musical rhythmic Delay with restrained Feedback and Effect Level so each arpeggio note remains audible.", true));
        replace(r, step("reverb", "effectLevel", {"Plate", "Hall1"},
            "Reverb acrescenta profundidade atrás do arpejo; limite Reverb Time e nível para não cobrir sua definição.",
            "Reverb adds depth behind arpeggios; limit Reverb Time and level so definition is not masked.", true));
    }
    QString eqPt, eqEn;
    switch (s.toneProblem) {
    case ToneProblem::Muddy:
        eqPt = "Para evitar embolamento sem perder sustain, controle Low Cut/Bass/Low Gain e excesso de Gain, preservando médios úteis. EQ antes molda a entrada; depois corrige o balanço. Não duplique low-end em A/B: equilibre níveis e confira overlap na soma mono.";
        eqEn = "To avoid muddiness without losing sustain, control Low Cut/Bass/Low Gain and excessive Gain while retaining useful mids. EQ before shapes input; after corrects balance. Do not duplicate A/B low-end: balance levels and check overlap in the mono sum.";
        break;
    case ToneProblem::Harsh:
        eqPt = "Para conter estridência/fizz, compare Presence, Treble/High Gain e High Cut sem apagar o ataque. Confira voicing do amp/cab, guitarra/captador e monitoração; corte agudos das repetições se a aspereza vier da ambiência.";
        eqEn = "To tame harshness/fizz, compare Presence, Treble/High Gain, and High Cut without erasing attack. Check amp/cab voicing, guitar/pickup, and monitoring; cut repeat highs if ambience contributes harshness.";
        break;
    case ToneProblem::Thin:
        eqPt = "Para recuperar corpo, trabalhe médios e voicing do amp antes de reforçar Bass. Confira Low Cut excessivo, ganho insuficiente e level balance; se usar estéreo, teste soma mono e desequilíbrio/fase A/B. São possibilidades a comparar, não diagnóstico de áudio.";
        eqEn = "To recover body, work mids and amp voicing before boosting Bass. Check excessive Low Cut, insufficient gain, and level balance; with stereo, test the mono sum and A/B phase/imbalance. These are comparisons, not an audio diagnosis.";
        break;
    case ToneProblem::NotDefined:
        eqPt = "Para mais definição, preserve o ataque e médios úteis, controle Bass/Low Gain e recue excesso de Gain se as notas se misturam. Use Low Cut apenas onde houver grave inútil, sem afinar acordes.";
        eqEn = "For more definition, retain attack and useful mids, control Bass/Low Gain, and back off excessive Gain if notes merge. Use Low Cut only for useless lows without thinning chords.";
        break;
    case ToneProblem::NeedsSustain:
        eqPt = "Para sustain sem depender de mais Gain, preserve médios audíveis e equilibre os níveis. Compare OD/DS como boost de Drive baixo e COMP quando apropriado; evite comprimir duas vezes uma base já saturada.";
        eqEn = "For sustain without relying on more Gain, retain audible mids and balance levels. Compare low-Drive OD/DS boost and COMP when appropriate; avoid compressing an already saturated base twice.";
        break;
    case ToneProblem::Noisy:
        eqPt = "Para ruído, evite somar Gain e COMP desnecessários; compare cada estágio com volume equilibrado. Ajuste NS ao ruído observado sem cortar sustain ou finais de notas. Sem ouvir o sinal, não atribuo causa elétrica nem proponho reparo.";
        eqEn = "For noise, avoid unnecessary Gain and COMP stacking; compare stages at balanced volume. Set NS for observed noise without cutting sustain or note tails. Without hearing the signal, I do not infer electrical causes or repairs.";
        break;
    case ToneProblem::NeedsWidth:
        eqPt = "Para mais largura, preserve o centro da nota e level balance. Compare pequenas diferenças A/B e Delay Dual L/R; Chorus Stereo1/Stereo2 é opcional. Confira OUTPUT L/R, monitoração estéreo real e compatibilidade mono, não apenas mais Bass.";
        eqEn = "For more width, retain the note center and level balance. Compare small A/B differences and Dual L/R Delay; Stereo1/Stereo2 Chorus is optional. Check OUTPUT L/R, actual stereo monitoring, and mono compatibility, not just more Bass.";
        break;
    case ToneProblem::None: break;
    }
    if (!eqPt.isEmpty()) {
        replace(r, step("eq", "lowGain", {}, eqPt.toUtf8().constData(), eqEn.toUtf8().constData()));
        r->goals.append(ToneTaxonomy::id(s.toneProblem));
    }
    for (ToneRecipeStep &b : r->blockGuidance) {
        if (s.toneProblem == ToneProblem::Muddy && b.blockId == "delay") {
            b.ptBr += " Reduza Feedback/Effect Level se os ecos acumularem grave entre notas.";
            b.en += " Reduce Feedback/Effect Level if echoes accumulate lows between notes.";
        }
        if (s.toneProblem == ToneProblem::Muddy && b.blockId == "reverb") {
            b.ptBr += " Recue Reverb Time (decay) e Effect Level se a cauda encobrir a próxima nota.";
            b.en += " Back off Reverb Time (decay) and Effect Level if tails mask the next note.";
        }
        if (s.toneProblem == ToneProblem::Harsh && b.blockId == "delay") {
            b.ptBr += " Confira High Cut se as repetições trouxerem agudos ásperos.";
            b.en += " Check High Cut if repeats bring harsh highs.";
        }
    }
    if (s.family == ToneFamily::Ambient && s.toneProblem == ToneProblem::NotDefined) {
        replace(r, step("delay", "feedback", {"Single", "DualL/R"},
            "Mantenha a entrada da nota perceptível: recue Feedback e Effect Level do Delay quando as repetições cobrirem o ataque; High Cut pode separar a cauda da nota direta.",
            "Keep note onset audible: back off Delay Feedback and Effect Level if repeats cover attack; High Cut can separate tails from the direct note.", true));
        replace(r, step("reverb", "preDelay", {"Hall1", "Plate"},
            "Use Pre Delay para separar ataque e cauda. Equilibre Reverb Time (decay), Effect Level e High Cut: mais Reverb não resolve definição, e a próxima nota precisa continuar visível.",
            "Use Pre Delay to separate attack and tail. Balance Reverb Time (decay), Effect Level, and High Cut: more Reverb does not solve definition, and the next note must remain audible.", true));
    }
}
ToneRecipe styled(ToneFamily family, ToneProfile profile)
{
    ToneRecipe r = base(family);
    r.profile = profile;
    if (profile == ToneProfile::Forro) {
        r.goals = QStringList{"present_clean", "clear_attack", "body_without_boom", "short_ambience"};
        r.avoid.append("long_reverb");
        r.rationale.ptBr = "Para um clean de forró, priorize presença e ataque claro: a levada precisa recortar o conjunto sem virar um som fino.";
        r.rationale.en = "For a forró clean, prioritize presence and clear attack: the rhythm should cut through without becoming thin.";
        replace(&r, step("comp", "sustain", {"Comp"},
            "Use COMP apenas para nivelar a levada; preserve a diferença entre palhetadas e ajuste Attack ouvindo se o início da nota continua nítido.",
            "Use COMP only to even out the rhythm; retain picking differences and set Attack while checking note onset.", true));
        replace(&r, step("eq", "lowGain", {},
            "Dê corpo com médios sem disputar o baixo e a zabumba. Se há grave abaixo da região útil, experimente Low Cut, não só reduzir Bass/Low Gain; controle agudos/fizz com High Cut sem perder o ataque. Ajuste à guitarra, captador e monitoração.",
            "Build body through mids without crowding bass and zabumba. For lows below the useful region, try Low Cut rather than only reducing Bass/Low Gain; control highs/fizz with High Cut without losing attack. Fit guitar, pickup, and monitoring."));
        replace(&r, step("delay", "effectLevel", {"Single", "Analog"},
            "Um slapback opcional, com uma repetição percebida e nível discreto, pode dar corpo à frase; ajuste Delay Time à levada, sem uma trilha de ecos.",
            "An optional slapback with one perceptible repeat at a modest level can thicken the phrase; fit Delay Time to the rhythm rather than creating a trail of echoes.", true));
        replace(&r, step("reverb", "reverbTime", {"Room", "Plate"},
            "Prefira Reverb curto ou moderado. Evite caudas longas por padrão: elas tendem a apagar o ataque e embolar a levada rápida.",
            "Prefer short or moderate Reverb. Avoid long tails by default: they tend to blur attacks and fast rhythms.", true));
    } else if (profile == ToneProfile::NeoSoul) {
        r.goals = QStringList{"warm_clean", "controlled_dynamics", "body", "soft_highs"};
        r.preferredBlocks.append("comp");
        r.optionalBlocks.removeAll("comp");
        r.rationale.ptBr = "Para um clean Neo Soul, procure corpo quente e dinâmica controlada para acordes ricos e ghost notes, com ambiência moderada.";
        r.rationale.en = "For a Neo Soul clean, seek warm body and controlled dynamics for rich chords and ghost notes with moderate ambience.";
        replace(&r, step("comp", "sustain", {"Comp"},
            "COMP pode ser mais presente para nivelar ghost notes e acordes, mas preserve o ataque e recue Sustain se a levada perder contraste.",
            "COMP may be more present to even out ghost notes and chords, but retain attack and back off Sustain if the groove loses contrast."));
        replace(&r, step("eq", "highGain", {},
            "Para o warm clean, mantenha corpo nos médios; Low Cut pode limpar subgraves sem emagrecer o acorde. High Cut/High Gain suavizam agudos e fizz, mas não devem apagar ghost notes. Escolha cortes pela guitarra, captador e monitoração, não por uma receita fixa.",
            "For warm clean, retain mid body; Low Cut can clear sub-lows without thinning chords. High Cut/High Gain soften highs and fizz without erasing ghost notes. Choose cuts for guitar, pickup, and monitoring, not a fixed recipe."));
        replace(&r, step("delay", "effectLevel", {"Analog", "Single"},
            "Delay discreto dá textura ao groove, não uma trilha longa de ecos; teste sem ele para ouvir a divisão rítmica.",
            "Modest Delay adds groove texture rather than a long echo trail; compare without it to hear the rhythm.", true));
        replace(&r, step("reverb", "effectLevel", {"Room", "Plate"},
            "Ambiência moderada mantém ghost notes próximas; um Room ou Plate discreto une o acorde sem transformar a levada em um pad.",
            "Moderate ambience keeps ghost notes close; modest Room or Plate joins chords without turning the groove into a pad.", true));
        r.blockGuidance.append(step("fx1", "type", {"Phaser", "Tremolo"},
            "CHORUS suave ou Phaser/Tremolo em FX-1 são variações opcionais de movimento, sem dominar a divisão rítmica; use ambiência moderada.",
            "Soft CHORUS or FX-1 Phaser/Tremolo are optional movement variations that should not dominate the groove; retain moderate ambience.", true));
    } else if (profile == ToneProfile::Pop) {
        r.goals = QStringList{"controlled_clean", "polished_eq", "articulation"};
        r.rationale.ptBr = "Para clean pop, articulação e consistência de volume ajudam a guitarra a encaixar sem disputar espaço com a voz.";
        r.rationale.en = "For pop clean, articulation and consistent volume help the guitar fit without competing with vocals.";
        replace(&r, step("comp", "sustain", {"Comp"},
            "Use COMP para consistência entre acordes e frases; mantenha Attack legível. CHORUS leve é opcional, com EQ polido e ambiência curta ou moderada.",
            "Use COMP for consistency between chords and phrases while retaining readable Attack. Light CHORUS is optional, with polished EQ and short or moderate ambience.", true));
    } else if (profile == ToneProfile::Worship) {
        if (family == ToneFamily::Clean) {
            r.goals = QStringList{"light_saturation", "body", "dynamic_response", "musical_depth"};
            r.rationale.ptBr = "Clean Worship não precisa ser totalmente limpo: uma leve saturação, próxima de edge, pode dar corpo e resposta dinâmica. Use Delay e Reverb para criar profundidade sem perder o ataque, conforme a música pedir.";
            r.rationale.en = "Worship clean need not be completely clean: light saturation near edge can add body and dynamic response. Use Delay and Reverb for depth without losing attack as the song needs.";
            replace(&r, step("preampA", "gain", {"(Combo) VO Clean", "(TW Clean) Clean Twin", "(TW Clean) Tweed", "(Match) MATCH Drive", "(TW Clean) DELUX Crunch"},
                "Compare VO CLEAN, CLEAN TWIN, TWEED, MATCH e DELUX CRUNCH com Gain reduzido. Suba ganho só até a palhetada forte começar a quebrar e a leve continuar aberta; MATCH e DELUX CRUNCH também podem funcionar com pouco ganho. Não há um único amp correto, nem exigência de saturar sempre.",
                "Compare VO CLEAN, CLEAN TWIN, TWEED, MATCH, and low-Gain DELUX CRUNCH. Raise gain only until strong picking starts breaking up while light picking stays open; MATCH and DELUX CRUNCH can work at low gain too. There is no single correct amp or requirement to saturate."));
        }
        replace(&r, step("comp", "sustain", {"Comp"},
            "Compressão leve/moderada pode segurar a dinâmica entre arpejos e acordes; recue Sustain se a execução perder contraste, em vez de comprimir tudo para obter sustain.",
            "Light/moderate compression can manage dynamics between arpeggios and chords; back off Sustain if playing loses contrast rather than compressing everything for sustain.", true));
        replace(&r, step("eq", "lowCut", {},
            "No Worship, preserve corpo nos médios e limpe o grave que encobre baixo/teclado com Low Cut. High Cut pode conter agudos/fizz, inclusive nas caudas, sem tirar presença da frase. O ajuste depende da guitarra, captador, amp/cab, monitoração e mix.",
            "For Worship, retain mid body and use Low Cut to clear lows masking bass/keys. High Cut can tame highs/fizz, including tails, without removing phrase presence. Adjust for guitar, pickup, amp/cab, monitoring, and mix."));
        replace(&r, step("delay", "effectLevel", {"Single", "DualL/R", "Analog"},
            "Delay mais presente pode sustentar arpejos entre as notas; escolha a divisão rítmica pela música, sem exigir colcheia pontuada. Dual L/R oferece contraste em estéreo, mas é opcional; controle Feedback para o eco não tocar por cima do próximo acorde.",
            "More present Delay can support arpeggios between notes; choose rhythmic subdivision for the song, not mandatory dotted eighths. Dual L/R offers stereo contrast but is optional; manage Feedback so echoes do not cover the next chord.", true));
        replace(&r, step("reverb", "effectLevel", {"Plate", "Hall1", "Mod"},
            "Plate ou Hall1 oferecem espaço; Mod (Modulate) é uma variação de textura. Ajuste cauda e Effect Level separadamente e preserve a entrada da nota. Reverb enorme não é obrigatório.",
            "Plate or Hall1 offer space; Mod (Modulate) is a texture variation. Set tail length and Effect Level separately and preserve note onset. Huge Reverb is not required.", true));
        if (family == ToneFamily::Lead) {
            r.rationale.ptBr = "Lead Worship combina sustain, Gain moderado e presença com Delay musical e Reverb ambiente; a frase ainda precisa permanecer definida.";
            r.rationale.en = "Worship lead combines sustain, moderate Gain, and presence with musical Delay and ambient Reverb; the phrase must remain defined.";
        }
        if (family == ToneFamily::Ambient) {
            r.rationale.ptBr = "Ambient Worship prioriza profundidade e movimento, mas o ataque ainda precisa localizar a frase dentro da cauda.";
            r.rationale.en = "Ambient Worship prioritizes depth and movement, while the attack still needs to locate the phrase within the tail.";
            r.preferredBlocks.append({"delay", "reverb"});
            replace(&r, step("delay", "feedback", {"DualL/R", "Single"},
                "Use Delay rítmico ou Dual L/R para camadas: diferencie os tempos e controle Feedback para que repetições anteriores não cubram a frase seguinte.",
                "Use rhythmic Delay or Dual L/R for layers: contrast times and control Feedback so previous repeats do not cover the next phrase."));
            replace(&r, step("reverb", "reverbTime", {"Hall1", "Mod"},
                "Uma cauda longa pode criar profundidade; ajuste High Cut para suavizar a cauda e Effect Level para preservar ataque. Modulação é opcional, não Shimmer ou Cloud Reverb.",
                "A long tail can add depth; set High Cut to soften the tail and Effect Level to retain attack. Modulation is optional, not Shimmer or Cloud Reverb."));
        }
    }
    if (profile == ToneProfile::Rock && family == ToneFamily::Crunch) {
        r.rationale.ptBr.replace("Para crunch,", "Para crunch de rock,");
        r.rationale.en.replace("For crunch,", "For rock crunch,");
    }
    if (profile == ToneProfile::Rock || family == ToneFamily::Crunch) {
        replace(&r, step("comp", "sustain", {},
            "No rock saturado, compressão extra costuma ser menos prioritária: avalie primeiro o sustain do amp e deixe COMP opcional para passagens limpas.",
            "For driven rock, extra compression is usually lower priority: assess amp sustain first and leave COMP optional for clean passages.", true));
        replace(&r, step("delay", "effectLevel", {"Single", "Analog"},
            "Delay discreto preserva acordes na base; num lead, um nível moderado pode prolongar a frase sem esconder a articulação.",
            "Modest Delay retains rhythm chords; for lead, a moderate level can extend phrases without hiding articulation.", true));
        replace(&r, step("reverb", "effectLevel", {"Room", "Plate"},
            "Use Room/Plate contido na base ou um pouco mais presente no solo, conforme a música; a pausa deve continuar clara.",
            "Use restrained rhythm Room/Plate or a little more for solos as the song needs; rests should remain clear.", true));
    }
    if (profile == ToneProfile::Metal && family == ToneFamily::HighGain) {
        r.rationale.ptBr.replace("Para high gain,", "Para high gain no metal,");
        r.rationale.en.replace("For high gain,", "For metal high gain,");
    }
    if (profile == ToneProfile::Worship) {
        r.rationale.sources.append("FieldKnowledge");
        for (ToneRecipeStep &s : r.blockGuidance)
            if (s.blockId == "eq" || (s.blockId == "preampA" && family == ToneFamily::Clean))
                s.rationale.sources.append("FieldKnowledge");
    }
    return r;
}
}

QList<ToneRecipe> ToneStyleRecipes::recipes() const
{
    QList<ToneRecipe> result;
    for (int i = 0; i < 6; ++i) result.append(base(static_cast<ToneFamily>(i)));
    for (ToneProfile p : {ToneProfile::Worship, ToneProfile::Forro, ToneProfile::NeoSoul, ToneProfile::Pop})
        result.append(styled(ToneFamily::Clean, p));
    result.append(styled(ToneFamily::EdgeOfBreakup, ToneProfile::Worship));
    result.append(styled(ToneFamily::Crunch, ToneProfile::Rock));
    result.append(styled(ToneFamily::HighGain, ToneProfile::Metal));
    result.append(styled(ToneFamily::Lead, ToneProfile::Rock));
    result.append(styled(ToneFamily::Lead, ToneProfile::Worship));
    result.append(styled(ToneFamily::Ambient, ToneProfile::Worship));
    return result;
}

QList<FieldKnowledgeEntry> ToneStyleRecipes::fieldKnowledge() const
{
    // Maintainer-curated Phase 6.1 input. This does not assert that this
    // implementation performed a physical tone or functional validation.
    const QString notes = "Maintainer-curated musical direction; subjective and context-dependent. Not official documentation or a physical test performed by this implementation.";
    return {
        {"worship_clean_character", "Clean Worship pode ter leve saturação, corpo e ambiência mais presente que Forró/Neo Soul.", "Worship clean may use light saturation, body, and more present ambience than Forró/Neo Soul.", "tone_character", "clean/worship", FieldValidationType::CuratedMusicalGuidance, "Guidance", notes},
        {"worship_clean_amps", "VO CLEAN, CLEAN TWIN, TWEED, MATCH e DELUX CRUNCH com pouco Gain são pontos de partida, não um amp obrigatório.", "VO CLEAN, CLEAN TWIN, TWEED, MATCH, and low-Gain DELUX CRUNCH are starting points, not a mandatory amp.", "preamp_direction", "clean/worship", FieldValidationType::CuratedMusicalGuidance, "Guidance", notes},
        {"worship_eq_references", "Perto de 100 Hz e 8 kHz são referências iniciais ajustáveis, não regras ou valores exatos em todo equipamento.", "Near 100 Hz and 8 kHz are adjustable starting references, not rules or exact values on every device.", "eq_reference", "worship", FieldValidationType::CuratedMusicalGuidance, "Guidance", notes},
        {"dataset_eq_low_cut_110", "Low Cut de 110 Hz aparece de forma fortemente recorrente nos presets analisados com EQ ativo.", "A 110 Hz Low Cut is strongly recurring in the analyzed EQ-active presets.", "EQ", "general_tone_guidance", FieldValidationType::ObservedPresetPattern, "StrongWithinDataset",
            "Dataset-derived starting reference; not universal; requires guitar, pickup and monitoring context. No physical tone validation.",
            {{"sourceKind", "PresetReferenceDataset"}, {"candidateId", "KC-15"}, {"evidenceCount", 42}, {"denominator", 53}, {"percentage", 79.25}, {"creatorCount", 4}, {"confidence", "StrongWithinDataset"}, {"dataset", "preset_reference_analysis_2026-09-30"}, {"promotionStatus", "PromotedByMaintainer"}}},
        {"dataset_eq_high_cut_8k", "High Cut de 8 kHz aparece de forma recorrente em parte relevante dos presets analisados com EQ ativo.", "An 8 kHz High Cut recurs in a relevant portion of the analyzed EQ-active presets.", "EQ", "contextual_tone_guidance", FieldValidationType::ObservedPresetPattern, "ModerateWithinDataset",
            "Dataset-derived contextual starting reference; not universal; distribution differs by creator. No physical tone validation.",
            {{"sourceKind", "PresetReferenceDataset"}, {"candidateId", "KC-18"}, {"evidenceCount", 29}, {"denominator", 53}, {"percentage", 54.72}, {"creatorCount", 5}, {"confidence", "ModerateWithinDataset"}, {"dataset", "preset_reference_analysis_2026-09-30"}, {"promotionStatus", "PromotedByMaintainer"}}}
    };
}

ToneConversationReply ToneStyleRecipes::eqDatasetGuidance(const AIRequest &request) const
{
    ToneConversationReply reply;
    const QString q = ToneIntentParser::normalize(request.userMessage);
    // Narrow guidance only: retain technical explanations, patch assessment,
    // capability lists, style builds and effect-specific filters on their paths.
    for (const char *term : {"meu patch", "patch atual", "my patch", "current", "o que", "what is", "explique", "explain", "diferenca", "compare", "range", "opcoes", "options", "tipos", "types", "assign", "target", "reverb", "chorus", "delay", "montar", "build", "criar", "write", "aplique", "altere", "mude"})
        if (q.contains(QLatin1String(term))) return reply;
    if (q.contains("como fazer") || q.contains("how to make") || q.contains("quero um") || q.contains("i want a")) return reply;
    const bool mandatory = q.contains("obrigatorio") || q.contains("mandatory") || q.contains("regra") || q.contains("rule");
    const bool low = q.contains("low cut") || q.contains("lowcut") || q.contains("low end") || q.contains("low-end") || q.contains("grave embolado") || q.contains("embaixo")
        || ((mandatory || q.contains("fazem sentido")) && (q.contains("110") || q.contains("100")));
    const bool high = q.contains("high cut") || q.contains("highcut") || q.contains("fizz") || q.contains("controlar agudos")
        || ((mandatory || q.contains("fazem sentido")) && q.contains("8") && (q.contains("khz") || q.contains("k hz")));
    const bool guidance = mandatory || q.contains("usar") || q.contains("use") || q.contains("qual") || q.contains("which") || q.contains("que frequencia") || q.contains("como") || q.contains("how") || q.contains("fazem sentido");
    if ((!low && !high) || !guidance) return reply;
    const bool pt = request.language.startsWith("pt", Qt::CaseInsensitive);
    QStringList paragraphs;
    if (mandatory) paragraphs.append(choose(pt, "Não é uma regra obrigatória; são referências ajustáveis, não valores universais.", "These are adjustable references, not mandatory rules or universal values."));
    if (low) paragraphs.append(choose(pt,
        "Para o EQ, experimente 110 Hz como ponto de partida: é uma referência fortemente recorrente nos presets analisados. Ajuste ouvindo conforme guitarra, captador e monitoração; se perder corpo, compare FLAT, 55 Hz ou outra opção disponível.",
        "For EQ, try 110 Hz as a starting point: it is strongly recurring in the analyzed reference presets. Adjust by ear for your guitar, pickup and monitoring; if body is lost, compare FLAT, 55 Hz or another available option."));
    if (high) paragraphs.append(choose(pt,
        "No High Cut do EQ, 8 kHz é uma referência recorrente nos presets analisados, não um valor universal. 6 kHz também aparece com frequência nesse dataset, como alternativa de referência. Experimente e ajuste ouvindo o equilíbrio entre presença e fizz, conforme guitarra, captador e monitoração.",
        "For EQ High Cut, 8 kHz is a recurring reference in the analyzed presets, not a universal value. 6 kHz also occurs frequently in that dataset as an alternative reference. Experiment and adjust by ear for the balance between presence and fizz, guitar, pickup and monitoring."));
    QJsonArray trace;
    for (const FieldKnowledgeEntry &entry : fieldKnowledge()) {
        if ((low && entry.id == "dataset_eq_low_cut_110") || (high && entry.id == "dataset_eq_high_cut_8k")) {
            QJsonObject source = entry.sourceTrace;
            source.insert("id", entry.id); source.insert("category", entry.category);
            source.insert("scope", entry.scope); source.insert("validationType", "ObservedPresetPattern");
            source.insert("notes", entry.notes); trace.append(source);
        }
    }
    reply.handled = reply.answered = true;
    reply.mode = ToneResponseMode::Recommend;
    reply.route = "eq_dataset_guidance"; reply.topicId = "eq_starting_references";
    reply.text = paragraphs.join("\n\n");
    reply.metadata = {{"fieldKnowledge", trace}, {"knowledgeSources", QJsonArray{"PresetReferenceDataset"}}, {"readOnly", true}};
    if (high) reply.metadata.insert("referenceOnly", QJsonArray{QJsonObject{{"candidateId", "KC-17"}, {"value", "6 kHz"}, {"evidenceCount", 22}, {"denominator", 53}, {"creatorCount", 4}, {"promotionStatus", "NotPromoted"}}});
    return reply;
}

QList<ToneCatalogEntry> ToneStyleRecipes::catalog() const
{
    QList<ToneCatalogEntry> result;
    const auto add = [&](const char *block, const char *type, const QStringList &tags,
                         const char *role, const char *gain, const char *brightness,
                         const QStringList &profiles, const char *limit) {
        if (exactType(capabilities, block, type))
            result.append({block, type, tags, role, gain, brightness, profiles,
                limit, {"CapabilityService", "Guidance"}});
    };
    add("preampA", "(JC Clean) JC-120", {"clean", "definition"}, "clean_base", "low", "adjustable", {"neutral", "forro", "pop", "worship"}, "Audition at matched level; Gain and playing affect the result.");
    add("preampA", "(TW Clean) Clean Twin", {"clean", "body", "breakup"}, "touch_sensitive_base", "low_to_moderate", "adjustable", {"neo_soul", "worship", "neutral"}, "Breakup depends on variant, Gain, pickups and attack.");
    add("preampA", "(Crunch) Boss Crunch", {"crunch", "attack"}, "rhythm", "moderate", "adjustable", {"rock"}, "Excess Gain can mask chords.");
    add("preampA", "(MS Classic)MS1959 I", {"crunch", "mid_presence"}, "rhythm_or_lead", "moderate", "adjustable", {"rock"}, "Compare at matched level; no commercial-pedal equivalence asserted.");
    add("preampA", "(R-Fier) R-Fier Modern", {"high_gain", "definition"}, "heavy_rhythm", "high", "adjustable", {"metal"}, "Modern is a driven variant; do not confuse it with R-Fier Clean.");
    add("odDs", "(Blues) Blues OD", {"low_gain", "second_stage"}, "light_drive", "low_to_moderate", "adjustable", {"worship", "rock"}, "Optional stage; not an absolute best drive.");
    add("odDs", "(Booster) Clean Boost", {"boost", "level"}, "boost", "low", "adjustable", {"worship", "pop"}, "Boost before a saturated amp may add distortion instead of output volume.");
    add("odDs", "(OD) Tubescreamer", {"drive", "focus"}, "tighten_input", "moderate", "adjustable", {"metal", "rock"}, "Low Drive is an optional input-shaping approach.");
    add("comp", "Comp", {"compression", "sustain"}, "level_control", "not_applicable", "adjustable", {"forro", "pop", "neo_soul"}, "Too much Sustain can raise noise and flatten dynamics.");
    add("delay", "Single", {"rhythm", "slapback"}, "repeat", "not_applicable", "adjustable", {"forro", "pop"}, "Fit time and repeat level to the rhythm.");
    add("delay", "DualL/R", {"stereo", "contrast"}, "spatial_repeat", "not_applicable", "adjustable", {"worship", "rock"}, "Requires two output paths to hear left/right contrast.");
    add("reverb", "Room", {"short_space"}, "controlled_ambience", "not_applicable", "adjustable", {"forro", "pop", "neutral"}, "Level and time still determine masking.");
    add("reverb", "Plate", {"space", "sustain"}, "musical_tail", "not_applicable", "adjustable", {"worship", "rock", "neo_soul"}, "Long tails can obscure articulation.");
    add("reverb", "Mod", {"texture", "ambient"}, "moving_tail", "not_applicable", "adjustable", {"worship"}, "Not Shimmer or Cloud Reverb.");
    add("chorus", "Stereo1", {"stereo", "movement"}, "width", "not_applicable", "adjustable", {"pop", "worship", "neo_soul"}, "Check mono compatibility and the output destination.");
    add("chorus", "Stereo2", {"stereo", "movement"}, "width_variation", "not_applicable", "adjustable", {"pop", "worship"}, "Audition modes; no fixed ideal depth.");
    add("fx1", "Phaser", {"movement"}, "groove_variation", "not_applicable", "adjustable", {"neo_soul", "rock"}, "Consumes the FX-1 slot; avoid obscuring the groove.");
    add("fx1", "Tremolo", {"rhythmic_level"}, "groove_variation", "not_applicable", "adjustable", {"neo_soul", "pop"}, "Consumes the FX-1 slot; match movement to the phrase.");
    return result;
}

ToneConversationReply ToneStyleRecipes::respond(const AIRequest &request) const
{
    ToneConversationReply reply;
    const ToneStyleRequest style = ToneTaxonomy::parse(request.userMessage);
    if (!style.recognized) return reply;
    const bool pt = request.language.startsWith("pt", Qt::CaseInsensitive);
    reply.handled = true;
    reply.answered = !style.needsClarification;
    reply.mode = style.needsClarification ? ToneResponseMode::Clarify : ToneResponseMode::Build;
    reply.route = "tone_build";
    reply.topicId = "style_recipe";
    QJsonObject &meta = reply.metadata;
    const bool implicitSpatial = !style.explicitFamily && (style.spatialGoal == ToneSpatialGoal::Big || style.spatialGoal == ToneSpatialGoal::Stereo);
    const bool problemOnly = !style.explicitFamily && !style.explicitProfile
        && style.spatialGoal == ToneSpatialGoal::None && style.toneProblem != ToneProblem::None;
    const bool composed = style.usageRole != ToneUsageRole::Neutral || style.toneProblem != ToneProblem::None;
    meta.insert("toneFamily", implicitSpatial || problemOnly ? QString() : ToneTaxonomy::id(style.family));
    meta.insert("toneGoal", style.spatialGoal == ToneSpatialGoal::Big ? "bigger" : style.spatialGoal == ToneSpatialGoal::Stereo ? "wider" : "");
    meta.insert("toneProfile", ToneTaxonomy::id(style.profile));
    QStringList modifierIds;
    for (ToneModifier modifier : style.modifiers) modifierIds.append(ToneTaxonomy::id(modifier));
    meta.insert("toneModifiers", QJsonArray::fromStringList(modifierIds));
    meta.insert("guitarContext", style.guitarContext);
    meta.insert("usageContext", style.usageContext);
    meta.insert("usageRole", ToneTaxonomy::id(style.usageRole));
    meta.insert("toneProblem", ToneTaxonomy::id(style.toneProblem));
    meta.insert("spatialGoal", style.spatialGoal == ToneSpatialGoal::Stereo ? "stereo" : style.spatialGoal == ToneSpatialGoal::Big ? "big" : style.spatialGoal == ToneSpatialGoal::Modern ? "modern" : "none");
    meta.insert("compositionPriority", QJsonArray::fromStringList({"explicit_goal", "usage_role", "explicit_problem", "family", "profile", "spatial_goal", "modifiers", "patch_context"}));
    meta.insert("preserveBody", style.preserveBody);
    meta.insert("patchContextRequested", style.usePatchContext);
    meta.insert("responseDetail", style.wantsDetail ? "detailed" : "compact");
    meta.insert("readOnly", true);
    if (style.needsClarification) {
        reply.text = choose(pt, "Você quer um rock pesado com acordes definidos ou high gain para metal com palm mute apertado? Essa escolha muda a base de ganho e EQ.", "Do you want heavy rock with defined chords or metal high gain with tight palm mutes? That changes the gain and EQ foundation.");
        return reply;
    }
    ToneRecipe recipe = base(style.family);
    bool exactRecipe = false;
    for (const ToneRecipe &candidate : recipes()) {
        if (candidate.family == style.family && candidate.profile == style.profile) {
            recipe = candidate;
            exactRecipe = true;
            break;
        }
    }
    if (problemOnly) {
        recipe.goals = QStringList{"retain_tonal_family", "compare_stages", "level_balance"};
        // A reported problem does not identify a clean amp or a new family.
        replace(&recipe, step("preampA", "gain", {},
            "Preserve a família tonal e compare ganho, voicing e níveis um estágio por vez, sem assumir clean nem carregar outro patch.",
            "Retain the tonal family and compare gain, voicing, and levels one stage at a time, without assuming clean or loading another patch."));
    }
    composeUsage(&recipe, style);
    QStringList paragraphs;
    QString proposal = pt ? recipe.rationale.ptBr : recipe.rationale.en;
    if (style.explicitProfile && !style.explicitFamily && style.profile != ToneProfile::Metal)
        proposal += choose(pt, " Vou partir de clean; se você quiser edge, crunch ou lead, a estrutura de ganho muda.", " I will start from clean; edge, crunch, or lead would change the gain structure.");
    paragraphs.append(proposal);
    QStringList sources = recipe.rationale.sources;
    if (style.usageRole != ToneUsageRole::Neutral) sources.append("UsageRole");
    if (style.toneProblem != ToneProblem::None) sources.append("ToneProblem");
    QStringList addressed;
    QJsonArray steps;
    const QJsonArray cuts = eqCutCapabilities();
    meta.insert("eqCutCapabilities", cuts);
    QJsonArray fieldTrace;
    if (style.profile == ToneProfile::Worship) {
        for (const FieldKnowledgeEntry &entry : fieldKnowledge()) {
            if (entry.validationType != FieldValidationType::CuratedMusicalGuidance) continue;
            if (entry.scope == "clean/worship" && style.family != ToneFamily::Clean) continue;
            fieldTrace.append(QJsonObject{{"id", entry.id}, {"claim", pt ? entry.claimPtBr : entry.claimEn},
                {"category", entry.category}, {"scope", entry.scope},
                {"validationType", "CuratedMusicalGuidance"}, {"confidence", entry.confidence},
                {"notes", entry.notes}, {"source", "FieldKnowledge"}});
        }
        sources.append("FieldKnowledge");
    }
    meta.insert("fieldKnowledge", fieldTrace);
    QStringList dry, space, dynamics, equalization;
    const QList<ToneCatalogEntry> semantics = catalog();
    for (const ToneRecipeStep &s : recipe.blockGuidance) {
        if (implicitSpatial && (s.blockId == "preampA" || s.blockId == "comp")) continue;
        if (!capabilities.hasBlock(s.blockId)) continue;
        addressed.append(s.blockId);
        QStringList types;
        for (const QString &candidate : s.compatibleTypes)
            if (exactType(capabilities, s.blockId, candidate)) types.append(candidate);
        QJsonObject entry;
        entry.insert("blockId", s.blockId);
        entry.insert("parameterId", s.parameterId);
        entry.insert("optional", s.optional);
        entry.insert("types", QJsonArray::fromStringList(types));
        QJsonArray characters;
        for (const ToneCatalogEntry &character : semantics) {
            if (character.blockId == s.blockId && types.contains(character.type))
                characters.append(QJsonObject{{"type", character.type},
                    {"tags", QJsonArray::fromStringList(character.characterTags)},
                    {"role", character.typicalRole}, {"gainTendency", character.gainTendency},
                    {"brightnessTendency", character.brightnessTendency},
                    {"limitations", character.limitation},
                    {"sources", QJsonArray::fromStringList(character.sources)}});
        }
        entry.insert("characters", characters);
        entry.insert("guidance", pt ? s.ptBr : s.en);
        entry.insert("sources", QJsonArray::fromStringList(s.rationale.sources + QStringList{"CapabilityService"}));
        steps.append(entry);
        // Keep all evidence/options in metadata; render only the requested
        // block when the question is focused, without a second composer.
        if (!style.focusBlock.isEmpty() && style.focusBlock != "sizeComparison"
            && style.focusBlock != s.blockId && style.focusBlock != "odDs") continue;
        QString sentence = pt ? s.ptBr : s.en;
        const QString question = ToneIntentParser::normalize(request.userMessage);
        if (s.blockId == "preampA" && style.profile == ToneProfile::Worship && question.contains("posso usar")
            && (question.contains("match") || question.contains("deluxe crunch") || question.contains("delux crunch"))) {
            const bool deluxe = question.contains("deluxe crunch") || question.contains("delux crunch");
            const QString model = deluxe ? "DELUX CRUNCH" : "MATCH";
            sentence = choose(pt, "Sim, ", "Yes, ") + model + choose(pt,
                " pode funcionar com pouco ganho para esse clean. Ajuste Gain pela resposta ao toque: preserve abertura na palhetada leve e teste uma leve saturação na forte, sem exigir que o som esteja sempre saturado.",
                " can work at low gain for this clean. Set Gain by touch response: retain openness under light picking and test light saturation with strong picking, without requiring constant saturation.");
            types = QStringList{deluxe ? "(TW Clean) DELUX Crunch" : "(Match) MATCH Drive"};
        }
        if (s.blockId == "preampA" && style.profile == ToneProfile::Worship
            && style.family == ToneFamily::Clean && !style.wantsDetail && !question.contains("posso usar")) {
            sentence = choose(pt, "Eu começaria com MATCH ou VO CLEAN com Gain reduzido: deixe a palhetada forte começar a quebrar e a leve continuar aberta. Outras opções também funcionam.",
                "I would start with MATCH or VO CLEAN at low Gain: let strong picking begin to break up while light picking stays open. Other options can work too.");
            if (style.usageRole == ToneUsageRole::Arpeggio) sentence += choose(pt,
                " Uma leve saturação pode dar textura sem fundir as notas.", " Light saturation can add texture without merging notes.");
            types = QStringList{"(Match) MATCH Drive", "(Combo) VO Clean"};
            if (style.focusBlock == "preampA") {
                sentence += choose(pt, " TWEED também é um ponto de partida com Gain reduzido; compare com pouco ganho e volume equilibrado.",
                    " TWEED is another low-Gain starting point; compare at low gain and balanced volume.");
                types.append("(TW Clean) Tweed");
            }
        }
        if (s.blockId == "eq" && style.profile == ToneProfile::Worship
            && (style.toneProblem == ToneProblem::None || style.wantsDetail)) {
            bool low = false, high = false;
            for (const QJsonValue &c : cuts) {
                const QJsonObject info = c.toObject();
                const QJsonArray values = info.value("enumValues").toArray();
                if (info.value("parameterId") == "lowCut") low = values.contains("110Hz");
                if (info.value("parameterId") == "highCut") high = values.contains("8.00kHz");
            }
            if (low && high) sentence += choose(pt,
                " Como referência, experimente perto de 100 Hz e 8 kHz: a GT-10 oferece 110Hz e 8.00kHz. Não é obrigatório; ajuste à guitarra, captador, amp/cab, monitoração e mix, inclusive usando FLAT.",
                " As a reference, try near 100 Hz and 8 kHz: GT-10 offers 110Hz and 8.00kHz. It is not mandatory; fit guitar, pickup, amp/cab, monitoring, and mix, including FLAT.");
        }
        // A focused model-choice/detail question can expose the full catalog;
        // ordinary builds get at most two relevant alternatives per block.
        const bool worshipModelsAlreadyNamed = s.blockId == "preampA" && style.profile == ToneProfile::Worship
            && style.family == ToneFamily::Clean && !style.wantsDetail;
        if (!types.isEmpty() && !worshipModelsAlreadyNamed
            && (s.blockId == "preampA" || style.wantsDetail || !style.focusBlock.isEmpty()))
            sentence += choose(pt, " Comece por ", " Start with ")
                + (style.wantsDetail ? types : types.mid(0, 3)).join(choose(pt, " ou ", " or ")) + ".";
        if (s.blockId == "preampA" || s.blockId == "comp" || s.blockId == "eq") {
            dry.append(sentence);
            if (s.blockId == "eq") equalization.append(sentence);
            else dynamics.append(sentence);
        }
        else space.append(sentence);
    }
    sources.append("CapabilityService");
    paragraphs.append(dry.join(' '));
    paragraphs.append(space.join(' '));
    if (!style.focusBlock.isEmpty() && style.focusBlock != "sizeComparison") {
        if (style.focusBlock == "eq") paragraphs[0] = choose(pt, "Para o EQ, controle graves que ocupam espaço e agudos ásperos sem perder corpo nem ataque.", "For EQ, control lows that crowd the mix and harsh highs without losing body or attack.");
        else if (style.focusBlock == "preampA") {
            if (ToneIntentParser::normalize(request.userMessage).contains("posso usar") && !dry.isEmpty()) paragraphs[0] = dry.takeFirst();
            else paragraphs[0] = choose(pt, "Escolha o PREAMP pela resposta ao toque e pela quantidade de saturação que você quer, comparando com o volume equilibrado.", "Choose PREAMP by touch response and desired saturation, comparing at balanced volume.");
            if (dry.isEmpty()) paragraphs[1].clear();
        }
        else if (style.focusBlock == "delay") paragraphs[0] = choose(pt, "Para o Delay, escolha o tipo e a divisão pela frase que você toca; mantenha a próxima nota clara.", "For Delay, choose type and subdivision for your phrase; keep the next note clear.");
        else if (style.focusBlock == "reverb") paragraphs[0] = choose(pt, "Para o Reverb, escolha uma cauda que dê profundidade sem esconder o ataque.", "For Reverb, choose a tail that adds depth without hiding attack.");
    }
    if (style.focusBlock == "odDs") {
        QStringList options;
        for (const ToneCatalogEntry &entry : catalog()) {
            if (entry.blockId == "odDs") options.append(entry.type);
        }
        paragraphs[1] = choose(pt, "A escolha de OD/DS depende da função, não de um 'melhor para Worship'. Para base low gain, considere Blues OD; para boost, Clean Boost; para focar a entrada de um PREAMP saturado, (OD) Tubescreamer com Drive baixo. Antes do PREAMP, boost pode aumentar saturação; para lead, compare sustain e volume percebido. Opções confirmadas no catálogo: ", "OD/DS choice depends on role, not a 'best for Worship'. For low-gain base consider Blues OD; for boost, Clean Boost; for focusing a saturated PREAMP input, low-Drive (OD) Tubescreamer. Before PREAMP, boost may increase saturation; for lead compare sustain and perceived volume. Catalog-confirmed options: ") + options.join(" / ") + ".";
        addressed.append("odDs");
    }

    const bool spatial = style.spatialGoal == ToneSpatialGoal::Big || style.spatialGoal == ToneSpatialGoal::Stereo || style.modifiers.contains(ToneModifier::Wider);
    QJsonArray documentationTrace;
    if (style.spatialGoal == ToneSpatialGoal::Big) {
        paragraphs[0] = choose(pt, "Grande é densidade percebida, corpo e profundidade, não sinônimo de estéreo. Um timbre pode soar grande em mono; não precisa virar clean nem ganhar mais Reverb automaticamente.", "Big means perceived density, body, and depth, not stereo. A tone can sound big in mono; it need not become clean or automatically gain more Reverb.");
        paragraphs[1] = choose(pt, "Preserve a estrutura de ganho do preset e procure sustain que apoie a frase. No EQ, abra espaço espectral para o corpo sem competir com baixo/voz: Low Cut pode remover grave inútil e High Cut conter fizz, conforme guitarra, captador e monitoração. Compare com level balance, porque mais volume sozinho engana a percepção de tamanho.", "Retain the preset's gain structure and seek sustain that supports phrases. In EQ, make spectral space for body without competing with bass/vocals: Low Cut can remove useless lows and High Cut tame fizz for your guitar, pickup, and monitoring. Compare with level balance, since extra volume alone can mislead perceived size.");
        paragraphs[2] = choose(pt, "Delay e Reverb podem criar profundidade atrás da nota: ajuste cauda e nível mantendo contraste entre ataque, sustain e silêncio. Dual amp é opcional; voicings A/B complementares podem preencher regiões diferentes, mas some-os em mono para conferir embolamento e equilibre os níveis. Largura estéreo é só uma possibilidade, não requisito para soar grande.", "Delay and Reverb can place depth behind notes: adjust tails and level while retaining contrast between attack, sustain, and silence. Dual amp is optional; complementary A/B voicings can fill different regions, but sum in mono to check muddiness and balance levels. Stereo width is only one possibility, not a requirement for size.");
        addressed.append({"preampGlobal", "preampA", "preampB", "eq", "delay", "reverb"});
        if (style.explicitFamily) {
            paragraphs[0] = choose(pt, "Para um timbre ", "For a ")
                + ToneTaxonomy::id(style.family).replace('_', ' ')
                + choose(pt, " soar grande, trabalhe corpo, densidade e profundidade; isso também pode funcionar em mono.",
                    " tone to sound big, work on body, density, and depth; this can work in mono too.");
            paragraphs[1] = dry.join(' ');
        }
    } else if (spatial) {
        paragraphs[0] = choose(pt, "Estéreo é separação L/R audível e controlada; corpo e sustain não são o objetivo principal aqui. Preserve a família tonal do preset e organize diferenças entre os dois lados.", "Stereo is audible, controlled L/R separation; body and sustain are not the main objective here. Retain the preset's tonal family and organize differences between the two sides.");
        paragraphs[1] = choose(pt, "PREAMP em Dual L/R separa A e B. Use diferenças pequenas de voicing/ganho e equilibre os níveis para não deslocar o centro; dois lados iguais têm pouco contraste. Confira o routing real da cadeia e a posição dos efeitos: não basta ativar dois amps para obter separação na saída.", "PREAMP Dual L/R separates A and B. Use small voicing/gain differences and balance levels to avoid shifting the center; identical sides have little contrast. Check actual chain routing and effect positions: enabling two amps alone does not ensure output separation.");
        paragraphs[2] = choose(pt, "CHORUS Stereo1/Stereo2 e Delay Dual L/R podem distribuir movimento e repetições entre os lados; diferencie tempos/níveis com critério, sem borrar o ataque. Use OUTPUT L/R e duas entradas com monitoração estéreo real. Depois confira a compatibilidade mono: somar os lados pode mudar equilíbrio e modulação, e uma única entrada mono não entrega a mesma separação. Não estou propondo um stereo widener dedicado.", "CHORUS Stereo1/Stereo2 and Dual L/R Delay can distribute movement and repeats across the sides; contrast times/levels carefully without blurring attack. Use OUTPUT L/R and two inputs with actual stereo monitoring. Then check mono compatibility: summing sides may change balance and modulation, and one mono input does not deliver the same separation. No dedicated stereo widener is proposed.");
        addressed.append({"preampGlobal", "preampA", "preampB", "delay", "reverb", "chorus", "eq"});
        if (style.explicitFamily) {
            // Compose family + spatial goal rather than replacing one with
            // the other. Keep gain advice and make stereo the opening action.
            paragraphs[0] = choose(pt, "Para fazer um timbre ", "To build a ")
                + ToneTaxonomy::id(style.family).replace('_', ' ')
                + (style.profile == ToneProfile::Metal ? choose(pt, " para metal", " for metal") : QString())
                + choose(pt, " realmente estéreo na GT-10, comece com Dual Preamp em Dual L/R. Crie pequenas diferenças de voicing, EQ ou ganho entre A/B, equilibrando os níveis para manter o centro e a coesão.",
                    " truly stereo tone on GT-10, start with Dual Preamp in Dual L/R. Create small A/B differences in voicing, EQ, or gain, balancing levels to retain the center and cohesion.");
            paragraphs[1] = dry.join(' ');
        }
        if (style.family == ToneFamily::HighGain && style.explicitFamily && !composed) {
            paragraphs[1] = choose(pt,
                "Use ganho suficiente, não máximo, para preservar definição da nota. Comece com R-Fier Modern ou MS HiGain e compare com level balance. Controle o low-end com Low Cut/Bass sem tirar peso, mantenha médios audíveis e Presence/agudos sem fizz. OD/DS com Drive baixo antes do PREAMP é opcional. Ajuste NS ao ruído real sem cortar sustain.",
                "Use sufficient gain, not maximum, to retain note definition. Start with R-Fier Modern or MS HiGain and compare at balanced level. Control low-end with Low Cut/Bass without losing weight, retain audible mids, and keep Presence/highs free of fizz. Low-Drive OD/DS before PREAMP is optional. Set NS for actual noise without cutting sustain.");
            paragraphs[2] = choose(pt,
                "Delay Dual L/R cria repetições nos dois lados; Chorus Stereo1/Stereo2 é opcional. Defina o uso antes do nível de ambiência: para base, preserve pausas com espaço discreto; para lead, teste mais profundidade sem borrar a frase.",
                "Dual L/R Delay creates repeats on both sides; Stereo1/Stereo2 Chorus is optional. Define the role before ambience level: for rhythm, retain rests with modest space; for lead, test more depth without blurring phrases.");
            paragraphs.append(choose(pt,
                "Use OUTPUT L/R e duas entradas com monitoração estéreo real; uma entrada mono não entrega a mesma largura. Confira o routing até as saídas e teste a compatibilidade mono para não perder peso ou desequilibrar A/B.",
                "Use OUTPUT L/R and two inputs with real stereo monitoring; one mono input does not deliver the same width. Check routing to the outputs and test mono compatibility to avoid losing weight or unbalancing A/B."));
        }
    }
    if (style.focusBlock == "sizeComparison") {
        paragraphs[0] = choose(pt, "Soar grande e soar estéreo são objetivos diferentes: grande descreve corpo, densidade, sustain e profundidade, inclusive em mono; estéreo descreve separação L/R.", "Big and stereo are different goals: big describes body, density, sustain, and depth even in mono; stereo describes L/R separation.");
        paragraphs[1] = choose(pt, "Para tamanho, trabalhe ocupação espectral no EQ, contraste e equilíbrio de níveis; Delay/Reverb podem apoiar a profundidade. Para separação, confira PREAMP Dual L/R, Chorus Stereo1/Stereo2, Delay Dual L/R e o routing até OUTPUT L/R.", "For size, work EQ spectral space, contrast, and level balance; Delay/Reverb can support depth. For separation, check PREAMP Dual L/R, Chorus Stereo1/Stereo2, Dual L/R Delay, and routing to OUTPUT L/R.");
    }
    if (style.spatialGoal == ToneSpatialGoal::Modern)
        paragraphs[0] = choose(pt, "'Moderno' não define um único timbre: vou partir de definição e controle. Na GT-10, comece com graves controlados, ganho suficiente e equilíbrio de níveis; decida depois se a música pede clean, crunch ou high gain.", "'Modern' does not define one tone: I will start with definition and control. On the GT-10, begin with controlled lows, sufficient gain, and balanced levels; then decide whether the song needs clean, crunch, or high gain.");
    if (style.preserveBody && spatial) paragraphs[2] += choose(pt,
        " Para abrir sem perder peso, preserve o corpo nos médios e confira o grave na soma mono; não compense largura aumentando Bass nos dois lados.",
        " To widen without losing weight, retain mid body and check lows in the mono sum; do not compensate width by raising Bass on both sides.");
    // Keep semantic block guidance through spatial composition. The opening
    // names the goal; following paragraphs perform gain/EQ/space actions.
    if (composed && style.focusBlock.isEmpty()) {
        const QString role = style.usageRole == ToneUsageRole::Lead ? "lead"
            : style.usageRole == ToneUsageRole::Rhythm ? choose(pt, "timbre de base", "rhythm tone")
            : style.usageRole == ToneUsageRole::Arpeggio ? choose(pt, "arpejo", "arpeggio")
            : style.usageRole == ToneUsageRole::Ambient ? "ambient" : choose(pt, "timbre", "tone");
        const QString familyId = ToneTaxonomy::id(style.family).replace('_', ' ');
        const QString family = problemOnly || role == familyId ? QString() : familyId;
        const QString profile = style.profile == ToneProfile::Neutral ? QString() : ToneTaxonomy::id(style.profile);
        QString opening = choose(pt, "Para esse ", "For this ") + role + " " + family + " " + profile;
        if (style.spatialGoal == ToneSpatialGoal::Stereo) opening += choose(pt, " estéreo", " stereo");
        else if (style.spatialGoal == ToneSpatialGoal::Big) opening += choose(pt, " grande", " big");
        opening = opening.simplified();
        QString goal;
        switch (style.toneProblem) {
        case ToneProblem::Muddy: goal = choose(pt, "priorize sustain com definição e low-end controlado antes de aumentar ganho", "prioritize sustain with definition and controlled low-end before adding gain"); break;
        case ToneProblem::Harsh: goal = choose(pt, "priorize ataque claro sem estridência, controlando Presence e agudos sem apagar a dinâmica", "prioritize clear attack without harshness, controlling Presence and highs without erasing dynamics"); break;
        case ToneProblem::Thin: goal = choose(pt, "recupere corpo nos médios antes de compensar tudo com graves", "recover mid body before compensating everything with lows"); break;
        case ToneProblem::Noisy: goal = choose(pt, "controle ruído sem sacrificar sustain ou finais de notas", "control noise without sacrificing sustain or note tails"); break;
        case ToneProblem::NotDefined: goal = choose(pt, "priorize definição e preserve o ataque antes de acrescentar ganho ou caudas", "prioritize definition and retain attack before adding gain or tails"); break;
        case ToneProblem::NeedsSustain: goal = choose(pt, "priorize sustain e médios sem depender de aumentar Gain", "prioritize sustain and mids without relying on raising Gain"); break;
        case ToneProblem::NeedsWidth: goal = choose(pt, "abra os lados sem perder o centro da nota", "widen the sides without losing the note center"); break;
        default: goal = style.usageRole == ToneUsageRole::Lead ? choose(pt, "priorize sustain, médios e definição da nota", "prioritize sustain, mids, and note definition") : choose(pt, "priorize ataque, articulação e espaço adequado à execução", "prioritize attack, articulation, and space suited to playing"); break;
        }
        paragraphs = QStringList{opening + ", " + goal + ".", dynamics.join(' '), equalization.join(' '), space.join(' ')};
        if (style.spatialGoal == ToneSpatialGoal::Stereo) {
            paragraphs[0] += choose(pt, " Use Dual L/R com pequenas diferenças A/B e níveis equilibrados para manter o centro da nota.", " Use Dual L/R with small A/B differences and balanced levels to retain the note center.");
            paragraphs[3] += choose(pt, " Delay Dual L/R pode distribuir repetições sem desfazer o centro; Chorus Stereo1/Stereo2 é opcional.", " Dual L/R Delay can distribute repeats without losing the center; Stereo1/Stereo2 Chorus is optional.");
            paragraphs[3] += choose(pt, " Use OUTPUT L/R com duas entradas e monitoração estéreo real. Confira o routing até as saídas e teste compatibilidade mono e equilíbrio A/B; uma entrada mono não entrega a mesma largura.", " Use OUTPUT L/R with two inputs and real stereo monitoring. Check routing to the outputs and test mono compatibility and A/B balance; one mono input does not deliver the same width.");
        }
    }
    if (style.profile == ToneProfile::Worship && style.focusBlock.isEmpty() && !spatial)
        paragraphs = QStringList{paragraphs.first(), dynamics.join(' '), equalization.join(' '), space.join(' ')};
    if (style.preserveBody && spatial && composed) paragraphs[2] += choose(pt,
        " Para abrir sem perder peso, preserve corpo nos médios e confira a soma mono.",
        " To widen without losing weight, retain mid body and check the mono sum.");
    QJsonObject renderedOptions;
    // Presentation budget only: retain full recipes, model choices and evidence
    // above. Focused EQ/model questions and requested detail keep their content.
    const bool compactBuild = !style.wantsDetail && style.focusBlock.isEmpty()
        && !implicitSpatial && !problemOnly && style.spatialGoal != ToneSpatialGoal::Big
        && style.spatialGoal != ToneSpatialGoal::Modern
        && (style.profile == ToneProfile::Worship || style.family == ToneFamily::HighGain
            || style.family == ToneFamily::Lead || (style.family == ToneFamily::Crunch && composed));
    if (compactBuild) {
        QStringList amps;
        for (const auto &v : steps) {
            const auto b = v.toObject();
            if (b.value("blockId") != "preampA") continue;
            for (const auto &t : b.value("types").toArray()) amps.append(t.toString());
        }
        if (style.profile == ToneProfile::Worship && style.family == ToneFamily::Clean)
            amps = QStringList{"(Match) MATCH Drive", "(Combo) VO Clean"};
        amps = amps.mid(0, 2);
        renderedOptions.insert("preampA", QJsonArray::fromStringList(amps));
        QStringList labels;
        for (const QString &a : amps) labels.append(a.section(')', -1).trimmed());
        if (style.profile == ToneProfile::Worship && style.family == ToneFamily::Clean)
            labels = QStringList{"MATCH", "VO CLEAN"};
        QString amp = choose(pt, "Eu começaria com ", "I would start with ") + labels.join(choose(pt, " ou ", " or ")) + ". ";
        const bool lead = style.usageRole == ToneUsageRole::Lead;
        const bool rhythm = style.usageRole == ToneUsageRole::Rhythm;
        QString opening = paragraphs.first();
        if (style.profile == ToneProfile::Worship && style.family == ToneFamily::Clean && !composed
            && !ToneIntentParser::normalize(request.userMessage).contains("totalmente limpo"))
            opening = choose(pt, "Para um clean Worship, procure leve saturação e dinâmica, com ataque claro e corpo.", "For a Worship clean, aim for light saturation and dynamics, with clear attack and body.");
        if (style.family == ToneFamily::HighGain && !composed && !spatial) opening = choose(pt,
            "Para high gain, priorize definição e médios audíveis com ganho suficiente, não máximo.",
            "For high gain, prioritize definition and audible mids with sufficient, not maximum, gain.");
        if (style.family == ToneFamily::Clean) amp += choose(pt,
            "Use Gain reduzido para a palhetada forte começar a quebrar e a leve continuar aberta. Compressão leve é opcional, sem apagar ataque e dinâmica.",
            "Use low Gain so strong picking begins to break up while light picking stays open. Light compression is optional, without erasing attack or dynamics.");
        else if (lead) amp += choose(pt,
            "Use Gain suficiente para a nota sustentar; OD/DS com Drive baixo pode servir de boost opcional. Compare COMP só se faltar sustain, sem comprimir de novo uma base já saturada.",
            "Use enough Gain to sustain notes; low-Drive OD/DS can be an optional boost. Compare COMP only if sustain is missing, without recompressing an already saturated base.");
        else amp += choose(pt,
            "Use ganho suficiente para definição de acordes e articulação: apenas o Gain necessário, sem exagerar. OD/DS de Drive baixo antes do PREAMP é opcional.",
            "Use sufficient gain for chord definition and articulation: only the Gain needed, without excess. Low-Drive OD/DS before PREAMP is optional.");
        if (style.family == ToneFamily::HighGain) {
            amp += choose(pt, " Ajuste NS sem cortar finais de notas.", " Set NS without cutting note tails.");
            if (!lead) amp += choose(pt, " COMP não é prioridade.", " COMP is not a priority.");
        }
        if (style.profile == ToneProfile::Worship && style.family == ToneFamily::Clean && composed)
            amp += choose(pt, " Uma leve saturação pode dar textura sem fundir notas.", " Light saturation can add texture without merging notes.");
        QString eq = choose(pt, "No EQ, controle low-end com Low Cut/Bass e mantenha médios úteis; recue Presence/agudos ou High Cut se houver fizz, sem perder ataque.", "In EQ, control low-end with Low Cut/Bass and retain useful mids; back off Presence/highs or use High Cut for fizz without losing attack.");
        switch (style.toneProblem) {
        case ToneProblem::Muddy: eq = choose(pt, "Para não embolar, controle Low Cut/Bass/Low Gain e ganho excessivo, sem tirar sustain nem médios. Em Dual L/R, evite duplicar low-end: confira overlap A/B e level balance na soma mono.", "To avoid muddiness, control Low Cut/Bass/Low Gain and excess gain without losing sustain or mids. In Dual L/R, avoid duplicated low-end: check A/B overlap and level balance in the mono sum."); break;
        case ToneProblem::Harsh: eq = choose(pt, "Para conter estridência, compare Presence, Treble/High Gain e High Cut sem apagar o ataque; ajuste pela guitarra e monitoração.", "To tame harshness, compare Presence, Treble/High Gain, and High Cut without erasing attack; fit guitar and monitoring."); break;
        case ToneProblem::NotDefined: eq = choose(pt, "Para mais definição, segure Bass/Low Gain e excesso de Gain, preservando médios e acordes. Low Cut só deve retirar grave inútil, não afinar o corpo.", "For more definition, control Bass/Low Gain and excessive Gain while retaining mids and chords. Low Cut should only remove useless lows, not thin body."); break;
        case ToneProblem::NeedsSustain: eq = choose(pt, "Preserve médios e equilibre níveis: volume percebido e cauda também ajudam a nota aparecer, sem resolver tudo aumentando Gain.", "Retain mids and balance levels: perceived volume and tails also help notes stand out, without solving everything by raising Gain."); break;
        default: if (style.toneProblem != ToneProblem::None) eq = equalization.join(' '); break;
        }
        if (style.profile == ToneProfile::Worship && style.toneProblem == ToneProblem::None)
            eq += choose(pt, " Perto de 100 Hz/8 kHz é uma referência inicial, não regra; a GT-10 oferece 110Hz e 8.00kHz.", " Near 100 Hz/8 kHz is an initial reference, not a rule; GT-10 offers 110Hz and 8.00kHz.");
        QString ambience = lead ? choose(pt,
            "Delay com Feedback moderado apoia a frase; Reverb Plate ou Hall1 pode ser mais presente, sem lavar o ataque.",
            "Delay with moderate Feedback supports phrases; Plate or Hall1 Reverb can be more present without washing out attack.") : rhythm ? choose(pt,
            "Na base, deixe Delay mínimo ou desligado e Reverb curto para manter palm mute e pausas definidos.",
            "For rhythm, leave Delay minimal or off and Reverb short to retain palm mutes and clear rests.") : choose(pt,
            "Use Delay rítmico com Feedback contido e Reverb Plate ou Hall1 atrás do ataque. Ajuste a cauda à próxima nota.",
            "Use rhythmic Delay with restrained Feedback and Plate or Hall1 Reverb behind attack. Fit tails to the next note.");
        if (style.family == ToneFamily::HighGain && !composed) ambience = choose(pt,
            "Defina o uso antes da ambiência: Delay mínimo/desligado para base; repetições moderadas e Reverb podem apoiar lead sem borrar a frase.",
            "Define the role before ambience: minimal/off Delay for rhythm; moderate repeats and Reverb can support lead without blurring phrases.");
        if (style.toneProblem == ToneProblem::Muddy) ambience += choose(pt, " Controle Reverb Time (decay) e Effect Level para a cauda não acumular.", " Control Reverb Time (decay) and Effect Level to prevent tail buildup.");
        if (style.toneProblem == ToneProblem::Harsh) ambience += choose(pt, " Confira High Cut nas caudas ásperas.", " Check High Cut on harsh tails.");
        if (style.spatialGoal == ToneSpatialGoal::Stereo) {
            ambience += choose(pt, " Para largura, use Delay Dual L/R com diferenças sutis. Chorus Stereo1/Stereo2 é opcional; use OUTPUT L/R com monitoração estéreo real, confira o routing e compatibilidade mono.", " For width, use Dual L/R Delay with subtle differences. Stereo1/Stereo2 Chorus is optional; use OUTPUT L/R with real stereo monitoring, check routing and mono compatibility.");
            renderedOptions.insert("delay", QJsonArray{"DualL/R"});
        }
        if (lead || (!rhythm && !(style.family == ToneFamily::HighGain && !composed)))
            renderedOptions.insert("reverb", QJsonArray{"Plate", "Hall1"});
        if (style.profile == ToneProfile::Worship && !spatial && !lead && !rhythm) {
            ambience += choose(pt, " Dual L/R é opcional se a música pedir largura.", " Dual L/R is optional if the music calls for width.");
            renderedOptions.insert("delay", QJsonArray{"DualL/R"});
        }
        paragraphs = QStringList{opening, amp, eq, ambience};
        if (style.family == ToneFamily::HighGain && !composed && !spatial)
            paragraphs = QStringList{opening, amp + " " + eq, ambience};
    }
    const Knowledge::LocalDocumentationIndex documentation = Knowledge::officialGT10DocumentationIndex();
    for (const auto &topic : documentation.search(spatial ? "Dual L/R" : "PREAMP Parameters", request.language, 2)) {
        const auto &entry = topic.entry;
        documentationTrace.append(QJsonObject{{"id", entry.id}, {"document", entry.sourceDocument},
            {"section", entry.section}, {"pageStart", entry.pageStart}, {"pageEnd", entry.pageEnd}});
    }
    if (!documentationTrace.isEmpty()) sources.append("OfficialDocumentation");

    QStringList adaptation;
    const bool patchAvailable = !request.context.root.value("effects").toArray().isEmpty();
    const bool hasPatch = style.usePatchContext && patchAvailable;
    meta.insert("patchContextAvailable", patchAvailable);
    meta.insert("patchAware", hasPatch);
    if (hasPatch) {
        reply.analysis = ToneAnalyzer().analyze(request.context, request.language);
        sources.append("PatchContext");
        const QJsonObject amp = effect(request.context, "preampA");
        if (!amp.isEmpty()) {
            adaptation.append(choose(pt, "No patch carregado, PREAMP A está ", "In the loaded patch, PREAMP A is ")
                + (amp.contains("enabled") ? (amp.value("enabled").toBool() ? "ON" : "OFF") : choose(pt, "desconhecido", "unknown"))
                + " (" + (amp.value("type").toString().isEmpty() ? "?" : amp.value("type").toString())
                + ", Gain " + (valueText(amp.value("gain")).isEmpty() ? "?" : valueText(amp.value("gain"))) + ").");
            ToneAdjustmentIntent intent;
            switch (style.family) {
            case ToneFamily::Clean: intent.direction = ToneDirection::MoreClean; break;
            case ToneFamily::Ambient: intent.direction = ToneDirection::MoreAmbient; break;
            default: intent.direction = ToneDirection::MoreCrunch; break;
            }
            if (implicitSpatial) intent.direction = style.spatialGoal == ToneSpatialGoal::Big ? ToneDirection::MoreDefined : ToneDirection::MoreWide;
            if (style.toneProblem == ToneProblem::Muddy) intent.direction = ToneDirection::LessMuddy;
            else if (style.toneProblem == ToneProblem::NotDefined) intent.direction = ToneDirection::MoreDefined;
            else if (style.toneProblem == ToneProblem::Harsh) intent.direction = ToneDirection::MoreDark;
            else if (style.toneProblem == ToneProblem::NeedsWidth) intent.direction = ToneDirection::MoreWide;
            else if (composed || problemOnly) intent.direction = ToneDirection::None;
            reply.advice = ToneAdjustmentAdvisor().advise(request.context, intent, request.language);
            if (!reply.advice.suggestions.isEmpty()) {
                const auto &s = reply.advice.suggestions.first();
                adaptation.append(s.reason);
                if (style.wantsValues)
                    adaptation.append(choose(pt, "Ponto de partida contextual, não valor ideal: ", "Contextual starting point, not an ideal setting: ")
                        + s.blockId + " / " + s.parameterId + " " + s.currentValue + " → " + s.suggestedTargetRange + ".");
            }
        }
        if (spatial) {
            const QString mode = request.context.root.value("routing").toObject().value("mode").toString();
            if (!mode.isEmpty())
                adaptation.append(choose(pt, "Channel Mode atual: ", "Current Channel Mode: ") + mode + ".");
            if (style.spatialGoal == ToneSpatialGoal::Stereo && !mode.isEmpty() && mode != "Dual L/R")
                adaptation.append(choose(pt, "Para a separação pedida, compare esse modo com Dual L/R e confira o caminho A/B até OUTPUT L/R; nada será alterado automaticamente.",
                    "For the requested separation, compare that mode with Dual L/R and check A/B paths to OUTPUT L/R; nothing is changed automatically."));
            const QJsonObject b = effect(request.context, "preampB");
            if (!b.value("type").toString().isEmpty())
                adaptation.append(choose(pt, "Compare também PREAMP B (", "Also compare PREAMP B (") + b.value("type").toString() + choose(pt, ") ao canal A, sem alterar automaticamente o routing.", ") against channel A, without automatically changing routing."));
        }
        const QJsonObject od = effect(request.context, "odDs");
        if (!implicitSpatial && style.family == ToneFamily::Clean && od.value("enabled").toBool())
            adaptation.append(choose(pt, "O OD/DS está ON: experimente desligá-lo primeiro para avaliar a base limpa, em vez de somar mais ganho.", "OD/DS is ON: try disabling it first to evaluate the clean base instead of adding gain."));
        for (const QString &id : (style.wantsDetail ? QStringList{"delay", "reverb"} : QStringList{})) {
            const QJsonObject block = effect(request.context, id);
            if (!block.isEmpty()) adaptation.append(ToneRules::blockLabel(id, request.language) + " "
                + (block.value("enabled").toBool() ? "ON" : "OFF") + " (" + block.value("type").toString() + ").");
        }
        for (const ToneFeature &f : reply.analysis.features) {
            if (f.id == "muddy_risk") adaptation.append(choose(pt, "Os parâmetros atuais indicam risco de acúmulo de graves; controle essa região antes de aumentar a ambiência.", "Current settings indicate possible low-end buildup; control that region before adding ambience."));
        }
    } else if (style.usePatchContext) adaptation.append(choose(pt,
        "Não tenho dados suficientes do patch atual para adaptar com segurança; essa orientação é conceitual, não uma leitura do preset.",
        "I lack sufficient current patch data to adapt safely; this guidance is conceptual, not a reading of the preset."));
    if (style.guitarContext == "single_coil") adaptation.append(choose(pt, "Com single coil/Strato, confira brilho e ruído antes de aumentar Presence ou compressão; preserve corpo pelos médios.", "With single coils/Strat, check brightness and noise before adding Presence or compression; retain body through mids."));
    if (style.guitarContext == "humbucker") adaptation.append(choose(pt, "Com humbucker, confira o ganho de entrada e os graves: pode ser necessário menos Gain/Bass para preservar definição.", "With humbuckers, check input gain and lows: less Gain/Bass may be needed to retain definition."));
    for (ToneModifier modifier : style.modifiers) {
        if ((style.toneProblem == ToneProblem::NeedsSustain && (modifier == ToneModifier::MoreSustain || modifier == ToneModifier::LessGain))
            || (style.toneProblem == ToneProblem::NotDefined && modifier == ToneModifier::MoreDefined)
            || (style.toneProblem == ToneProblem::Muddy && modifier == ToneModifier::LessMuddy)) continue;
        ToneDirection direction = ToneDirection::None;
        switch (modifier) {
        case ToneModifier::Brighter: direction = ToneDirection::MoreBright; break;
        case ToneModifier::Darker: direction = ToneDirection::MoreDark; break;
        case ToneModifier::MoreDefined: direction = ToneDirection::MoreDefined; break;
        case ToneModifier::LessMuddy: direction = ToneDirection::LessMuddy; break;
        case ToneModifier::MoreDynamic: direction = ToneDirection::MoreDynamic; break;
        case ToneModifier::MoreCompressed: direction = ToneDirection::MoreCompressed; break;
        case ToneModifier::MoreAmbient: direction = ToneDirection::MoreAmbient; break;
        case ToneModifier::MoreFocused: direction = ToneDirection::MoreFocused; break;
        case ToneModifier::MoreAggressive: direction = ToneDirection::MoreAggressive; break;
        case ToneModifier::MoreSmooth: direction = ToneDirection::MoreSmooth; break;
        case ToneModifier::LessGain: direction = ToneDirection::MoreClean; break;
        case ToneModifier::MoreGain: direction = ToneDirection::MoreCrunch; break;
        default: break;
        }
        if (hasPatch && direction != ToneDirection::None) {
            ToneAdjustmentIntent intent; intent.direction = direction;
            const ToneAdvice advice = ToneAdjustmentAdvisor().advise(request.context, intent, request.language);
            if (!advice.suggestions.isEmpty()) adaptation.append(advice.suggestions.first().reason);
        }
        if (!hasPatch && direction != ToneDirection::None) {
            // Directions only: numeric deltas remain the existing advisor's
            // responsibility once an actual patch context is available.
            switch (modifier) {
            case ToneModifier::Brighter: adaptation.append(choose(pt, "Para brilho, suba Treble/Presence aos poucos sem criar aspereza.", "For brightness, raise Treble/Presence gradually without harshness.")); break;
            case ToneModifier::Darker: adaptation.append(choose(pt, "Para escurecer, recue Treble/Presence sem perder articulação.", "For a darker sound, back off Treble/Presence without losing articulation.")); break;
            case ToneModifier::MoreDefined:
            case ToneModifier::LessMuddy: adaptation.append(choose(pt, "Para definição, controle Bass/Low Gain e excesso de Gain antes de somar efeitos.", "For definition, control Bass/Low Gain and excess Gain before adding effects.")); break;
            case ToneModifier::MoreDynamic: adaptation.append(choose(pt, "Para dinâmica, recue compressão e ganho e compare ataques leves e fortes.", "For dynamics, reduce compression and gain and compare soft and strong attacks.")); break;
            case ToneModifier::MoreCompressed: adaptation.append(choose(pt, "Para nivelar mais, suba Sustain do COMP aos poucos, preservando o ataque.", "For more leveling, raise COMP Sustain gradually while retaining attack.")); break;
            case ToneModifier::MoreAmbient: adaptation.append(choose(pt, "Para ambiente, acrescente Delay/Reverb gradualmente sem encobrir a próxima nota.", "For ambience, add Delay/Reverb gradually without masking the next note.")); break;
            case ToneModifier::MoreFocused: adaptation.append(choose(pt, "Para foco, preserve médios e reduza graves excessivos e largura se ela dispersar a frase.", "For focus, retain mids and reduce excessive lows and width if they disperse the phrase.")); break;
            case ToneModifier::MoreAggressive: adaptation.append(choose(pt, "Para agressividade, compare ataque e Gain mantendo palm mutes legíveis.", "For aggression, compare attack and Gain while keeping palm mutes legible.")); break;
            case ToneModifier::MoreSmooth: adaptation.append(choose(pt, "Para suavidade, recue agudos ásperos no EQ sem apagar os médios.", "For smoothness, tame harsh highs in EQ without removing mids.")); break;
            case ToneModifier::LessGain: adaptation.append(choose(pt, "Para menos ganho, recue Gain/Drive e compense apenas o volume percebido.", "For less gain, back off Gain/Drive and compensate only perceived volume.")); break;
            case ToneModifier::MoreGain: adaptation.append(choose(pt, "Para mais ganho, suba Gain em um estágio por vez sem perder definição.", "For more gain, raise Gain in one stage at a time without losing definition.")); break;
            default: break;
            }
        }
        if (modifier == ToneModifier::Warmer) adaptation.append(choose(pt, "Para mais corpo, trabalhe médios e suavize agudos antes de reforçar graves.", "For more body, work the mids and soften highs before boosting lows."));
        if (modifier == ToneModifier::LessAmbient) adaptation.append(choose(pt, "Para menos ambiente, reduza Effect Level de Delay/Reverb e encurte a cauda até a próxima nota se destacar.", "For less ambience, reduce Delay/Reverb Effect Level and shorten the tail until the next note stands out."));
        if (modifier == ToneModifier::MoreSustain) adaptation.append(choose(pt, "Para sustain, compare COMP e OD/DS como alternativas, sem aumentar os dois de uma vez nem eliminar a dinâmica.", "For sustain, compare COMP and OD/DS as alternatives without raising both at once or removing dynamics."));
    }
    if (style.wantsValues && !hasPatch) {
        const auto capability = capabilities.parameterCapability("preampA", "JC Clean", "Gain");
        if (capability.available) adaptation.append(choose(pt, "Faixa técnica de Gain no catálogo: ", "Catalog technical Gain range: ")
            + capability.minimum + "–" + capability.maximum
            + choose(pt, ". Isso não define valores ideais; ajuste pela resposta ao toque e monitoração.", ". This does not define ideal settings; adjust by touch response and monitoring."));
    }
    if (!adaptation.isEmpty()) {
        // Compact builds retain 3–4 coherent paragraphs, folding only relevant
        // qualifiers/context into the last paragraph instead of a patch dump.
        if (!style.wantsDetail && paragraphs.size() >= 4)
            paragraphs.last() += " " + adaptation.join(' ');
        else paragraphs.append(adaptation.join(' '));
    }
    paragraphs.removeAll(QString());
    meta.insert("responseParagraphCount", paragraphs.size());
    sources.removeDuplicates(); addressed.removeDuplicates();
    meta.insert("knowledgeSources", QJsonArray::fromStringList(sources));
    meta.insert("documentationTrace", documentationTrace);
    meta.insert("recipeSteps", steps);
    meta.insert("expectedSubsystems", QJsonArray::fromStringList(addressed));
    meta.insert("recipeGoals", QJsonArray::fromStringList(implicitSpatial
        ? (style.spatialGoal == ToneSpatialGoal::Big ? QStringList{"body", "density", "depth", "level_balance"}
            : QStringList{"lr_separation", "stereo_output", "mono_compatibility"}) : recipe.goals));
    meta.insert("recipeAvoid", QJsonArray::fromStringList(recipe.avoid));
    meta.insert("exactRecipe", exactRecipe);
    reply.text = paragraphs.join("\n\n");
    meta.insert("responseCharacterCount", reply.text.size());
    meta.insert("renderedOptions", renderedOptions);
    QJsonObject optionCounts;
    for (auto it = renderedOptions.constBegin(); it != renderedOptions.constEnd(); ++it)
        optionCounts.insert(it.key(), it.value().toArray().size());
    meta.insert("optionCountPerBlock", optionCounts);
    return reply;
}

} }
