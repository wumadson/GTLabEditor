#include "ai/AIContextBuilder.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTextStream>

using namespace Ai;

#define AI_STRINGIFY_DETAIL(value) #value
#define AI_STRINGIFY(value) AI_STRINGIFY_DETAIL(value)

namespace {
int failures = 0;
int assertions = 0;

void expect(bool condition, const QString &name)
{
    ++assertions;
    if (!condition) {
        ++failures;
        QTextStream(stderr) << "FAIL: " << name << '\n';
    }
}

ParameterValue parameter(const QString &id, int raw,
                         const QString &display,
                         const QString &enumText = QString(),
                         const QString &unit = QString())
{
    ParameterValue value;
    value.id = id;
    value.address = "0A00:01";
    value.hasRaw = true;
    value.raw = raw;
    value.rawHex = QString::number(raw, 16).toUpper();
    value.display = display;
    value.enumText = enumText;
    value.unit = unit;
    return value;
}

SignalChainModule chainModule(const QString &id, const QString &name,
                              bool enabled = true)
{
    SignalChainModule module;
    module.id = id;
    module.name = name;
    module.raw = "44";
    module.normalized = "04";
    module.position = 1;
    module.hasEnabled = true;
    module.enabled = enabled;
    return module;
}

EffectSnapshot effect(const QString &id, bool enabled,
                      const QString &type)
{
    EffectSnapshot result;
    result.id = id;
    result.hasEnabled = true;
    result.enabled = enabled;
    result.type = parameter("type", 1, type, type);
    return result;
}

PatchSnapshot referenceSnapshot()
{
    PatchSnapshot snapshot;
    snapshot.device.model = "BOSS GT-10";
    snapshot.device.connected = true;
    snapshot.device.confirmed = true;
    snapshot.source.provenance = "deviceTemporaryBuffer";
    snapshot.source.reliability = "deviceConfirmed";
    snapshot.patch.displayNumber = "U29-1";
    snapshot.patch.name = "WC ALL-IN-ONE";
    snapshot.patch.bank = 29;
    snapshot.patch.slot = 1;
    snapshot.patch.location = "user";
    snapshot.patchParameters.append(parameter("masterBpm", 60, "60", {}, "BPM"));
    snapshot.patchParameters.append(parameter("patchLevel", 50, "100 %", {}, "%"));
    snapshot.routing.append(parameter("channelMode", 2, "DualL/R", "DualL/R"));
    snapshot.routing.append(parameter("channelSelect", 0, "Channel A", "Channel A"));
    snapshot.routing.append(parameter("channelDelay", 3, "3ms", {}, "ms"));
    snapshot.routing.append(parameter("dynamicSense", 50, "50"));

    snapshot.signalChain.commonPrefix = {
        chainModule("module00", "COMP"),
        chainModule("module01", "OD/DS")};
    snapshot.signalChain.split = chainModule("split", "DIVIDER");
    snapshot.signalChain.split.hasEnabled = false;
    snapshot.signalChain.pathA = {
        chainModule("module02", "PREAMP A"),
        chainModule("module07", "DELAY")};
    snapshot.signalChain.pathB = {
        chainModule("module03", "PREAMP B")};
    snapshot.signalChain.merge = chainModule("merge", "MIXER");
    snapshot.signalChain.merge.hasEnabled = false;
    snapshot.signalChain.commonSuffix = {
        chainModule("module09", "REVERB")};
    snapshot.signalChain.channelMode = "DUAL L/R";
    snapshot.signalChain.channelSelect = 0;

    EffectSnapshot global = effect("preampGlobal", true, QString());
    global.parameters = snapshot.routing;
    snapshot.effects.append(global);

    EffectSnapshot preampA = effect("preampA", true, "(Match) MATCH Drive");
    preampA.parameters.append(parameter("gain", 55, "55"));
    preampA.parameters.append(parameter("bass", 40, "40"));
    preampA.parameters.append(parameter("middle", 60, "60"));
    preampA.parameters.append(parameter("treble", 57, "57"));
    preampA.parameters.append(parameter("presence", 30, "30"));
    preampA.parameters.append(parameter("level", 75, "75"));
    snapshot.effects.append(preampA);

    EffectSnapshot preampB = effect("preampB", true, "(VO) VO Drive");
    preampB.parameters.append(parameter("gain", 48, "48"));
    snapshot.effects.append(preampB);

    EffectSnapshot delay = effect("delay", true, "DualL/R");
    delay.parameters.append(parameter("directLevel", 100, "100"));
    delay.parameters.append(parameter("dualDelay1Time", 1,
                                     "dotted eighth note"));
    delay.parameters.append(parameter("dualDelay1Feedback", 45, "45"));
    delay.parameters.append(parameter("dualDelay1HighCut", 4, "4.00kHz"));
    delay.parameters.append(parameter("dualDelay1Level", 60, "60"));
    delay.parameters.append(parameter("dualDelay2Time", 2, "quarter note"));
    delay.parameters.append(parameter("dualDelay2Feedback", 45, "45"));
    delay.parameters.append(parameter("dualDelay2HighCut", 4, "4.00kHz"));
    delay.parameters.append(parameter("dualDelay2Level", 55, "55"));
    snapshot.effects.append(delay);

    EffectSnapshot reverb = effect("reverb", true, "Mod");
    reverb.parameters.append(parameter("reverbTime", 90, "9.0 sec", {}, "sec"));
    reverb.parameters.append(parameter("preDelay", 100, "100", {}, "ms"));
    reverb.parameters.append(parameter("effectLevel", 40, "40"));
    reverb.parameters.append(parameter("directLevel", 100, "100"));
    snapshot.effects.append(reverb);

    EffectSnapshot eq = effect("eq", true, QString());
    eq.type = ParameterValue();
    eq.parameters.append(parameter("lowCut", 1, "110Hz"));
    eq.parameters.append(parameter("lowGain", 0, "0", {}, "dB"));
    snapshot.effects.append(eq);

    EffectSnapshot pedalFxFv = effect("pedalFxFv", true, "Foot Volume");
    pedalFxFv.parameters.append(parameter("footVolumeLevel", 81, "81"));
    pedalFxFv.parameters.append(parameter("footVolumeMinimum", 12, "12"));
    pedalFxFv.parameters.append(parameter("footVolumeMaximum", 99, "99"));
    pedalFxFv.parameters.append(parameter("footVolumeCurve", 2, "Normal",
                                           "Normal"));
    snapshot.effects.append(pedalFxFv);

    snapshot.effects.append(effect("odDs", false, "(Blues) Blues OD"));
    snapshot.effects.append(effect("chorus", false, "Stereo1"));
    snapshot.effects.append(effect("fx1", false, "Tremolo"));

    EffectSnapshot off = effect("fx2", false, "Harmonist");
    ParameterValue unavailable;
    unavailable.id = "hiddenParameter";
    unavailable.available = false;
    unavailable.unavailableReason = "not_for_algorithm";
    unavailable.address = "/Users/private/should-not-leak";
    off.parameters.append(unavailable);
    snapshot.effects.append(off);
    return snapshot;
}

QJsonObject findEffect(const QJsonObject &root, const QString &id)
{
    for (const QJsonValue &value : root.value("effects").toArray()) {
        if (value.toObject().value("id").toString() == id)
            return value.toObject();
    }
    return {};
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const PatchSnapshot snapshot = referenceSnapshot();
    const AIContextBuilder builder;
    const AIContext compact = builder.build(snapshot, AIContextDetail::Compact);
    const AIContext detailed = builder.build(snapshot, AIContextDetail::Detailed);
    const QByteArray compactJson = AIContextBuilder::serialize(compact);
    const QByteArray detailedJson = AIContextBuilder::serialize(detailed);

    expect(compact.root.value("detail") == "compact", "1 compact mode");
    expect(detailed.root.value("detail") == "detailed", "2 detailed mode");
    expect(compact.root.value("patch").toObject().value("number") == "U29-1"
           && compact.root.value("patch").toObject().value("name")
              == "WC ALL-IN-ONE", "3 patch identity");
    expect(compact.root.value("routing").toObject().value("mode")
           == "Dual L/R", "4 routing normalization");
    expect(compact.root.value("signalChain").toObject()
               .value("pathA").toArray().size() == 2
           && compact.root.value("signalChain").toObject()
               .value("pathB").toArray().size() == 1,
           "5 signal chain branches preserved");
    expect(findEffect(compact.root, "preampA").value("type")
           == "MATCH Drive", "6 family prefix removed");
    expect(AIContextBuilder::normalizeLabel("(Blues) Blues OD")
               == "Blues OD"
           && AIContextBuilder::normalizeLabel("(VO) VO Drive")
               == "VO Drive", "6b confirmed family labels normalized");
    expect(findEffect(compact.root, "delay").value("type")
           == "Dual L/R", "7 DualL/R normalized");
    expect(findEffect(compact.root, "reverb").value("type")
           == "Modulate", "8 confirmed Mod label normalized");
    expect(findEffect(compact.root, "chorus").value("type")
           == "Stereo 1", "9 Stereo1 normalized");
    expect(!findEffect(compact.root, "fx2").contains("parameters"),
           "10 OFF effect omits compact parameters");
    expect(findEffect(compact.root, "delay").value("delay1").toObject()
               .value("feedback") == "45"
           && findEffect(compact.root, "delay").value("delay2").toObject()
               .value("time") == "quarter note",
           "11 Dual Delay grouped from canonical ids");
    expect(findEffect(compact.root, "delay").value("delay1").toObject()
               .value("time") == "dotted eighth note",
           "12 dotted note spelling preserved");
    QFile midiXml(QString::fromLatin1(AI_STRINGIFY(AI_CONTEXT_MIDI_XML)));
    expect(midiXml.open(QIODevice::ReadOnly), "12b shared MIDI labels open");
    QTextStream midiText(&midiXml);
    const QString midiLabels = midiText.readAll();
    expect(!midiLabels.contains("doted ")
           && midiLabels.contains("dotted eighth note"),
           "12c dotted spelling corrected at shared source");
    expect(!compactJson.contains("\"raw\"")
           && !compactJson.contains("rawHex")
           && !compactJson.contains("address")
           && !compactJson.contains("unavailableReason"),
           "13 compact excludes all internal parameter metadata");
    expect(detailedJson.contains("address")
           && detailedJson.contains("rawHex")
           && detailedJson.contains("unavailableReason"),
           "14 detailed includes diagnostic metadata");
    expect(!compactJson.contains("/Users/")
           && !detailedJson.contains("/Users/"),
           "15 local paths never enter AI context");
    expect(!compactJson.contains("controllers")
           && !detailedJson.contains("controllers")
           && !compactJson.contains("system"),
           "16 controllers and System data excluded");
    expect(!AIContextBuilder::toText(compact).isEmpty()
           && AIContextBuilder::toText(compact).contains("A: PREAMP A")
           && AIContextBuilder::toText(compact).contains("Gain: 55"),
           "17 readable text projection");
    expect(snapshot.effects.last().parameters.first().address
           == "/Users/private/should-not-leak",
           "18 builder does not mutate snapshot");
    expect(AIContextBuilder::serialize(
               builder.build(snapshot, AIContextDetail::Compact))
               == compactJson,
           "18b deterministic compact output");
    expect(findEffect(compact.root, "preampA").value("gain") == "55"
           && findEffect(compact.root, "delay").value("type")
               == "Dual L/R",
           "18c display fallback and enum preference");
    expect(!compactJson.contains("username")
           && !compactJson.contains("midiPort")
           && !compactJson.contains("git")
           && !compactJson.contains("systemSource")
           && !compactJson.contains("fileName")
           && !compactJson.contains("F0 41"),
           "18d private runtime and full SysEx fields excluded");
    expect(findEffect(compact.root, "preampGlobal").isEmpty()
           && !findEffect(detailed.root, "preampGlobal").isEmpty(),
           "18e preampGlobal omitted only from compact effects");
    const QJsonObject routing = compact.root.value("routing").toObject();
    expect(routing.value("channelSelect") == "Channel A"
           && routing.value("channelDelay") == "3 ms"
           && routing.value("dynamicSense") == "50",
           "18f compact routing consolidates preamp globals");
    const QJsonObject patchParameters = compact.root.value("patchParameters").toObject();
    expect(patchParameters.value("masterBpm") == "60 BPM"
           && patchParameters.value("patchLevel") == "100 %"
           && !patchParameters.value("masterBpm").toString().contains("BPM BPM"),
           "18g patch parameters are compact maps with safe units");
    expect(findEffect(compact.root, "preampA").value("bass") == "40"
           && findEffect(compact.root, "preampB").value("gain") == "48",
           "18h both preamps retain tonal parameters");
    expect(findEffect(compact.root, "reverb").value("preDelay") == "100 ms"
           && findEffect(compact.root, "reverb").value("reverbTime") == "9.0 sec",
           "18i reverb is compact and unit safe");
    expect(findEffect(compact.root, "eq").value("lowCut") == "110Hz"
           && findEffect(compact.root, "eq").value("lowGain") == "0 dB",
           "18j EQ is a compact key/value map");
    const QJsonObject compactFootVolume = findEffect(compact.root, "pedalFxFv");
    expect(compactFootVolume.value("footVolumeLevel") == "81"
           && compactFootVolume.value("footVolumeMinimum") == "12"
           && compactFootVolume.value("footVolumeMaximum") == "99"
           && compactFootVolume.value("footVolumeCurve") == "Normal",
           "18j2 Foot Volume values survive compact projection");
    const QJsonObject detailedFootVolume = findEffect(detailed.root, "pedalFxFv");
    const QJsonArray detailedFootVolumeParameters =
        detailedFootVolume.value("parameters").toArray();
    expect(detailedFootVolumeParameters.at(0).toObject().value("raw").toInt()
               == 81
           && detailedFootVolumeParameters.at(0).toObject().value("value")
               == "81",
           "18j3 Foot Volume raw 81 survives detailed projection");
    expect(!compactJson.contains("module00")
           && !compactJson.contains("module07")
           && compact.root.value("signalChain").toObject()
               .value("pathA").toArray().size() == 2,
           "18k compact chain removes internal ids and preserves paths");
    expect(!compact.root.value("signalChain").toObject()
               .value("split").toObject().contains("enabled")
           && !compact.root.value("signalChain").toObject()
               .value("merge").toObject().contains("enabled"),
           "18l non-applicable chain enabled state omitted");

    QFile fixture(QString::fromLatin1(AI_STRINGIFY(AI_CONTEXT_FIXTURE_DIR))
                  + "/ai_context_wc_all_in_one.json");
    expect(fixture.open(QIODevice::ReadOnly), "19 regression fixture opens");
    const QJsonDocument golden = QJsonDocument::fromJson(fixture.readAll());
    const bool fixtureMatches = golden.isObject()
        && compact.root == golden.object();
    if (!fixtureMatches)
        QTextStream(stderr) << compactJson;
    expect(fixtureMatches, "20 U29-1 WC ALL-IN-ONE compact regression");

    QTextStream(stdout) << "AI Context Builder tests: assertions="
                        << assertions << " failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
