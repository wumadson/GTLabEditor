#include "quickSettingCodec.h"

#include <QCoreApplication>
#include <QDebug>

namespace {
bool expect(bool condition, const char *message)
{
    if (!condition)
        qCritical() << "FAILED:" << message;
    return condition;
}

QByteArray payload(int seed,
                   int size = QuickSettingCodec::PreampPayloadSize)
{
    QByteArray bytes;
    for (int index = 0; index < size; ++index)
        bytes.append(static_cast<char>((seed + index) & 0x7F));
    return bytes;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;
    QString error;

    ok &= expect(QuickSettingCodec::effectAddress(
        1, QuickSettingEffect::PreampA).toHex().toUpper() == "30000110",
        "U01 PREAMP A address");
    ok &= expect(QuickSettingCodec::effectAddress(
        10, QuickSettingEffect::PreampB).toHex().toUpper() == "30090130",
        "U10 PREAMP B address");
    ok &= expect(QuickSettingCodec::temporaryEffectAddress(
        QuickSettingEffect::PreampA).toHex().toUpper() == "60000110",
        "LOAD A targets only Temporary PREAMP A");
    ok &= expect(QuickSettingCodec::temporaryEffectAddress(
        QuickSettingEffect::PreampB).toHex().toUpper() == "60000130",
        "LOAD B targets only Temporary PREAMP B");
    ok &= expect(QuickSettingCodec::effectAddress(
        1, QuickSettingEffect::OverdriveDistortion).toHex().toUpper()
                     == "30000070",
                 "U01 OD/DS starts at 30:00:00:70");
    ok &= expect(QuickSettingCodec::effectAddress(
        10, QuickSettingEffect::OverdriveDistortion).toHex().toUpper()
                     == "30090070",
                 "U10 OD/DS starts at 30:09:00:70");
    ok &= expect(QuickSettingCodec::temporaryEffectAddress(
        QuickSettingEffect::OverdriveDistortion).toHex().toUpper()
                     == "60000070",
                 "LOAD OD/DS targets only Temporary 00:70");
    ok &= expect(QuickSettingCodec::effectPayloadSize(
        QuickSettingEffect::OverdriveDistortion) == 14,
        "OD/DS payload is exactly 00:70-00:7D");
    ok &= expect(QuickSettingCodec::nameAddress(10).toHex().toUpper()
                     == "30094024",
                 "U10 shared name address");
    ok &= expect(QuickSettingCodec::effectNameAddress(
        1, QuickSettingEffect::OverdriveDistortion).toHex().toUpper()
                     == "30000000",
                 "U01 OD/DS name starts at the User Quick Setting base");
    ok &= expect(QuickSettingCodec::effectNameAddress(
        10, QuickSettingEffect::OverdriveDistortion).toHex().toUpper()
                     == "30090000",
                 "U10 OD/DS name starts at the User Quick Setting base");
    ok &= expect(QuickSettingCodec::effectAddress(
        1, QuickSettingEffect::Delay).toHex().toUpper() == "30000A00",
        "U01 DELAY starts at 30:00:0A:00");
    ok &= expect(QuickSettingCodec::effectAddress(
        10, QuickSettingEffect::Delay).toHex().toUpper() == "30090A00",
        "U10 DELAY starts at 30:09:0A:00");
    ok &= expect(QuickSettingCodec::temporaryEffectAddress(
        QuickSettingEffect::Delay).toHex().toUpper() == "60000A00",
        "LOAD DELAY targets only Temporary 0A:00");
    ok &= expect(QuickSettingCodec::effectPayloadSize(
        QuickSettingEffect::Delay) == 32,
        "DELAY payload is exactly 0A:00-0A:1F");
    ok &= expect(QuickSettingCodec::effectNameAddress(
        1, QuickSettingEffect::Delay).toHex().toUpper() == "30004404",
        "U01 DELAY name starts at 44:04");
    ok &= expect(QuickSettingCodec::effectNameAddress(
        10, QuickSettingEffect::Delay).toHex().toUpper() == "30094404",
        "U10 DELAY name starts at 44:04");
    ok &= expect(QuickSettingCodec::effectAddress(
        1, QuickSettingEffect::Chorus).toHex().toUpper() == "30000A20",
        "U01 CHORUS starts at 30:00:0A:20");
    ok &= expect(QuickSettingCodec::effectAddress(
        10, QuickSettingEffect::Chorus).toHex().toUpper() == "30090A20",
        "U10 CHORUS starts at 30:09:0A:20");
    ok &= expect(QuickSettingCodec::temporaryEffectAddress(
        QuickSettingEffect::Chorus).toHex().toUpper() == "60000A20",
        "LOAD CHORUS targets only Temporary 0A:20");
    ok &= expect(QuickSettingCodec::effectPayloadSize(
        QuickSettingEffect::Chorus) == 16,
        "CHORUS payload is exactly 0A:20-0A:2F");
    ok &= expect(QuickSettingCodec::effectNameAddress(
        1, QuickSettingEffect::Chorus).toHex().toUpper() == "30004410",
        "U01 CHORUS name starts at 44:10");
    ok &= expect(QuickSettingCodec::effectNameAddress(
        10, QuickSettingEffect::Chorus).toHex().toUpper() == "30094410",
        "U10 CHORUS name starts at 44:10");
    ok &= expect(QuickSettingCodec::effectAddress(
        1, QuickSettingEffect::Reverb).toHex().toUpper() == "30000A30",
        "U01 REVERB starts at 30:00:0A:30");
    ok &= expect(QuickSettingCodec::effectAddress(
        10, QuickSettingEffect::Reverb).toHex().toUpper() == "30090A30",
        "U10 REVERB starts at 30:09:0A:30");
    ok &= expect(QuickSettingCodec::temporaryEffectAddress(
        QuickSettingEffect::Reverb).toHex().toUpper() == "60000A30",
        "LOAD REVERB targets only Temporary 0A:30");
    ok &= expect(QuickSettingCodec::effectPayloadSize(
        QuickSettingEffect::Reverb) == 16,
        "REVERB payload is exactly 0A:30-0A:3F");
    ok &= expect(QuickSettingCodec::effectNameAddress(
        1, QuickSettingEffect::Reverb).toHex().toUpper() == "3000441C",
        "U01 REVERB name starts at 44:1C");
    ok &= expect(QuickSettingCodec::effectNameAddress(
        10, QuickSettingEffect::Reverb).toHex().toUpper() == "3009441C",
        "U10 REVERB name starts at 44:1C");
    ok &= expect(QuickSettingCodec::effectAddress(
        1, QuickSettingEffect::Compressor).toHex().toUpper() == "30000040",
        "U01 COMP starts at 30:00:00:40");
    ok &= expect(QuickSettingCodec::effectAddress(
        10, QuickSettingEffect::Compressor).toHex().toUpper() == "30090040",
        "U10 COMP starts at 30:09:00:40");
    ok &= expect(QuickSettingCodec::temporaryEffectAddress(
        QuickSettingEffect::Compressor).toHex().toUpper() == "60000040",
        "LOAD COMP targets only Temporary 00:40");
    ok &= expect(QuickSettingCodec::effectPayloadSize(
        QuickSettingEffect::Compressor) == 16,
        "COMP payload is exactly 00:40-00:4F");
    ok &= expect(QuickSettingCodec::effectNameAddress(
        1, QuickSettingEffect::Compressor).toHex().toUpper() == "30004000",
        "U01 COMP name starts at 40:00");
    ok &= expect(QuickSettingCodec::effectNameAddress(
        10, QuickSettingEffect::Compressor).toHex().toUpper() == "30094000",
        "U10 COMP name starts at 40:00");
    ok &= expect(QuickSettingCodec::effectAddress(
        1, QuickSettingEffect::Equalizer).toHex().toUpper() == "30000170",
        "U01 EQ starts at 30:00:01:70");
    ok &= expect(QuickSettingCodec::effectAddress(
        10, QuickSettingEffect::Equalizer).toHex().toUpper() == "30090170",
        "U10 EQ starts at 30:09:01:70");
    ok &= expect(QuickSettingCodec::temporaryEffectAddress(
        QuickSettingEffect::Equalizer).toHex().toUpper() == "60000170",
        "LOAD EQ targets only Temporary 01:70");
    ok &= expect(QuickSettingCodec::effectPayloadSize(
        QuickSettingEffect::Equalizer) == 16,
        "EQ payload is exactly 01:70-01:7F");
    ok &= expect(QuickSettingCodec::effectNameAddress(
        1, QuickSettingEffect::Equalizer).toHex().toUpper() == "3000403C",
        "U01 EQ name starts at 40:3C");
    ok &= expect(QuickSettingCodec::effectNameAddress(
        10, QuickSettingEffect::Equalizer).toHex().toUpper() == "3009403C",
        "U10 EQ name starts at 40:3C");
    ok &= expect(QuickSettingCodec::effectAddress(
        1, QuickSettingEffect::SendReturn).toHex().toUpper() == "30000A79",
        "U01 S/R starts at 30:00:0A:79");
    ok &= expect(QuickSettingCodec::effectAddress(
        10, QuickSettingEffect::SendReturn).toHex().toUpper() == "30090A79",
        "U10 S/R starts at 30:09:0A:79");
    ok &= expect(QuickSettingCodec::temporaryEffectAddress(
        QuickSettingEffect::SendReturn).toHex().toUpper() == "60000A79",
        "LOAD S/R targets only Temporary 0A:79");
    ok &= expect(QuickSettingCodec::effectPayloadSize(
        QuickSettingEffect::SendReturn) == 4,
        "S/R payload is exactly ON/OFF, MODE, SEND and RETURN");
    ok &= expect(QuickSettingCodec::effectNameAddress(
        1, QuickSettingEffect::SendReturn).toHex().toUpper() == "30004440",
        "U01 S/R name starts at the physically confirmed 44:40");
    ok &= expect(QuickSettingCodec::effectNameAddress(
        10, QuickSettingEffect::SendReturn).toHex().toUpper() == "30094440",
        "U10 S/R name preserves the selected slot");
    ok &= expect(QuickSettingCodec::slotBaseAddress(0, &error).isEmpty(),
                 "preset/out-of-range slots rejected");

    const QString rq1 = QuickSettingCodec::buildReadRequest(
        QuickSettingCodec::effectAddress(3, QuickSettingEffect::PreampA),
        QuickSettingCodec::PreampPayloadSize);
    ok &= expect(rq1.startsWith("F0410000002F1130020110"),
                 "RQ1 uses 30:<slot-1>, never 31");
    ok &= expect(rq1.contains("0000001D"), "RQ1 requests exactly 29 bytes");
    const QString oddsRq1 = QuickSettingCodec::buildReadRequest(
        QuickSettingCodec::effectAddress(
            10, QuickSettingEffect::OverdriveDistortion),
        QuickSettingCodec::OddsPayloadSize);
    ok &= expect(oddsRq1.startsWith("F0410000002F1130090070"),
                 "OD/DS RQ1 uses the selected U01-U10 slot");
    ok &= expect(oddsRq1.contains("0000000E"),
                 "OD/DS RQ1 requests exactly 14 bytes");
    const QString delayRq1 = QuickSettingCodec::buildReadRequest(
        QuickSettingCodec::effectAddress(10, QuickSettingEffect::Delay),
        QuickSettingCodec::DelayPayloadSize);
    ok &= expect(delayRq1.startsWith("F0410000002F1130090A00"),
                 "DELAY RQ1 uses the selected U01-U10 slot");
    ok &= expect(delayRq1.contains("00000020"),
                 "DELAY RQ1 requests exactly 32 bytes");
    const QString chorusRq1 = QuickSettingCodec::buildReadRequest(
        QuickSettingCodec::effectAddress(10, QuickSettingEffect::Chorus),
        QuickSettingCodec::ChorusPayloadSize);
    ok &= expect(chorusRq1.startsWith("F0410000002F1130090A20"),
                 "CHORUS RQ1 uses the selected U01-U10 slot");
    ok &= expect(chorusRq1.contains("00000010"),
                 "CHORUS RQ1 requests exactly 16 bytes");
    const QString reverbRq1 = QuickSettingCodec::buildReadRequest(
        QuickSettingCodec::effectAddress(10, QuickSettingEffect::Reverb),
        QuickSettingCodec::ReverbPayloadSize);
    ok &= expect(reverbRq1.startsWith("F0410000002F1130090A30"),
                 "REVERB RQ1 uses the selected U01-U10 slot");
    ok &= expect(reverbRq1.contains("00000010"),
                 "REVERB RQ1 requests exactly 16 bytes");
    const QString compRq1 = QuickSettingCodec::buildReadRequest(
        QuickSettingCodec::effectAddress(10, QuickSettingEffect::Compressor),
        QuickSettingCodec::CompressorPayloadSize);
    ok &= expect(compRq1.startsWith("F0410000002F1130090040"),
                 "COMP RQ1 uses the selected U01-U10 slot");
    ok &= expect(compRq1.contains("00000010"),
                 "COMP RQ1 requests exactly 16 bytes");
    const QString eqRq1 = QuickSettingCodec::buildReadRequest(
        QuickSettingCodec::effectAddress(10, QuickSettingEffect::Equalizer),
        QuickSettingCodec::EqualizerPayloadSize);
    ok &= expect(eqRq1.startsWith("F0410000002F1130090170"),
                 "EQ RQ1 uses the selected U01-U10 slot");
    ok &= expect(eqRq1.contains("00000010"),
                 "EQ RQ1 requests exactly 16 bytes");
    const QString sendReturnRq1 = QuickSettingCodec::buildReadRequest(
        QuickSettingCodec::effectAddress(10, QuickSettingEffect::SendReturn),
        QuickSettingCodec::SendReturnPayloadSize);
    ok &= expect(sendReturnRq1.startsWith("F0410000002F1130090A79"),
                 "S/R RQ1 uses the selected U01-U10 slot");
    ok &= expect(sendReturnRq1.contains("00000004"),
                 "S/R RQ1 requests exactly 4 bytes");

    const QByteArray effectPayload = payload(7);
    ok &= expect(QuickSettingCodec::preampTypeRaw(effectPayload) == 7,
                 "PREAMP TYPE is extracted from payload byte zero");
    ok &= expect(QuickSettingCodec::preampTypeRaw(payload(0x0E)) == 0x0E,
                 "PREAMP A/B TYPE extraction preserves the real raw");
    ok &= expect(QuickSettingCodec::preampTypeRaw(QByteArray(28, 0), &error) < 0,
                 "TYPE extraction rejects incomplete payloads");
    QByteArray oddsPayload = payload(0, QuickSettingCodec::OddsPayloadSize);
    oddsPayload[0] = static_cast<char>(1);
    oddsPayload[1] = static_cast<char>(0x12);
    ok &= expect(QuickSettingCodec::effectTypeRaw(
        QuickSettingEffect::OverdriveDistortion, oddsPayload) == 0x12,
        "OD/DS TYPE is payload byte one after ON/OFF");
    ok &= expect(QuickSettingCodec::effectTypeRaw(
        QuickSettingEffect::OverdriveDistortion,
        QByteArray(QuickSettingCodec::OddsPayloadSize - 1, 0), &error) < 0,
        "OD/DS TYPE extraction rejects incomplete payloads");
    QByteArray delayPayload = payload(0, QuickSettingCodec::DelayPayloadSize);
    delayPayload[0] = static_cast<char>(1);
    delayPayload[1] = static_cast<char>(0x06);
    ok &= expect(QuickSettingCodec::effectTypeRaw(
        QuickSettingEffect::Delay, delayPayload) == 0x06,
        "DELAY TYPE is payload byte one after ON/OFF");
    ok &= expect(QuickSettingCodec::effectTypeRaw(
        QuickSettingEffect::Delay,
        QByteArray(QuickSettingCodec::DelayPayloadSize - 1, 0), &error) < 0,
        "DELAY TYPE extraction rejects incomplete payloads");
    const QString delayDt1 = QuickSettingCodec::buildWriteMessage(
        QuickSettingCodec::effectAddress(10, QuickSettingEffect::Delay),
        delayPayload);
    const QuickSettingReply delayDecoded = QuickSettingCodec::decodeReply(
        delayDt1,
        QuickSettingCodec::effectAddress(10, QuickSettingEffect::Delay),
        QuickSettingCodec::DelayPayloadSize);
    ok &= expect(delayDecoded.valid && delayDecoded.payload == delayPayload,
                 "DELAY SAVE contains only the 32-byte effect block");
    QByteArray chorusPayload = payload(0, QuickSettingCodec::ChorusPayloadSize);
    chorusPayload[0] = static_cast<char>(1);
    chorusPayload[1] = static_cast<char>(0x02);
    ok &= expect(QuickSettingCodec::effectTypeRaw(
        QuickSettingEffect::Chorus, chorusPayload) == 0x02,
        "CHORUS TYPE is payload byte one after ON/OFF");
    ok &= expect(QuickSettingCodec::effectTypeRaw(
        QuickSettingEffect::Chorus,
        QByteArray(QuickSettingCodec::ChorusPayloadSize - 1, 0), &error) < 0,
        "CHORUS TYPE extraction rejects incomplete payloads");
    const QString chorusDt1 = QuickSettingCodec::buildWriteMessage(
        QuickSettingCodec::effectAddress(10, QuickSettingEffect::Chorus),
        chorusPayload);
    const QuickSettingReply chorusDecoded = QuickSettingCodec::decodeReply(
        chorusDt1,
        QuickSettingCodec::effectAddress(10, QuickSettingEffect::Chorus),
        QuickSettingCodec::ChorusPayloadSize);
    ok &= expect(chorusDecoded.valid && chorusDecoded.payload == chorusPayload,
                 "CHORUS SAVE contains only the 16-byte effect block");
    QByteArray reverbPayload = payload(0, QuickSettingCodec::ReverbPayloadSize);
    reverbPayload[0] = static_cast<char>(1);
    reverbPayload[1] = static_cast<char>(0x05);
    ok &= expect(QuickSettingCodec::effectTypeRaw(
        QuickSettingEffect::Reverb, reverbPayload) == 0x05,
        "REVERB TYPE is payload byte one after ON/OFF");
    ok &= expect(QuickSettingCodec::effectTypeRaw(
        QuickSettingEffect::Reverb,
        QByteArray(QuickSettingCodec::ReverbPayloadSize - 1, 0), &error) < 0,
        "REVERB TYPE extraction rejects incomplete payloads");
    const QString reverbDt1 = QuickSettingCodec::buildWriteMessage(
        QuickSettingCodec::effectAddress(10, QuickSettingEffect::Reverb),
        reverbPayload);
    const QuickSettingReply reverbDecoded = QuickSettingCodec::decodeReply(
        reverbDt1,
        QuickSettingCodec::effectAddress(10, QuickSettingEffect::Reverb),
        QuickSettingCodec::ReverbPayloadSize);
    ok &= expect(reverbDecoded.valid && reverbDecoded.payload == reverbPayload,
                 "REVERB SAVE contains only the 16-byte effect block");
    QByteArray compPayload = payload(0, QuickSettingCodec::CompressorPayloadSize);
    compPayload[0] = static_cast<char>(1);
    compPayload[1] = static_cast<char>(1);
    ok &= expect(QuickSettingCodec::effectTypeRaw(
        QuickSettingEffect::Compressor, compPayload) == 1,
        "COMP TYPE is payload byte one after ON/OFF");
    ok &= expect(QuickSettingCodec::effectTypeRaw(
        QuickSettingEffect::Compressor,
        QByteArray(QuickSettingCodec::CompressorPayloadSize - 1, 0), &error) < 0,
        "COMP TYPE extraction rejects incomplete payloads");
    const QString compDt1 = QuickSettingCodec::buildWriteMessage(
        QuickSettingCodec::effectAddress(10, QuickSettingEffect::Compressor),
        compPayload);
    const QuickSettingReply compDecoded = QuickSettingCodec::decodeReply(
        compDt1,
        QuickSettingCodec::effectAddress(10, QuickSettingEffect::Compressor),
        QuickSettingCodec::CompressorPayloadSize);
    ok &= expect(compDecoded.valid && compDecoded.payload == compPayload,
                 "COMP SAVE contains only the 16-byte effect block");
    QByteArray eqPayload = payload(0, QuickSettingCodec::EqualizerPayloadSize);
    eqPayload[0] = static_cast<char>(1);
    ok &= expect(QuickSettingCodec::effectTypeRaw(
        QuickSettingEffect::Equalizer, eqPayload, &error) < 0,
        "EQ has no TYPE field");
    ok &= expect(QuickSettingCodec::effectTypeRaw(
        QuickSettingEffect::Equalizer,
        QByteArray(QuickSettingCodec::EqualizerPayloadSize - 1, 0), &error) < 0,
        "EQ incomplete payload cannot expose a TYPE");
    const QString eqDt1 = QuickSettingCodec::buildWriteMessage(
        QuickSettingCodec::effectAddress(10, QuickSettingEffect::Equalizer),
        eqPayload);
    const QuickSettingReply eqDecoded = QuickSettingCodec::decodeReply(
        eqDt1, QuickSettingCodec::effectAddress(
            10, QuickSettingEffect::Equalizer),
        QuickSettingCodec::EqualizerPayloadSize);
    ok &= expect(eqDecoded.valid && eqDecoded.payload == eqPayload,
                 "EQ SAVE round-trips only its 16-byte native block");
    QByteArray sendReturnPayload = payload(
        0, QuickSettingCodec::SendReturnPayloadSize);
    sendReturnPayload[0] = static_cast<char>(1);
    sendReturnPayload[1] = static_cast<char>(2);
    sendReturnPayload[2] = static_cast<char>(100);
    sendReturnPayload[3] = static_cast<char>(90);
    ok &= expect(QuickSettingCodec::effectTypeRaw(
        QuickSettingEffect::SendReturn, sendReturnPayload) == 2,
        "S/R LOOP MODE is payload byte one after ON/OFF");
    ok &= expect(QuickSettingCodec::effectTypeRaw(
        QuickSettingEffect::SendReturn,
        QByteArray(QuickSettingCodec::SendReturnPayloadSize - 1, 0),
        &error) < 0,
        "S/R rejects an incomplete payload");
    const QString sendReturnDt1 = QuickSettingCodec::buildWriteMessage(
        QuickSettingCodec::effectAddress(10, QuickSettingEffect::SendReturn),
        sendReturnPayload);
    const QuickSettingReply sendReturnDecoded = QuickSettingCodec::decodeReply(
        sendReturnDt1,
        QuickSettingCodec::effectAddress(10, QuickSettingEffect::SendReturn),
        QuickSettingCodec::SendReturnPayloadSize);
    ok &= expect(sendReturnDecoded.valid
                     && sendReturnDecoded.payload == sendReturnPayload,
                 "S/R SAVE round-trips only its 4-byte native block");
    const QString oddsDt1 = QuickSettingCodec::buildWriteMessage(
        QuickSettingCodec::effectAddress(
            10, QuickSettingEffect::OverdriveDistortion), oddsPayload);
    const QuickSettingReply oddsDecoded = QuickSettingCodec::decodeReply(
        oddsDt1, QuickSettingCodec::effectAddress(
            10, QuickSettingEffect::OverdriveDistortion),
        QuickSettingCodec::OddsPayloadSize);
    ok &= expect(oddsDecoded.valid && oddsDecoded.payload == oddsPayload,
                 "OD/DS SAVE contains only the 14-byte effect block");
    const QString dt1 = QuickSettingCodec::buildWriteMessage(
        QuickSettingCodec::effectAddress(4, QuickSettingEffect::PreampB),
        effectPayload);
    const QuickSettingReply decoded = QuickSettingCodec::decodeReply(
        dt1, QuickSettingCodec::effectAddress(4, QuickSettingEffect::PreampB),
        QuickSettingCodec::PreampPayloadSize);
    ok &= expect(decoded.valid && decoded.payload == effectPayload,
                 "strict DT1 decode and checksum");

    QByteArray damaged = QByteArray::fromHex(dt1.toLatin1());
    damaged[12] = static_cast<char>(damaged.at(12) ^ 0x01);
    ok &= expect(!QuickSettingCodec::decodeReply(
        QString::fromLatin1(damaged.toHex()),
        QuickSettingCodec::effectAddress(4, QuickSettingEffect::PreampB),
        QuickSettingCodec::PreampPayloadSize).valid,
        "checksum mismatch rejected");

    const QByteArray name = QuickSettingCodec::encodeName("T-AMP LEAD", &error);
    ok &= expect(name.size() == QuickSettingCodec::NameSize
                     && name.endsWith("  "),
                 "name is padded to 12 bytes");
    ok &= expect(QuickSettingCodec::decodeName(name) == "T-AMP LEAD",
                 "name round-trip");
    const QString oddsNameDt1 = QuickSettingCodec::buildWriteMessage(
        QuickSettingCodec::effectNameAddress(
            10, QuickSettingEffect::OverdriveDistortion),
        QuickSettingCodec::encodeName("MORNING GLRY"));
    const QuickSettingReply oddsNameDecoded = QuickSettingCodec::decodeReply(
        oddsNameDt1, QuickSettingCodec::effectNameAddress(
            10, QuickSettingEffect::OverdriveDistortion),
        QuickSettingCodec::NameSize);
    ok &= expect(oddsNameDecoded.valid
                     && QuickSettingCodec::decodeName(oddsNameDecoded.payload)
                            == "MORNING GLRY",
                 "OD/DS name DT1 preserves its independent 12-byte field");
    const QString delayNameDt1 = QuickSettingCodec::buildWriteMessage(
        QuickSettingCodec::effectNameAddress(10, QuickSettingEffect::Delay),
        QuickSettingCodec::encodeName("WORSHIP 8D"));
    const QuickSettingReply delayNameDecoded = QuickSettingCodec::decodeReply(
        delayNameDt1,
        QuickSettingCodec::effectNameAddress(10, QuickSettingEffect::Delay),
        QuickSettingCodec::NameSize);
    ok &= expect(delayNameDecoded.valid
                     && QuickSettingCodec::decodeName(delayNameDecoded.payload)
                            == "WORSHIP 8D",
                 "DELAY name DT1 preserves its independent 12-byte field");
    const QString reverbNameDt1 = QuickSettingCodec::buildWriteMessage(
        QuickSettingCodec::effectNameAddress(10, QuickSettingEffect::Reverb),
        QuickSettingCodec::encodeName("BIG WORSHIP"));
    const QuickSettingReply reverbNameDecoded = QuickSettingCodec::decodeReply(
        reverbNameDt1,
        QuickSettingCodec::effectNameAddress(10, QuickSettingEffect::Reverb),
        QuickSettingCodec::NameSize);
    ok &= expect(reverbNameDecoded.valid
                     && QuickSettingCodec::decodeName(reverbNameDecoded.payload)
                            == "BIG WORSHIP",
                 "REVERB name DT1 preserves its independent 12-byte field");
    const QString chorusNameDt1 = QuickSettingCodec::buildWriteMessage(
        QuickSettingCodec::effectNameAddress(10, QuickSettingEffect::Chorus),
        QuickSettingCodec::encodeName("WORSHIP CH"));
    const QuickSettingReply chorusNameDecoded = QuickSettingCodec::decodeReply(
        chorusNameDt1,
        QuickSettingCodec::effectNameAddress(10, QuickSettingEffect::Chorus),
        QuickSettingCodec::NameSize);
    ok &= expect(chorusNameDecoded.valid
                     && QuickSettingCodec::decodeName(chorusNameDecoded.payload)
                            == "WORSHIP CH",
                 "CHORUS name DT1 preserves its independent 12-byte field");
    const QString compNameDt1 = QuickSettingCodec::buildWriteMessage(
        QuickSettingCodec::effectNameAddress(10, QuickSettingEffect::Compressor),
        QuickSettingCodec::encodeName("CLEAN COMP"));
    const QuickSettingReply compNameDecoded = QuickSettingCodec::decodeReply(
        compNameDt1,
        QuickSettingCodec::effectNameAddress(10, QuickSettingEffect::Compressor),
        QuickSettingCodec::NameSize);
    ok &= expect(compNameDecoded.valid
                     && QuickSettingCodec::decodeName(compNameDecoded.payload)
                            == "CLEAN COMP",
                 "COMP name DT1 preserves its independent 12-byte field");
    const QString eqNameDt1 = QuickSettingCodec::buildWriteMessage(
        QuickSettingCodec::effectNameAddress(10, QuickSettingEffect::Equalizer),
        QuickSettingCodec::encodeName("WORSHIP EQ"));
    const QuickSettingReply eqNameDecoded = QuickSettingCodec::decodeReply(
        eqNameDt1,
        QuickSettingCodec::effectNameAddress(10, QuickSettingEffect::Equalizer),
        QuickSettingCodec::NameSize);
    ok &= expect(eqNameDecoded.valid
                     && QuickSettingCodec::decodeName(eqNameDecoded.payload)
                            == "WORSHIP EQ",
                 "EQ name DT1 preserves its independent padded field");
    const QString sendReturnNameDt1 = QuickSettingCodec::buildWriteMessage(
        QuickSettingCodec::effectNameAddress(
            10, QuickSettingEffect::SendReturn),
        QuickSettingCodec::encodeName("PARALLEL LP"));
    const QuickSettingReply sendReturnNameDecoded =
        QuickSettingCodec::decodeReply(
            sendReturnNameDt1,
            QuickSettingCodec::effectNameAddress(
                10, QuickSettingEffect::SendReturn),
            QuickSettingCodec::NameSize);
    ok &= expect(sendReturnNameDecoded.valid
                     && QuickSettingCodec::decodeName(
                            sendReturnNameDecoded.payload) == "PARALLEL LP",
                 "S/R name DT1 preserves its independent padded field");
    ok &= expect(QuickSettingCodec::encodeName("0123456789ABC", &error).isEmpty(),
                 "names longer than 12 rejected");
    ok &= expect(QuickSettingCodec::encodeName(QString::fromUtf8("INVÁLIDO"),
                                                &error).isEmpty(),
                 "non GT-10 name characters rejected");

    QByteArray temporary(0x60, static_cast<char>(0x55));
    const QByteArray before = temporary;
    temporary.replace(0x10, QuickSettingCodec::PreampPayloadSize,
                      effectPayload);
    ok &= expect(temporary.left(0x10) == before.left(0x10),
                 "common PREAMP bytes stay untouched");
    ok &= expect(temporary.mid(0x10, QuickSettingCodec::PreampPayloadSize)
                     == effectPayload,
                 "only selected PREAMP A slice replaced");
    ok &= expect(temporary.mid(0x30) == before.mid(0x30),
                 "PREAMP B and following data stay untouched");
    temporary = before;
    temporary.replace(0x30, QuickSettingCodec::PreampPayloadSize,
                      effectPayload);
    ok &= expect(temporary.mid(0x10, QuickSettingCodec::PreampPayloadSize)
                     == before.mid(0x10, QuickSettingCodec::PreampPayloadSize),
                 "PREAMP B save/load leaves PREAMP A untouched");
    ok &= expect(temporary.mid(0x30, QuickSettingCodec::PreampPayloadSize)
                     == effectPayload,
                 "only selected PREAMP B slice replaced");
    QByteArray oddsTemporary(0x100, static_cast<char>(0x55));
    const QByteArray oddsBefore = oddsTemporary;
    oddsTemporary.replace(0x70, QuickSettingCodec::OddsPayloadSize,
                          oddsPayload);
    ok &= expect(oddsTemporary.left(0x70) == oddsBefore.left(0x70),
                 "OD/DS leaves every byte before 00:70 untouched");
    ok &= expect(oddsTemporary.mid(0x70, QuickSettingCodec::OddsPayloadSize)
                     == oddsPayload,
                 "OD/DS replaces ON/OFF through Custom High");
    ok &= expect(oddsTemporary.mid(0x7E) == oddsBefore.mid(0x7E),
                 "OD/DS leaves every byte after 00:7D untouched");
    QByteArray delayTemporary(0x40, static_cast<char>(0x55));
    const QByteArray delayBefore = delayTemporary;
    delayTemporary.replace(0, QuickSettingCodec::DelayPayloadSize,
                           delayPayload);
    ok &= expect(delayTemporary.left(QuickSettingCodec::DelayPayloadSize)
                     == delayPayload,
                 "DELAY replaces exactly 0A:00-0A:1F");
    ok &= expect(delayTemporary.mid(QuickSettingCodec::DelayPayloadSize)
                     == delayBefore.mid(QuickSettingCodec::DelayPayloadSize),
                 "DELAY leaves every byte after 0A:1F untouched");
    QByteArray chorusTemporary(0x50, static_cast<char>(0x55));
    const QByteArray chorusBefore = chorusTemporary;
    chorusTemporary.replace(0x20, QuickSettingCodec::ChorusPayloadSize,
                            chorusPayload);
    ok &= expect(chorusTemporary.left(0x20) == chorusBefore.left(0x20),
                 "CHORUS leaves DELAY and preceding bytes untouched");
    ok &= expect(chorusTemporary.mid(0x20,
                     QuickSettingCodec::ChorusPayloadSize) == chorusPayload,
                 "CHORUS replaces exactly 0A:20-0A:2F");
    ok &= expect(chorusTemporary.mid(0x30) == chorusBefore.mid(0x30),
                 "CHORUS leaves REVERB and following bytes untouched");
    QByteArray reverbTemporary(0x50, static_cast<char>(0x55));
    const QByteArray reverbBefore = reverbTemporary;
    reverbTemporary.replace(0x30, QuickSettingCodec::ReverbPayloadSize,
                            reverbPayload);
    ok &= expect(reverbTemporary.left(0x30) == reverbBefore.left(0x30),
                 "REVERB leaves DELAY, CHORUS and preceding bytes untouched");
    ok &= expect(reverbTemporary.mid(0x30,
                     QuickSettingCodec::ReverbPayloadSize) == reverbPayload,
                 "REVERB replaces exactly 0A:30-0A:3F");
    ok &= expect(reverbTemporary.mid(0x40) == reverbBefore.mid(0x40),
                 "REVERB leaves every byte after 0A:3F untouched");
    QByteArray compTemporary(0x80, static_cast<char>(0x55));
    const QByteArray compBefore = compTemporary;
    compTemporary.replace(0x40, QuickSettingCodec::CompressorPayloadSize,
                          compPayload);
    ok &= expect(compTemporary.left(0x40) == compBefore.left(0x40),
                 "COMP leaves every byte before 00:40 untouched");
    ok &= expect(compTemporary.mid(0x40,
                     QuickSettingCodec::CompressorPayloadSize) == compPayload,
                 "COMP replaces exactly 00:40-00:4F");
    ok &= expect(compTemporary.mid(0x50) == compBefore.mid(0x50),
                 "COMP leaves every byte after 00:4F untouched");
    QByteArray eqTemporary(0x120, static_cast<char>(0x55));
    const QByteArray eqBefore = eqTemporary;
    const int eqOffset = 0x70;
    eqTemporary.replace(eqOffset, QuickSettingCodec::EqualizerPayloadSize,
                        eqPayload);
    ok &= expect(eqTemporary.left(eqOffset) == eqBefore.left(eqOffset),
                 "EQ leaves every byte before 01:70 untouched");
    ok &= expect(eqTemporary.mid(eqOffset,
                     QuickSettingCodec::EqualizerPayloadSize) == eqPayload,
                 "EQ replaces its exact region through 01:7F");
    ok &= expect(eqTemporary.mid(eqOffset + QuickSettingCodec::EqualizerPayloadSize)
                     == eqBefore.mid(eqOffset + QuickSettingCodec::EqualizerPayloadSize),
                 "EQ leaves every byte after its 24-byte region untouched");
    QByteArray sendReturnTemporary(0x80, static_cast<char>(0x55));
    const QByteArray sendReturnBefore = sendReturnTemporary;
    sendReturnTemporary.replace(0x79,
                                QuickSettingCodec::SendReturnPayloadSize,
                                sendReturnPayload);
    ok &= expect(sendReturnTemporary.left(0x79)
                     == sendReturnBefore.left(0x79),
                 "S/R leaves every byte before 0A:79 untouched");
    ok &= expect(sendReturnTemporary.mid(
                     0x79, QuickSettingCodec::SendReturnPayloadSize)
                     == sendReturnPayload,
                 "S/R replaces exactly 0A:79-0A:7C");
    ok &= expect(sendReturnTemporary.mid(0x7D)
                     == sendReturnBefore.mid(0x7D),
                 "S/R leaves every byte after 0A:7C untouched");
    QByteArray sendReturnMismatch = sendReturnPayload;
    sendReturnMismatch[3] = static_cast<char>(
        static_cast<quint8>(sendReturnMismatch.at(3)) ^ 0x01);
    ok &= expect(!QuickSettingCodec::payloadMatches(
                     sendReturnPayload, sendReturnMismatch),
                 "S/R readback mismatch is detected byte by byte");
    QByteArray eqMismatch = eqPayload;
    eqMismatch[11] = static_cast<char>(
        static_cast<quint8>(eqMismatch.at(11)) ^ 0x01);
    ok &= expect(!QuickSettingCodec::payloadMatches(eqPayload, eqMismatch),
                 "EQ readback mismatch is detected byte by byte");

    const QuickSettingTransferPlan fx1U01 = QuickSettingCodec::transferPlan(
        1, QuickSettingEffect::Fx1, false, &error);
    const QuickSettingTransferPlan fx1U10 = QuickSettingCodec::transferPlan(
        10, QuickSettingEffect::Fx1, false, &error);
    const QuickSettingTransferPlan fx1Temporary =
        QuickSettingCodec::transferPlan(
            1, QuickSettingEffect::Fx1, true, &error);
    ok &= expect(fx1U01.isValid() && fx1U01.totalLogicalSize == 470,
                 "FX-1 U01 segmented plan totals 470 bytes");
    ok &= expect(fx1U01.segments.size() == 4
                     && fx1U01.segments.at(0).address.toHex().toUpper()
                            == "30000200"
                     && fx1U01.segments.at(1).address.toHex().toUpper()
                            == "30000300"
                     && fx1U01.segments.at(2).address.toHex().toUpper()
                            == "30000400"
                     && fx1U01.segments.at(3).address.toHex().toUpper()
                            == "30000500",
                 "FX-1 U01 uses only pages 02-05");
    ok &= expect(fx1U01.segments.at(0).size == 128
                     && fx1U01.segments.at(1).size == 128
                     && fx1U01.segments.at(2).size == 128
                     && fx1U01.segments.at(3).size == 86,
                 "FX-1 segment sizes are 128/128/128/86");
    ok &= expect(fx1U10.segments.at(0).address.toHex().toUpper()
                         == "30090200"
                     && fx1U10.segments.at(3).address.toHex().toUpper()
                         == "30090500",
                 "FX-1 U10 preserves selected slot in every segment");
    ok &= expect(fx1Temporary.segments.at(0).address.toHex().toUpper()
                         == "60000200"
                     && fx1Temporary.segments.at(3).address.toHex().toUpper()
                         == "60000500",
                 "FX-1 LOAD targets only Temporary pages 02-05");
    ok &= expect(QuickSettingCodec::buildReadRequest(
        fx1U01.segments.at(0).address, 128).contains("00000100"),
        "FX-1 128-byte RQ1 uses Roland 7-bit size carry");

    QByteArray fx1Payload = payload(0, QuickSettingCodec::Fx1PayloadSize);
    fx1Payload[0] = static_cast<char>(1);
    fx1Payload[1] = static_cast<char>(0x16);
    ok &= expect(static_cast<quint8>(fx1Payload.at(0)) == 1,
                 "FX-1 STATE remains logical offset zero");
    QVector<QByteArray> fx1Parts = QuickSettingCodec::splitPayload(
        fx1Payload, fx1U01, &error);
    ok &= expect(fx1Parts.size() == 4
                     && fx1Parts.at(0).size() == 128
                     && fx1Parts.at(3).size() == 86,
                 "FX-1 logical payload splits without overlap");
    ok &= expect(QuickSettingCodec::joinSegments(
                     fx1Parts, fx1U01, &error) == fx1Payload,
                 "FX-1 segments concatenate to the original 470 bytes");
    const QString fx1FirstReply = QuickSettingCodec::buildWriteMessage(
        fx1U01.segments.at(0).address, fx1Parts.at(0), &error);
    ok &= expect(QuickSettingCodec::decodeReply(
                     fx1FirstReply, fx1U01.segments.at(1).address, 128)
                     .valid == false,
                 "FX-1 segment reply at the wrong address is rejected");
    QString corruptFx1Reply = fx1FirstReply;
    corruptFx1Reply[corruptFx1Reply.size() - 4] =
        corruptFx1Reply.at(corruptFx1Reply.size() - 4) == QChar('0')
            ? QChar('1') : QChar('0');
    ok &= expect(!QuickSettingCodec::decodeReply(
                      corruptFx1Reply, fx1U01.segments.at(0).address, 128)
                      .valid,
                 "FX-1 segment checksum mismatch is rejected");
    QVector<QByteArray> incompleteFx1 = fx1Parts;
    incompleteFx1.removeLast();
    ok &= expect(QuickSettingCodec::joinSegments(
                     incompleteFx1, fx1U01, &error).isEmpty(),
                 "FX-1 incomplete segmented payload is rejected");
    QVector<QByteArray> wrongSizeFx1 = fx1Parts;
    wrongSizeFx1[2].chop(1);
    ok &= expect(QuickSettingCodec::joinSegments(
                     wrongSizeFx1, fx1U01, &error).isEmpty(),
                 "FX-1 wrong segment size is rejected");
    ok &= expect(QuickSettingCodec::effectTypeRaw(
                     QuickSettingEffect::Fx1, fx1Payload) == 0x16,
                 "FX-1 TYPE is logical offset one");
    ok &= expect(QuickSettingCodec::effectNameAddress(
                     1, QuickSettingEffect::Fx1, 0x16).toHex().toUpper()
                         == "30004250",
                 "FX-1 PHASER NAME is the physically confirmed 42:50");
    ok &= expect(QuickSettingCodec::effectNameAddress(
                     10, QuickSettingEffect::Fx1, 0x21).toHex().toUpper()
                         == "30094354",
                 "FX-1 NAME calculation preserves slot and 7-bit carry");
    ok &= expect(QuickSettingCodec::effectNameAddress(
                     1, QuickSettingEffect::Fx1, 0x22, &error).isEmpty(),
                 "FX-1 rejects TYPE outside the documented 00-21 range");
    QVector<QByteArray> temporaryFx1 = QuickSettingCodec::splitPayload(
        fx1Payload, fx1Temporary, &error);
    bool fx2Untouched = temporaryFx1.size() == 4;
    for (const QuickSettingSegment &segment : fx1Temporary.segments)
        fx2Untouched = fx2Untouched
            && static_cast<quint8>(segment.address.at(2)) < 0x06;
    ok &= expect(fx2Untouched,
                 "FX-1 apply plan never targets FX-2 pages 06-09");
    ok &= expect(!QuickSettingCodec::payloadMatches(effectPayload, payload(8)),
                 "readback mismatch detected");

    ok &= expect(QuickSettingCodec::hasPresentationName(
                     QuickSettingEffect::Chorus),
                 "CHORUS presentation uses NAME-first");
    ok &= expect(QuickSettingCodec::hasPresentationName(
                     QuickSettingEffect::Equalizer),
                 "EQ presentation uses NAME-only");
    ok &= expect(!QuickSettingCodec::hasPresentationName(
                     QuickSettingEffect::PreampA),
                 "PREAMP presentation does not invent a NAME");
    ok &= expect(QuickSettingCodec::presentationFallbackAddress(
                     1, QuickSettingEffect::PreampA).toHex().toUpper()
                         == "30000110",
                 "PREAMP A presentation reads one TYPE byte at 01:10");
    ok &= expect(QuickSettingCodec::presentationFallbackAddress(
                     10, QuickSettingEffect::PreampB).toHex().toUpper()
                         == "30090130",
                 "PREAMP B presentation preserves U10 and TYPE address");
    ok &= expect(QuickSettingCodec::presentationFallbackAddress(
                     1, QuickSettingEffect::Chorus).toHex().toUpper()
                         == "30000A21",
                 "CHORUS empty NAME falls back to TYPE +1");
    ok &= expect(QuickSettingCodec::presentationFallbackAddress(
                     1, QuickSettingEffect::SendReturn).toHex().toUpper()
                         == "30000A7A",
                 "S/R empty NAME falls back to LOOP MODE +1");
    ok &= expect(QuickSettingCodec::presentationFallbackAddress(
                     1, QuickSettingEffect::Equalizer, &error).isEmpty(),
                 "EQ has no unverified presentation fallback");
    ok &= expect(QuickSettingCodec::buildReadRequest(
                     QuickSettingCodec::presentationFallbackAddress(
                         1, QuickSettingEffect::PreampA), 1)
                     .contains("00000001"),
                 "PREAMP presentation RQ1 requests exactly one byte");

    if (ok)
        qInfo() << "Quick Setting codec tests passed";
    return ok ? 0 : 1;
}
