#include "quickSettingCodec.h"

namespace {
QByteArray addRolandAddress(const QByteArray &base, int increment)
{
    if (base.size() != 4 || increment < 0)
        return QByteArray();
    QByteArray result(base);
    for (int index = 3; index >= 0 && increment > 0; --index) {
        const int value = static_cast<quint8>(result.at(index)) + increment;
        result[index] = static_cast<char>(value & 0x7F);
        increment = value >> 7;
    }
    return increment == 0 ? result : QByteArray();
}

QByteArray normalizedBytes(QString value, bool *ok)
{
    value.remove(' ');
    value.remove('\n');
    value.remove('\r');
    if (value.size() % 2 != 0) {
        *ok = false;
        return QByteArray();
    }
    const QByteArray result = QByteArray::fromHex(value.toLatin1());
    *ok = result.size() * 2 == value.size();
    return result;
}

quint8 checksum(const QByteArray &bytes, int first, int lastExclusive)
{
    int sum = 0;
    for (int index = first; index < lastExclusive; ++index)
        sum += static_cast<quint8>(bytes.at(index));
    return static_cast<quint8>((0x80 - (sum % 0x80)) % 0x80);
}

QString encodedMessage(quint8 command, const QByteArray &address,
                       const QByteArray &data, QString *error)
{
    if (address.size() != 4) {
        if (error)
            *error = QStringLiteral("Quick Setting address must have 4 bytes");
        return QString();
    }
    QByteArray message = QByteArray::fromHex("F0410000002F");
    message.append(static_cast<char>(command));
    message.append(address);
    message.append(data);
    message.append(static_cast<char>(checksum(message, 7, message.size())));
    message.append(static_cast<char>(0xF7));
    return QString::fromLatin1(message.toHex()).toUpper();
}
}

bool QuickSettingTransferPlan::isValid() const
{
    if (segments.isEmpty() || totalLogicalSize <= 0)
        return false;
    int total = 0;
    for (const QuickSettingSegment &segment : segments) {
        if (segment.address.size() != 4 || segment.size <= 0)
            return false;
        total += segment.size;
    }
    return total == totalLogicalSize;
}

QByteArray QuickSettingCodec::slotBaseAddress(int slot, QString *error)
{
    if (slot < 1 || slot > 10) {
        if (error)
            *error = QStringLiteral("User Quick Setting slot must be U01-U10");
        return QByteArray();
    }
    QByteArray address(4, 0);
    address[0] = static_cast<char>(0x30);
    address[1] = static_cast<char>(slot - 1);
    return address;
}

QByteArray QuickSettingCodec::effectAddress(int slot,
                                             QuickSettingEffect effect,
                                             QString *error)
{
    QByteArray address = slotBaseAddress(slot, error);
    if (address.isEmpty())
        return address;
    if (effect == QuickSettingEffect::Compressor) {
        address[2] = static_cast<char>(0x00);
        address[3] = static_cast<char>(0x40);
    } else if (effect == QuickSettingEffect::Equalizer) {
        address[2] = static_cast<char>(0x01);
        address[3] = static_cast<char>(0x70);
    } else if (effect == QuickSettingEffect::OverdriveDistortion) {
        address[2] = static_cast<char>(0x00);
        address[3] = static_cast<char>(0x70);
    } else if (effect == QuickSettingEffect::Delay) {
        address[2] = static_cast<char>(0x0A);
        address[3] = static_cast<char>(0x00);
    } else if (effect == QuickSettingEffect::Chorus) {
        address[2] = static_cast<char>(0x0A);
        address[3] = static_cast<char>(0x20);
    } else if (effect == QuickSettingEffect::Reverb) {
        address[2] = static_cast<char>(0x0A);
        address[3] = static_cast<char>(0x30);
    } else if (effect == QuickSettingEffect::Fx1) {
        address[2] = static_cast<char>(0x02);
        address[3] = static_cast<char>(0x00);
    } else if (effect == QuickSettingEffect::SendReturn) {
        address[2] = static_cast<char>(0x0A);
        address[3] = static_cast<char>(0x79);
    } else {
        address[2] = static_cast<char>(0x01);
        address[3] = static_cast<char>(
            effect == QuickSettingEffect::PreampA ? 0x10 : 0x30);
    }
    return address;
}

QByteArray QuickSettingCodec::nameAddress(int slot, QString *error)
{
    return effectNameAddress(slot, QuickSettingEffect::PreampA, error);
}

QByteArray QuickSettingCodec::effectNameAddress(
    int slot, QuickSettingEffect effect, QString *error)
{
    QByteArray address = slotBaseAddress(slot, error);
    if (address.isEmpty())
        return address;
    if (effect == QuickSettingEffect::Compressor) {
        address[2] = static_cast<char>(0x40);
        address[3] = static_cast<char>(0x00);
        return address;
    }
    if (effect == QuickSettingEffect::Equalizer) {
        address[2] = static_cast<char>(0x40);
        address[3] = static_cast<char>(0x3C);
        return address;
    }
    if (effect == QuickSettingEffect::OverdriveDistortion)
        return address;
    if (effect == QuickSettingEffect::Delay) {
        address[2] = static_cast<char>(0x44);
        address[3] = static_cast<char>(0x04);
        return address;
    }
    if (effect == QuickSettingEffect::Chorus) {
        address[2] = static_cast<char>(0x44);
        address[3] = static_cast<char>(0x10);
        return address;
    }
    if (effect == QuickSettingEffect::Reverb) {
        address[2] = static_cast<char>(0x44);
        address[3] = static_cast<char>(0x1C);
        return address;
    }
    if (effect == QuickSettingEffect::SendReturn) {
        address[2] = static_cast<char>(0x44);
        address[3] = static_cast<char>(0x40);
        return address;
    }
    address[2] = static_cast<char>(0x40);
    address[3] = static_cast<char>(0x24);
    return address;
}

QByteArray QuickSettingCodec::effectNameAddress(
    int slot, QuickSettingEffect effect, int typeRaw, QString *error)
{
    if (effect != QuickSettingEffect::Fx1)
        return effectNameAddress(slot, effect, error);
    if (typeRaw < 0 || typeRaw > 0x21) {
        if (error)
            *error = QStringLiteral("FX Quick Setting TYPE must be 00-21");
        return QByteArray();
    }
    QByteArray address = slotBaseAddress(slot, error);
    if (address.isEmpty())
        return address;
    address[2] = static_cast<char>(0x40);
    address[3] = static_cast<char>(0x48);
    address = addRolandAddress(address, typeRaw * NameSize);
    if (address.isEmpty() && error)
        *error = QStringLiteral("FX Quick Setting NAME address overflow");
    return address;
}

bool QuickSettingCodec::hasPresentationName(QuickSettingEffect effect)
{
    return effect == QuickSettingEffect::OverdriveDistortion
        || effect == QuickSettingEffect::Delay
        || effect == QuickSettingEffect::Chorus
        || effect == QuickSettingEffect::Reverb
        || effect == QuickSettingEffect::Compressor
        || effect == QuickSettingEffect::Equalizer
        || effect == QuickSettingEffect::SendReturn;
}

QByteArray QuickSettingCodec::presentationFallbackAddress(
    int slot, QuickSettingEffect effect, QString *error)
{
    if (effect == QuickSettingEffect::Equalizer
        || effect == QuickSettingEffect::Fx1) {
        if (error)
            *error = QStringLiteral(
                "Quick Setting presentation fallback is not available");
        return QByteArray();
    }
    QByteArray address = effectAddress(slot, effect, error);
    if (address.isEmpty())
        return address;
    if (hasPresentationName(effect))
        address = addRolandAddress(address, 1);
    return address;
}

QByteArray QuickSettingCodec::temporaryEffectAddress(
    QuickSettingEffect effect)
{
    QByteArray address(4, 0);
    address[0] = static_cast<char>(0x60);
    if (effect == QuickSettingEffect::Compressor) {
        address[2] = static_cast<char>(0x00);
        address[3] = static_cast<char>(0x40);
    } else if (effect == QuickSettingEffect::Equalizer) {
        address[2] = static_cast<char>(0x01);
        address[3] = static_cast<char>(0x70);
    } else if (effect == QuickSettingEffect::OverdriveDistortion) {
        address[2] = static_cast<char>(0x00);
        address[3] = static_cast<char>(0x70);
    } else if (effect == QuickSettingEffect::Delay) {
        address[2] = static_cast<char>(0x0A);
        address[3] = static_cast<char>(0x00);
    } else if (effect == QuickSettingEffect::Chorus) {
        address[2] = static_cast<char>(0x0A);
        address[3] = static_cast<char>(0x20);
    } else if (effect == QuickSettingEffect::Reverb) {
        address[2] = static_cast<char>(0x0A);
        address[3] = static_cast<char>(0x30);
    } else if (effect == QuickSettingEffect::Fx1) {
        address[2] = static_cast<char>(0x02);
        address[3] = static_cast<char>(0x00);
    } else if (effect == QuickSettingEffect::SendReturn) {
        address[2] = static_cast<char>(0x0A);
        address[3] = static_cast<char>(0x79);
    } else {
        address[2] = static_cast<char>(0x01);
        address[3] = static_cast<char>(
            effect == QuickSettingEffect::PreampA ? 0x10 : 0x30);
    }
    return address;
}

QuickSettingTransferPlan QuickSettingCodec::transferPlan(
    int slot, QuickSettingEffect effect, bool temporary, QString *error)
{
    QuickSettingTransferPlan plan;
    if (effect != QuickSettingEffect::Fx1) {
        if (error)
            *error = QStringLiteral("Segmented transfer is not defined for this effect");
        return plan;
    }
    QByteArray base = temporary
        ? temporaryEffectAddress(effect) : slotBaseAddress(slot, error);
    if (base.isEmpty())
        return plan;
    const int sizes[] = {128, 128, 128, 86};
    for (int index = 0; index < 4; ++index) {
        QByteArray address(base);
        address[2] = static_cast<char>(0x02 + index);
        address[3] = static_cast<char>(0x00);
        plan.segments.append({address, sizes[index]});
        plan.totalLogicalSize += sizes[index];
    }
    return plan;
}

QByteArray QuickSettingCodec::joinSegments(
    const QVector<QByteArray> &segments,
    const QuickSettingTransferPlan &plan, QString *error)
{
    if (!plan.isValid() || segments.size() != plan.segments.size()) {
        if (error)
            *error = QStringLiteral("Quick Setting segmented payload is incomplete");
        return QByteArray();
    }
    QByteArray payload;
    for (int index = 0; index < segments.size(); ++index) {
        if (segments.at(index).size() != plan.segments.at(index).size) {
            if (error)
                *error = QStringLiteral("Quick Setting segment has an unexpected size");
            return QByteArray();
        }
        payload.append(segments.at(index));
    }
    if (payload.size() != plan.totalLogicalSize) {
        if (error)
            *error = QStringLiteral("Quick Setting logical payload has an unexpected size");
        return QByteArray();
    }
    return payload;
}

QVector<QByteArray> QuickSettingCodec::splitPayload(
    const QByteArray &payload, const QuickSettingTransferPlan &plan,
    QString *error)
{
    QVector<QByteArray> segments;
    if (!plan.isValid() || payload.size() != plan.totalLogicalSize) {
        if (error)
            *error = QStringLiteral("Quick Setting logical payload has an unexpected size");
        return segments;
    }
    int offset = 0;
    for (const QuickSettingSegment &segment : plan.segments) {
        segments.append(payload.mid(offset, segment.size));
        offset += segment.size;
    }
    return segments;
}

int QuickSettingCodec::effectPayloadSize(QuickSettingEffect effect)
{
    if (effect == QuickSettingEffect::OverdriveDistortion)
        return OddsPayloadSize;
    if (effect == QuickSettingEffect::Delay)
        return DelayPayloadSize;
    if (effect == QuickSettingEffect::Chorus)
        return ChorusPayloadSize;
    if (effect == QuickSettingEffect::Reverb)
        return ReverbPayloadSize;
    if (effect == QuickSettingEffect::Compressor)
        return CompressorPayloadSize;
    if (effect == QuickSettingEffect::Equalizer)
        return EqualizerPayloadSize;
    if (effect == QuickSettingEffect::Fx1)
        return Fx1PayloadSize;
    if (effect == QuickSettingEffect::SendReturn)
        return SendReturnPayloadSize;
    return PreampPayloadSize;
}

int QuickSettingCodec::effectTypeRaw(QuickSettingEffect effect,
                                     const QByteArray &payload,
                                     QString *error)
{
    if (effect == QuickSettingEffect::Equalizer) {
        if (error)
            *error = QStringLiteral("EQ Quick Settings do not have a TYPE field");
        return -1;
    }
    const int expectedSize = effectPayloadSize(effect);
    if (payload.size() != expectedSize) {
        if (error)
            *error = QStringLiteral("Quick Setting effect payload has an unexpected size");
        return -1;
    }
    const int typeOffset = (effect == QuickSettingEffect::Compressor
                            || effect == QuickSettingEffect::OverdriveDistortion
                            || effect == QuickSettingEffect::Delay
                            || effect == QuickSettingEffect::Chorus
                            || effect == QuickSettingEffect::Reverb
                            || effect == QuickSettingEffect::Fx1
                            || effect == QuickSettingEffect::SendReturn) ? 1 : 0;
    return static_cast<quint8>(payload.at(typeOffset));
}

QString QuickSettingCodec::buildReadRequest(const QByteArray &address,
                                             int size, QString *error)
{
    if (size < 1 || size > 0x3FFF) {
        if (error)
            *error = QStringLiteral("Quick Setting RQ1 size is invalid");
        return QString();
    }
    QByteArray encodedSize(4, 0);
    int remaining = size;
    for (int index = 3; index >= 0; --index) {
        encodedSize[index] = static_cast<char>(remaining & 0x7F);
        remaining >>= 7;
    }
    return encodedMessage(0x11, address, encodedSize, error);
}

QString QuickSettingCodec::buildWriteMessage(const QByteArray &address,
                                              const QByteArray &payload,
                                              QString *error)
{
    if (payload.isEmpty()) {
        if (error)
            *error = QStringLiteral("Quick Setting DT1 payload is empty");
        return QString();
    }
    for (char byte : payload) {
        if (static_cast<quint8>(byte) > 0x7F) {
            if (error)
                *error = QStringLiteral("Quick Setting payload is not 7-bit MIDI data");
            return QString();
        }
    }
    return encodedMessage(0x12, address, payload, error);
}

QuickSettingReply QuickSettingCodec::decodeReply(
    const QString &reply, const QByteArray &expectedAddress, int expectedSize)
{
    QuickSettingReply decoded;
    bool ok = false;
    const QByteArray bytes = normalizedBytes(reply, &ok);
    if (!ok || bytes.isEmpty()) {
        decoded.error = QStringLiteral("No valid Quick Setting reply was received");
        return decoded;
    }
    if (bytes.size() != 13 + expectedSize) {
        decoded.error = QStringLiteral("Quick Setting reply has an unexpected size");
        return decoded;
    }
    const QByteArray header = QByteArray::fromHex("F0410000002F12");
    if (!bytes.startsWith(header) || static_cast<quint8>(bytes.back()) != 0xF7) {
        decoded.error = QStringLiteral("Quick Setting reply has an invalid GT-10 DT1 header");
        return decoded;
    }
    decoded.address = bytes.mid(7, 4);
    if (decoded.address != expectedAddress) {
        decoded.error = QStringLiteral("Quick Setting reply address does not match the request");
        return decoded;
    }
    if (checksum(bytes, 7, bytes.size() - 2)
        != static_cast<quint8>(bytes.at(bytes.size() - 2))) {
        decoded.error = QStringLiteral("Quick Setting reply checksum is invalid");
        return decoded;
    }
    decoded.payload = bytes.mid(11, expectedSize);
    for (char byte : decoded.payload) {
        if (static_cast<quint8>(byte) > 0x7F) {
            decoded.payload.clear();
            decoded.error = QStringLiteral(
                "Quick Setting reply contains non 7-bit MIDI data");
            return decoded;
        }
    }
    decoded.valid = true;
    return decoded;
}

int QuickSettingCodec::preampTypeRaw(const QByteArray &payload, QString *error)
{
    return effectTypeRaw(QuickSettingEffect::PreampA, payload, error);
}

QByteArray QuickSettingCodec::encodeName(const QString &name, QString *error)
{
    if (name.size() > NameSize) {
        if (error)
            *error = QStringLiteral("Quick Setting name cannot exceed 12 characters");
        return QByteArray();
    }
    QByteArray encoded;
    for (QChar character : name) {
        const ushort value = character.unicode();
        if (value < 0x20 || value > 0x7D) {
            if (error)
                *error = QStringLiteral("Quick Setting name contains an unsupported character");
            return QByteArray();
        }
        encoded.append(static_cast<char>(value));
    }
    encoded.append(NameSize - encoded.size(), static_cast<char>(0x20));
    return encoded;
}

QString QuickSettingCodec::decodeName(const QByteArray &encoded,
                                      QString *error)
{
    if (encoded.size() != NameSize) {
        if (error)
            *error = QStringLiteral("Quick Setting name payload must have 12 bytes");
        return QString();
    }
    for (char byte : encoded) {
        const quint8 value = static_cast<quint8>(byte);
        if (value < 0x20 || value > 0x7D) {
            if (error)
                *error = QStringLiteral("Quick Setting name payload is invalid");
            return QString();
        }
    }
    return QString::fromLatin1(encoded).trimmed();
}

bool QuickSettingCodec::payloadMatches(const QByteArray &expected,
                                       const QByteArray &actual)
{
    return expected == actual;
}
