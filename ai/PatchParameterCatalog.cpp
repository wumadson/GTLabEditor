#include "ai/PatchParameterCatalog.h"

#include <QMap>
#include <QSet>
#include <QStringList>

namespace Ai {
namespace {

ParameterAddress address(const char *block, const char *offset,
                         bool twoByte = false)
{
    return {QString::fromLatin1(block), QString::fromLatin1(offset), twoByte};
}

ParameterDescriptor parameter(const char *id, const char *block,
                              const char *offset, bool twoByte = false,
                              const char *unit = nullptr)
{
    ParameterDescriptor descriptor;
    descriptor.id = QString::fromLatin1(id);
    descriptor.address = address(block, offset, twoByte);
    if (unit)
        descriptor.unit = QString::fromLatin1(unit);
    return descriptor;
}

ParameterDescriptor conditional(ParameterDescriptor descriptor,
                                const ParameterAddress &controller,
                                std::initializer_list<int> values,
                                const char *reason)
{
    descriptor.rule = AvailabilityRule::ControllerIn;
    descriptor.controller = controller;
    for (int value : values)
        descriptor.controllerValues.append(value);
    descriptor.unavailableReason = QString::fromLatin1(reason);
    return descriptor;
}

int byteAt(const QMap<QString, QString> &blocks,
           const ParameterAddress &where, bool *ok = nullptr)
{
    bool offsetOk = false;
    const int offset = where.offset.toInt(&offsetOk, 16);
    const QString payload = blocks.value(where.block);
    const bool valid = offsetOk && offset >= 0
        && payload.size() >= (offset + (where.twoByte ? 2 : 1)) * 2;
    if (ok)
        *ok = valid;
    if (!valid)
        return 0;
    bool valueOk = false;
    int value = payload.mid(offset * 2, 2).toInt(&valueOk, 16);
    if (where.twoByte) {
        bool lowOk = false;
        const int low = payload.mid((offset + 1) * 2, 2).toInt(&lowOk, 16);
        value = value * 128 + low;
        valueOk = valueOk && lowOk;
    }
    if (ok)
        *ok = valueOk;
    return value;
}

EffectDescriptor simpleEffect(const char *id, const char *block,
                              const char *stateOffset,
                              const char *typeOffset = nullptr)
{
    EffectDescriptor effect;
    effect.id = QString::fromLatin1(id);
    effect.state = address(block, stateOffset);
    if (typeOffset)
        effect.type = address(block, typeOffset);
    return effect;
}

const QMap<int, QStringList> &fxAddressMap()
{
    static const QMap<int, QStringList> map = {
        {0x00, {"0:0D","0:0E","0:0F","0:10","0:11","0:12","0:13"}},
        {0x01, {"0:14","0:15","0:16","0:17","0:18","0:19","0:1A"}},
        {0x02, {"3:30","3:31","3:32","3:33","3:34","3:35"}},
        {0x03, {"0:02","0:03","0:04","0:05","0:06"}},
        {0x04, {"0:07","0:08","0:09","0:0A","0:0B","0:0C"}},
        {0x05, {"3:37","3:38","3:39","3:3A","3:3B","3:3C","3:3D","3:3E","3:3F","3:40","3:36"}},
        {0x06, {"0:58","0:59","0:5A","0:5B","0:5C","0:5D","0:5E","0:5F","0:60","0:61","0:62"}},
        {0x07, {"3:1F","3:20","3:21","3:22","3:23"}},
        {0x08, {"3:24","3:25","3:26","3:28","3:27"}},
        {0x09, {"0:3E","0:3F"}},
        {0x0A, {"1:2D","1:2E","1:2F","1:30","1:31","1:32","1:33"}},
        {0x0B, {"1:3B","1:3C","1:3D","1:3E","1:3F","1:40","1:41","1:42"}},
        {0x0C, {"1:43","1:44","1:45","1:46","1:47","1:48","1:49","1:4A","1:4B","1:4C","1:4D","1:4E","1:4F","1:50","1:52","1:53","1:51"}},
        {0x0D, {"1:34","1:35","1:36","1:37","1:38","1:39","1:3A"}},
        {0x0E, {"1:15","1:16","1:17"}},
        {0x0F, {"1:06","1:07","1:08","1:09","1:0A:2","1:0C","1:0D","1:0E","1:0F","1:10:2","1:12","1:13","1:14"}},
        {0x10, {"0:63","X:68","0:64","0:65:2","0:67","0:68","0:69:2","0:6B","0:6C","0:6D"}},
        {0x11, {"1:54","1:55","1:56","1:57","1:58","1:59","1:5A","1:5B"}},
        {0x12, {"3:1C","3:1D","3:1E"}},
        {0x13, {"3:29","3:2A","3:2B","3:2C","3:2D","3:2E","3:2F"}},
        {0x14, {"0:40","0:41","0:43","0:42","0:44","0:45","0:46"}},
        {0x15, {"0:47","0:48","0:49","0:4A","0:4B","0:4C"}},
        {0x16, {"0:1E","0:1F","0:20","0:21","0:22","0:23","0:24","0:25"}},
        {0x17, {"0:26","0:27","0:28","0:29","0:2A","0:2B","0:2C","0:2D"}},
        {0x18, {"0:1B","0:1C","0:1D"}},
        {0x19, {"1:18","1:19","1:1A","1:1B","1:1C","1:1D"}},
        {0x1A, {"0:37","0:38","0:39"}},
        {0x1B, {"0:2E","0:2F","0:30","0:31","0:32"}},
        {0x1C, {"0:55","0:56","0:57"}},
        {0x1D, {"0:33","0:34","0:35","0:36"}},
        {0x1E, {"0:3A","0:3B","0:3C","0:3D"}},
        {0x1F, {"0:4D","0:4E","0:4F","0:50","0:51","0:52","0:53","0:54"}},
        {0x20, {"1:1E","1:1F","1:20","1:21","1:22","1:23","1:24","1:25","1:26"}},
        {0x21, {"1:27:2","1:29","1:2A","1:2B","1:2C"}}
    };
    return map;
}

} // namespace

QString ParameterAddress::text() const
{
    return block + ":" + offset;
}

QList<ParameterDescriptor> PatchParameterCatalog::patchParameters()
{
    return {
        parameter("patchLevel", "0A00", "60"),
        parameter("masterBpm", "0A00", "66", true, "BPM")
    };
}

QList<ParameterDescriptor> PatchParameterCatalog::routingParameters()
{
    return {
        parameter("channelMode", "0100", "01"),
        parameter("channelSelect", "0100", "02"),
        parameter("channelDelay", "0100", "03"),
        parameter("dynamicSense", "0100", "04")
    };
}

bool PatchParameterCatalog::preampBrightAvailable(int type, int customType)
{
    static const QSet<int> brightTypes = {
        0x00, 0x01, 0x02, 0x04, 0x05, 0x06,
        0x08, 0x09, 0x0B, 0x12, 0x13, 0x14
    };
    static const QSet<int> brightCustomTypes = {0x00, 0x01, 0x02, 0x04};
    return brightTypes.contains(type)
        || (type == 0x27 && brightCustomTypes.contains(customType));
}

bool PatchParameterCatalog::isAvailable(
    const ParameterDescriptor &descriptor,
    const QMap<QString, QString> &blocks)
{
    if (descriptor.rule == AvailabilityRule::Always)
        return true;
    bool ok = false;
    const int raw = byteAt(blocks, descriptor.controller, &ok);
    if (!ok)
        return false;
    if (descriptor.rule == AvailabilityRule::PreampBright) {
        const int typeOffset = descriptor.controller.offset.toInt(nullptr, 16);
        bool customOk = false;
        const int customType = byteAt(blocks,
            {descriptor.controller.block,
             QString::number(typeOffset + 0x11, 16).toUpper(), false},
            &customOk);
        return preampBrightAvailable(raw, customOk ? customType : -1);
    }
    if (descriptor.rule == AvailabilityRule::CustomWah) {
        bool modeOk = false;
        const int mode = byteAt(blocks, {"0A00", "45", false}, &modeOk);
        return modeOk && (mode == 3 || mode == 5) && raw == 0x06;
    }
    const bool contains = descriptor.controllerValues.contains(raw);
    if (descriptor.rule == AvailabilityRule::ControllerNotEquals)
        return !contains;
    return contains;
}

QString PatchParameterCatalog::canonicalId(const QString &source,
                                           const QString &fallback)
{
    QStringList words = source.simplified().split(' ', Qt::SkipEmptyParts);
    QString result;
    for (int index = 0; index < words.size(); ++index) {
        QString word = words.at(index);
        word.remove(QRegExp("[^A-Za-z0-9]"));
        if (word.isEmpty())
            continue;
        word = word.toLower();
        if (result.isEmpty()) {
            result = word;
        } else {
            result += word.left(1).toUpper() + word.mid(1);
        }
    }
    if (result == QStringLiteral("feedBack"))
        result = QStringLiteral("feedback");
    return result.isEmpty() ? fallback : result;
}

QList<ParameterDescriptor> PatchParameterCatalog::fxParameters(
    const QString &baseBlock, int algorithmRaw)
{
    QList<ParameterDescriptor> result;
    const QStringList specs = fxAddressMap().value(algorithmRaw);
    QMap<QString, int> ids;
    for (const QString &spec : specs) {
        const QStringList parts = spec.split(':');
        if (parts.size() < 2)
            continue;
        const int relative = parts.at(0) == "X" ? -1 : parts.at(0).toInt();
        const int base = baseBlock.left(2).toInt(nullptr, 16);
        const QString block = relative < 0 ? "0A00"
            : QString("%1%2").arg(base + relative, 2, 16, QChar('0'))
                  .arg("00").toUpper();
        const QString offset = parts.at(1);
        const bool twoByte = parts.size() > 2 && parts.at(2) == "2";
        QString id = QString("parameter%1%2")
            .arg(relative < 0 ? "External" : QString::number(relative))
            .arg(offset);
        const int duplicate = ids.value(id, 0);
        ids[id] = duplicate + 1;
        if (duplicate)
            id += QString::number(duplicate + 1);
        ParameterDescriptor descriptor;
        descriptor.id = id;
        descriptor.address = {block, offset, twoByte};

        if (algorithmRaw == 0x14 && QStringList({"42","43","44","45","46"}).contains(offset))
            descriptor = conditional(descriptor, {baseBlock, "40", false}, {0x00}, "fx_visible_when");
        else if (algorithmRaw == 0x0C && QStringList({"45","46","51"}).contains(offset))
            descriptor = conditional(descriptor, {QString("%1%2").arg(base + 1, 2, 16, QChar('0')).arg("00").toUpper(), "43", false}, {0x00, 0x01}, "fx_visible_when");
        else if (algorithmRaw == 0x0C && QStringList({"47","48"}).contains(offset))
            descriptor = conditional(descriptor, {QString("%1%2").arg(base + 1, 2, 16, QChar('0')).arg("00").toUpper(), "43", false}, {0x00}, "fx_visible_when");
        else if (algorithmRaw == 0x0F && QStringList({"0D","0E","0F","10","12"}).contains(offset))
            descriptor = conditional(descriptor, {QString("%1%2").arg(base + 1, 2, 16, QChar('0')).arg("00").toUpper(), "06", false}, {0x01, 0x02}, "fx_visible_when");
        else if (algorithmRaw == 0x10 && QStringList({"68","69","6B"}).contains(offset))
            descriptor = conditional(descriptor, {baseBlock, "63", false}, {0x01, 0x02}, "fx_visible_when");
        result.append(descriptor);
    }
    if (algorithmRaw == 0x10) {
        const int base = baseBlock.left(2).toInt(nullptr, 16);
        for (int note = 0; note < 12; ++note) {
            ParameterDescriptor scale = parameter(
                QString("userScale1Note%1").arg(note + 1).toLatin1().constData(),
                baseBlock.toLatin1().constData(),
                QString::number(0x6E + note, 16).toUpper().toLatin1().constData());
            result.append(scale);
        }
        for (int note = 0; note < 12; ++note) {
            const QString block = note < 6 ? baseBlock
                : QString("%1%2").arg(base + 1, 2, 16, QChar('0'))
                      .arg("00").toUpper();
            const int offset = note < 6 ? 0x7A + note : note - 6;
            ParameterDescriptor scale = parameter(
                QString("userScale2Note%1").arg(note + 1).toLatin1().constData(),
                block.toLatin1().constData(),
                QString::number(offset, 16).rightJustified(2, '0').toUpper()
                    .toLatin1().constData());
            result.append(scale);
        }
    }
    return result;
}

QList<EffectDescriptor> PatchParameterCatalog::effects(
    const QMap<QString, QString> &blocks)
{
    QList<EffectDescriptor> result;

    EffectDescriptor ns1 = simpleEffect("ns1", "0A00", "71");
    ns1.parameters = {parameter("threshold", "0A00", "72"), parameter("release", "0A00", "73"), parameter("detect", "0A00", "74")};
    result.append(ns1);

    EffectDescriptor comp = simpleEffect("comp", "0000", "40", "41");
    comp.parameters = {
        conditional(parameter("sustain", "0000", "42"), address("0000", "41"), {0}, "compressor_type_only"),
        conditional(parameter("attack", "0000", "43"), address("0000", "41"), {0}, "compressor_type_only"),
        conditional(parameter("threshold", "0000", "44"), address("0000", "41"), {1}, "limiter_type_only"),
        conditional(parameter("release", "0000", "45"), address("0000", "41"), {1}, "limiter_type_only"),
        parameter("tone", "0000", "46"), parameter("level", "0000", "47")};
    result.append(comp);

    {
        const QPair<QString, QString> slot("fx1", "0200");
        EffectDescriptor fx = simpleEffect(slot.first.toLatin1().constData(), slot.second.toLatin1().constData(), "00", "01");
        bool ok = false;
        const int type = byteAt(blocks, fx.type, &ok);
        if (ok)
            fx.parameters = fxParameters(slot.second, type);
        result.append(fx);
    }

    EffectDescriptor odds = simpleEffect("odDs", "0000", "70", "71");
    odds.parameters = {parameter("drive", "0000", "72"), parameter("bottom", "0000", "73"), parameter("tone", "0000", "74"), parameter("effectLevel", "0000", "75"), parameter("directLevel", "0000", "76"), parameter("solo", "0000", "77"), parameter("soloLevel", "0000", "78")};
    const QList<QPair<QString, QString>> customOdParameters = {
        {"customType","79"},{"customBottom","7A"},{"customTop","7B"},
        {"customLow","7C"},{"customHigh","7D"}};
    for (const auto &entry : customOdParameters)
        odds.parameters.append(conditional(parameter(entry.first.toLatin1().constData(), "0000", entry.second.toLatin1().constData()), address("0000", "71"), {0x19}, "custom_od_ds_only"));
    result.append(odds);

    EffectDescriptor preampGlobal = simpleEffect(
        "preampGlobal", "0100", "00");
    preampGlobal.parameters = {
        parameter("channelMode", "0100", "01"),
        parameter("channelSelect", "0100", "02"),
        parameter("channelDelay", "0100", "03"),
        parameter("dynamicSense", "0100", "04")};
    result.append(preampGlobal);

    for (const auto &channel : {qMakePair(QString("preampA"), 0x10), qMakePair(QString("preampB"), 0x30)}) {
        const QString base = QString::number(channel.second, 16).toUpper();
        EffectDescriptor preamp = simpleEffect(channel.first.toLatin1().constData(), "0100", "00");
        preamp.type = address("0100", base.toLatin1().constData());
        const QStringList ids = {"gain","bass","middle","treble","presence","level","bright","gainSwitch","solo","soloLevel","speakerType","micType","micDistance","micPosition","micLevel","directLevel","customType","customBottom","customEdge","customBassFrequency","customTrebleFrequency","customPreampLow","customPreampHigh","customSpeakerSize","customSpeakerColorLow","customSpeakerColorHigh","customSpeakerNumber","customCabinetBack"};
        for (int i = 0; i < ids.size(); ++i) {
            const QString offset = QString::number(channel.second + i + 1, 16).rightJustified(2, '0').toUpper();
            ParameterDescriptor d = parameter(ids.at(i).toLatin1().constData(), "0100", offset.toLatin1().constData());
            if (ids.at(i) == "bright") {
                d.rule = AvailabilityRule::PreampBright;
                d.controller = preamp.type;
                d.unavailableReason = "bright_not_supported_by_preamp_type";
            }
            if (i >= 16)
                d = conditional(d, preamp.type, {0x27}, "custom_preamp_only");
            if (ids.at(i).startsWith("customSpeaker"))
                d = conditional(d, address("0100", QString::number(channel.second + 0x0B, 16).rightJustified(2, '0').toUpper().toLatin1().constData()), {0x09}, "custom_speaker_only");
            preamp.parameters.append(d);
        }
        result.append(preamp);
    }

    EffectDescriptor ns2 = simpleEffect("ns2", "0A00", "75");
    ns2.parameters = {parameter("threshold", "0A00", "76"), parameter("release", "0A00", "77"), parameter("detect", "0A00", "78")};
    result.append(ns2);

    EffectDescriptor eq = simpleEffect("eq", "0100", "70");
    const QStringList eqIds = {"lowCut","lowGain","lowMiddleFrequency","lowMiddleQ","lowMiddleGain","highMiddleFrequency","highMiddleQ","highMiddleGain","highGain","highCut","outputLevel"};
    for (int i = 0; i < eqIds.size(); ++i)
        eq.parameters.append(parameter(eqIds.at(i).toLatin1().constData(), "0100", QString::number(0x71 + i, 16).toUpper().toLatin1().constData()));
    result.append(eq);

    {
        const QPair<QString, QString> slot("fx2", "0600");
        EffectDescriptor fx = simpleEffect(slot.first.toLatin1().constData(), slot.second.toLatin1().constData(), "00", "01");
        bool ok = false;
        const int type = byteAt(blocks, fx.type, &ok);
        if (ok)
            fx.parameters = fxParameters(slot.second, type);
        result.append(fx);
    }

    EffectDescriptor sendReturn = simpleEffect("sendReturn", "0A00", "79", "7A");
    sendReturn.parameters = {parameter("sendLevel", "0A00", "7B"), parameter("returnLevel", "0A00", "7C")};
    result.append(sendReturn);

    EffectDescriptor pedalFx = simpleEffect("pedalFxFv", "0A00", "40", "45");
    const QStringList wahIds = {
        "wahType", "wahPosition", "wahMinimum", "wahMaximum",
        "wahEffectLevel", "wahDirectLevel"};
    for (int index = 0; index < wahIds.size(); ++index)
        pedalFx.parameters.append(conditional(parameter(wahIds.at(index).toLatin1().constData(), "0A00", QString::number(0x49 + index, 16).toUpper().toLatin1().constData()), address("0A00", "45"), {3,5}, "wah_mode_only"));
    const QStringList pedalBendIds = {
        "pedalBendPitchMinimum", "pedalBendPitchMaximum",
        "pedalBendPosition", "pedalBendEffectLevel",
        "pedalBendDirectLevel"};
    for (int index = 0; index < pedalBendIds.size(); ++index)
        pedalFx.parameters.append(conditional(parameter(pedalBendIds.at(index).toLatin1().constData(), "0A00", QString::number(0x54 + index, 16).toUpper().toLatin1().constData()), address("0A00", "45"), {2,4}, "pedal_bend_mode_only"));
    const QStringList footVolumeIds = {
        "footVolumeLevel", "footVolumeMinimum",
        "footVolumeMaximum", "footVolumeCurve"};
    for (int index = 0; index < footVolumeIds.size(); ++index)
        pedalFx.parameters.append(conditional(parameter(footVolumeIds.at(index).toLatin1().constData(), "0A00", QString::number(0x5A + index, 16).toUpper().toLatin1().constData()), address("0A00", "45"), {1,4,5}, "foot_volume_mode_only"));
    for (int offset = 0x4F; offset <= 0x53; ++offset) {
        ParameterDescriptor custom = parameter(
            QString("customWah%1").arg(offset - 0x4E).toLatin1().constData(),
            "0A00", QString::number(offset, 16).toUpper().toLatin1().constData());
        custom.rule = AvailabilityRule::CustomWah;
        custom.controller = address("0A00", "49");
        custom.unavailableReason = "custom_wah_only";
        pedalFx.parameters.append(custom);
    }
    result.append(pedalFx);

    EffectDescriptor chorus = simpleEffect("chorus", "0A00", "20", "21");
    chorus.parameters = {parameter("rate", "0A00", "22"),parameter("depth", "0A00", "23"),parameter("preDelay", "0A00", "24"),parameter("lowCut", "0A00", "25"),parameter("highCut", "0A00", "26"),parameter("effectLevel", "0A00", "27")};
    result.append(chorus);

    EffectDescriptor delay = simpleEffect("delay", "0A00", "00", "01");
    delay.parameters = {
        conditional(parameter("delayTime", "0A00", "02", true), delay.type, {3,4,5}, "single_delay_only"),
        conditional(parameter("tapTime", "0A00", "04"), delay.type, {1}, "pan_delay_only"),
        conditional(parameter("feedback", "0A00", "05"), delay.type, {3,4,5}, "single_delay_only"),
        conditional(parameter("highCut", "0A00", "06"), delay.type, {3,4,5}, "single_delay_only"),
        conditional(parameter("effectLevel", "0A00", "17"), delay.type, {3,4,5}, "single_delay_only"),
        parameter("directLevel", "0A00", "18")};
    for (int i = 0; i < 5; ++i) {
        delay.parameters[i].rule = AvailabilityRule::ControllerNotEquals;
        if (i == 1) {
            delay.parameters[i].rule = AvailabilityRule::ControllerIn;
            delay.parameters[i].controllerValues = {1};
        }
    }
    const QList<QPair<QString, QString>> dualDelayParameters = {
        {"dualDelay1Time","07"},{"dualDelay1Feedback","09"},
        {"dualDelay1HighCut","0A"},{"dualDelay1Level","0B"},
        {"dualDelay2Time","0C"},{"dualDelay2Feedback","0E"},
        {"dualDelay2HighCut","0F"},{"dualDelay2Level","10"}};
    for (const auto &entry : dualDelayParameters)
        delay.parameters.append(conditional(parameter(entry.first.toLatin1().constData(), "0A00", entry.second.toLatin1().constData(), entry.second=="07" || entry.second=="0C"), delay.type, {3,4,5}, "dual_delay_only"));
    const QList<QPair<QString, QString>> warpParameters = {
        {"warpState","11"},{"warpRise","12"},
        {"warpDepth","13"},{"warpLevel","14"}};
    for (const auto &entry : warpParameters)
        delay.parameters.append(conditional(parameter(entry.first.toLatin1().constData(), "0A00", entry.second.toLatin1().constData()), delay.type, {9}, "warp_delay_only"));
    const QList<QPair<QString, QString>> modulateParameters = {
        {"modulateRate","15"},{"modulateDepth","16"}};
    for (const auto &entry : modulateParameters)
        delay.parameters.append(conditional(parameter(entry.first.toLatin1().constData(), "0A00", entry.second.toLatin1().constData()), delay.type, {10}, "modulate_delay_only"));
    result.append(delay);

    EffectDescriptor reverb = simpleEffect("reverb", "0A00", "30", "31");
    reverb.parameters = {parameter("reverbTime", "0A00", "32", false, "sec"),parameter("preDelay", "0A00", "3A", true, "ms"),parameter("lowCut", "0A00", "34"),parameter("highCut", "0A00", "35"),parameter("density", "0A00", "36"),parameter("effectLevel", "0A00", "37"),parameter("directLevel", "0A00", "38"),conditional(parameter("springSensitivity", "0A00", "39"), reverb.type, {5}, "spring_reverb_only")};
    result.append(reverb);

    return result;
}

} // namespace Ai
